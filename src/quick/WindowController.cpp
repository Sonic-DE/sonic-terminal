/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "WindowController.h"

#include "config-konsole.h"

#include "CommandBarModel.h"
#include "CompatWindows.h"
#include "MainWindow.h"
#include "QmlDialogs.h"
#include "TerminalView.h"
#include "TerminalViewRegistry.h"
#include "ViewManager.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"
#if HAVE_DBUS
#include "MainWindowInterface.h"
#endif

#include <Sonic/Logging/Log>

#include <KAcceleratorManager>
#include <KActionCollection>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KWindowEffects>
#include <KXMLGUIClient>
#include <KXMLGUIFactory>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QMenu>
#include <QMenuBar>
#include <QSet>
#include <QShortcut>

#include <QEvent>
#include <QHash>
#include <QPlatformSurfaceEvent>
#include <QPointer>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTimer>
#include <QWidget>

namespace Konsole {
namespace {
    QHash<const QObject*, WindowController*>& controllers()
    {
        static QHash<const QObject*, WindowController*> hash;
        return hash;
    }

    // What QApplication's active window gets, for compat windows, whose QML window is the one
    // on screen and active:
    // - dialogs, message boxes and menus parented to a compat window (or a widget in it) are
    //   transient for its QML window. Qt set the compat window as transient parent when it
    //   created theirs, before Show; X11 reads it when they map;
    // - KXmlGui's automatic accelerators (KCheckAccelerators, on unless [Development]
    //   AutoCheckAccelerators=false): 20 ms after widgets change, the active window is managed.
    class CompatWindowsFilter : public QObject {
    public:
        explicit CompatWindowsFilter(QObject* parent)
            : QObject(parent)
            , _autoAccelerators(KConfigGroup(KSharedConfig::openConfig(), QStringLiteral("Development")).readEntry("AutoCheckAccelerators", true))
        {
            _acceleratorTimer.setSingleShot(true);
            _acceleratorTimer.setInterval(20);
            connect(&_acceleratorTimer, &QTimer::timeout, this, [] {
                // Active also while one of its menus (transient for it) has the focus.
                for (auto it = controllers().cbegin(); it != controllers().cend(); ++it) {
                    if (it.value()->shownWindow() && it.value()->shownWindow()->isActive()) {
                        KAcceleratorManager::manage(qobject_cast<QWidget*>(it.value()->compatWindow()));
                    }
                }
            });
        }

        bool eventFilter(QObject* watched, QEvent* event) override
        {
            switch (event->type()) {
            case QEvent::Show: {
                auto* widget = qobject_cast<QWidget*>(watched);
                QWidget* parent = widget && widget->isWindow() ? widget->parentWidget() : nullptr;
                WindowController* controller = parent ? WindowController::controllerOf(parent->window()) : nullptr;
                if (controller && controller->shownWindow() && widget->windowHandle()) {
                    widget->windowHandle()->setTransientParent(controller->shownWindow());
                    sonicDebug() << "transient parent for" << widget->metaObject()->className();
                    if (qobject_cast<QDialog*>(widget) && !widget->testAttribute(Qt::WA_Moved)) {
                        placeDialog(widget, parent->window());
                    }
                    // QMenu's own Show handling sets its native parent's window again.
                    if (qobject_cast<QMenu*>(widget)) {
                        QMetaObject::invokeMethod(
                            widget,
                            [widget, shown = QPointer<QWindow>(controller->shownWindow())] {
                                if (shown && widget->windowHandle()) {
                                    widget->windowHandle()->setTransientParent(shown);
                                }
                            },
                            Qt::QueuedConnection);
                    }
                } else if (widget && widget->isWindow() && !parent && qobject_cast<QDialog*>(widget) && !widget->testAttribute(Qt::WA_Moved)
                    && widget->windowHandle()) {
                    // A dialog without a parent widget: one parented to QApplication::activeWindow()
                    // (KBookmarkMenu), which is null while a QML window is the active one, or a
                    // platform file dialog transient for a compat window's native window. As over a
                    // widget window, it belongs to the QML window.
                    const QWindow* transient = widget->windowHandle()->transientParent();
                    for (auto it = controllers().cbegin(); it != controllers().cend(); ++it) {
                        auto* compat = qobject_cast<QWidget*>(it.value()->compatWindow());
                        QWindow* shown = it.value()->shownWindow();
                        const bool ownWindow = transient && compat && transient == compat->windowHandle();
                        const bool activeWindow = !transient && !QApplication::activeWindow() && shown && shown->isActive();
                        if (shown && compat && (ownWindow || activeWindow)) {
                            widget->windowHandle()->setTransientParent(shown);
                            placeDialog(widget, compat);
                            break;
                        }
                    }
                }
                break;
            }
            case QEvent::ChildAdded:
            case QEvent::ChildRemoved:
                if (!static_cast<QChildEvent*>(event)->child()->isWidgetType()) {
                    break;
                }
                Q_FALLTHROUGH();
            case QEvent::Resize:
            case QEvent::LayoutRequest:
            case QEvent::WindowActivate:
            case QEvent::WindowDeactivate:
                if (_autoAccelerators) {
                    _acceleratorTimer.start();
                }
                break;
            default:
                break;
            }
            return false;
        }

    private:
        // QDialog::adjustPosition()'s estimate of the window frame from the visible top-levels,
        // taking a compat window's from its QML window when shown is set.
        static QPoint frameEstimate(bool shown)
        {
            int extraw = 0;
            int extrah = 0;
            const QWidgetList list = QApplication::topLevelWidgets();
            for (int i = 0; (extraw == 0 || extrah == 0) && i < list.size(); ++i) {
                QWidget* current = list.at(i);
                if (!current->isVisible()) {
                    continue;
                }
                WindowController* controller = shown ? WindowController::controllerOf(current) : nullptr;
                const QMargins frame = controller && controller->shownWindow()
                    ? controller->shownWindow()->frameMargins()
                    : QMargins(current->geometry().x() - current->x(), current->geometry().y() - current->y(), 0, 0);
                extraw = qMax(extraw, frame.left());
                extrah = qMax(extrah, frame.top());
            }
            if (extraw == 0 || extrah == 0 || extraw >= 10 || extrah >= 40) {
                return QPoint(10, 40);
            }
            return QPoint(extraw, extrah);
        }

        // QDialog::adjustPosition() over a compat window, with the frame of its QML window on
        // screen, as over a widget window. The dialog is not mapped yet.
        static void placeDialog(QWidget* dialog, const QWidget* window)
        {
            const QPoint extra = frameEstimate(true);
            const QRect desk = window->screen() ? window->screen()->availableGeometry() : QRect();
            const QPoint centre = window->mapToGlobal(QPoint(window->width() / 2, window->height() / 2));
            QPoint p(centre.x() - dialog->width() / 2 - extra.x(), centre.y() - dialog->height() / 2 - extra.y());
            if (p.x() + extra.x() + dialog->width() > desk.x() + desk.width()) {
                p.setX(desk.x() + desk.width() - dialog->width() - extra.x());
            }
            p.setX(qMax(p.x(), desk.x()));
            if (p.y() + extra.y() + dialog->height() > desk.y() + desk.height()) {
                p.setY(desk.y() + desk.height() - dialog->height() - extra.y());
            }
            p.setY(qMax(p.y(), desk.y()));
            // Moved, so QDialog::showEvent() (after this filter) keeps it; then, as QDialog leaves
            // it, not a position the program chose.
            dialog->move(p);
            QMetaObject::invokeMethod(
                dialog,
                [dialog] {
                    dialog->setAttribute(Qt::WA_Moved, false);
                },
                Qt::QueuedConnection);
        }

        const bool _autoAccelerators;
        QTimer _acceleratorTimer;
    };
}

WindowController::WindowController(MainWindow* window)
    : QObject(window)
    , _window(window)
    , _translucent(window->testAttribute(Qt::WA_TranslucentBackground))
{
    // KMainWindow's constructor placed only the native window at the saved position
    // (kmainwindow.cpp applyMainWindowSettings(), QTBUG-40584); the widget takes it, so the
    // QML window does on its first show.
    if (QWindow* handle = window->windowHandle(); handle && handle->position() != window->geometry().topLeft()) {
        window->setGeometry(QRect(handle->position(), window->size()));
        sonicDebug() << "saved position" << handle->position();
    }
    setUpCompatWindow();
    createQmlWindow();
    CompatWindows::setShownWindow(window, _qmlWindow);
}

WindowController::WindowController(MainWindow* window, QQuickItem* item, bool fullTerminal)
    : QObject(window)
    , _window(window)
    , _item(item)
    , _fullTerminal(fullTerminal)
{
    setUpCompatWindow();
    connect(item, &QQuickItem::xChanged, this, &WindowController::followItem);
    connect(item, &QQuickItem::yChanged, this, &WindowController::followItem);
    connect(item, &QQuickItem::widthChanged, this, &WindowController::followItem);
    connect(item, &QQuickItem::heightChanged, this, &WindowController::followItem);
    // Each frame too: the window holding the item appears later, and the item's parents move
    // without telling it.
    const auto followFrames = [this](QQuickWindow* itemWindow) {
        if (itemWindow) {
            connect(itemWindow, &QQuickWindow::afterAnimating, this, &WindowController::followItem, Qt::UniqueConnection);
        }
    };
    connect(item, &QQuickItem::windowChanged, this, [this, followFrames](QQuickWindow* itemWindow) {
        followFrames(itemWindow);
        followItem();
    });
    followFrames(item->window());
    connect(item, &QQuickItem::activeFocusChanged, this, &WindowController::scheduleShortcuts);
    watchActions();
    if (fullTerminal) {
        rewireFindAction();
    }
    followItem();
}

void WindowController::setUpCompatWindow()
{
    static auto* compatWindowsFilter = [] {
        auto* filter = new CompatWindowsFilter(qApp);
        qApp->installEventFilter(filter);
        return filter;
    }();
    Q_UNUSED(compatWindowsFilter)
    // Laid out and isVisible(), but never mapped or painted.
    _window->setAttribute(Qt::WA_DontShowOnScreen);
    _window->setUpdatesEnabled(false);
    CompatWindows::add(_window);
    controllers().insert(_window, this);
    _window->installEventFilter(this);
}

WindowController::~WindowController()
{
    CompatWindows::remove(_window);
    controllers().remove(_window);
    delete _qmlWindow;
}

WindowController* WindowController::controllerOf(const QObject* compatWindow)
{
    return controllers().value(compatWindow);
}

QObject* WindowController::compatWindow() const
{
    return _window;
}

QQuickWindow* WindowController::qmlWindow() const
{
    return _qmlWindow;
}

QWindow* WindowController::shownWindow() const
{
    if (_qmlWindow || !_item) {
        return _qmlWindow;
    }
    QQuickWindow* window = _item->window();
    QWindow* renderWindow = window ? QQuickRenderControl::renderWindowFor(window) : nullptr;
    return renderWindow ? renderWindow : window;
}

bool WindowController::translucent() const
{
    return _translucent;
}

void WindowController::setBlur(bool blur)
{
    if (blur == _blur) {
        return;
    }
    _blur = blur;
    Q_EMIT blurChanged();
    if (!_qmlWindow) {
        return;
    }
    if (blur) {
        // The platform theme's tools-area blur mustn't replace the full-window one.
        _qmlWindow->setProperty("_silver_skip_tools_blur", true);
        applyBlur();
    } else {
        if (_qmlWindow->handle()) {
            KWindowEffects::enableBlurBehind(_qmlWindow, false);
        }
        _qmlWindow->setProperty("_silver_skip_tools_blur", false);
    }
    sonicDebug() << "QML window blur" << blur;
}

bool WindowController::blur() const
{
    return _blur;
}

void WindowController::applyBlur()
{
    // Only with a platform window; SurfaceCreated brings a new one back here.
    if (_blur && _qmlWindow && _qmlWindow->handle()) {
        KWindowEffects::enableBlurBehind(_qmlWindow, true);
    }
}

void WindowController::setFrameless(bool frameless)
{
    if (_qmlWindow) {
        _qmlWindow->setFlag(Qt::FramelessWindowHint, frameless);
    }
}

void WindowController::activateMenuBar()
{
    Q_EMIT menuBarActivated();
}

void WindowController::createQmlWindow()
{
    QQmlComponent component(QmlDialogs::engine(), QStringLiteral("org.sonicde.terminal.app"), QStringLiteral("Main"));
    QObject* object = component.createWithInitialProperties({{QStringLiteral("controller"), QVariant::fromValue(this)}});
    _qmlWindow = qobject_cast<QQuickWindow*>(object);
    if (!_qmlWindow) {
        sonicCritical() << "the QML window didn't load:" << component.errorString();
        delete object;
        return;
    }
    if (_translucent) {
        // Before the platform window exists; the platform theme only does this for every
        // Quick window when [ToolsArea] is enabled.
        QSurfaceFormat format = _qmlWindow->format();
        format.setAlphaBufferSize(8);
        _qmlWindow->setFormat(format);
    }
    _qmlWindow->installEventFilter(this);
    // The window manager sees the compat window's identity (role, title, icon).
    _qmlWindow->setProperty("_q_xcb_wm_window_role", _window->objectName());
    _qmlWindow->setTitle(_window->windowTitle());
    _qmlWindow->setIcon(_window->windowIcon());
    connect(_qmlWindow, &QWindow::widthChanged, this, &WindowController::followQmlGeometry);
    connect(_qmlWindow, &QWindow::heightChanged, this, &WindowController::followQmlGeometry);
    connect(_qmlWindow, &QWindow::xChanged, this, &WindowController::followQmlGeometry);
    connect(_qmlWindow, &QWindow::yChanged, this, &WindowController::followQmlGeometry);
    connect(_qmlWindow, &QWindow::windowStateChanged, this, &WindowController::followQmlWindowState);
    // The compat window is never active; its widgets' colours follow this window's.
    connect(_qmlWindow, &QWindow::activeChanged, this, [this] {
        CompatWindows::setActive(_window, _qmlWindow->isActive());
    });
    sonicDebug() << "window pair" << _window->objectName() << "QML window" << _qmlWindow;
#if HAVE_DBUS
    new MainWindowInterface(_window, _qmlWindow);
    sonicDebug() << "org.kde.KMainWindow at" << _window->dbusName() << "answered for the QML window";
#endif

    watchActions();
    rewireFindAction();
}

void WindowController::rewireFindAction()
{
    // Find Action: KXmlGuiWindow's KCommandBar would open in the hidden compat window.
    if (QAction* findAction = _window->actionCollection()->action(QStringLiteral("open_kcommand_bar"))) {
        disconnect(findAction, &QAction::triggered, _window, nullptr);
        connect(findAction, &QAction::triggered, this, [this] {
            if (_window && _window->isCommandBarEnabled()) {
                openCommandBar(CommandBarModel::windowGroups(_window->guiFactory()));
            }
        });
    }
}

void WindowController::watchActions()
{
    // Plugging a session's client (tab switches), plugins and Configure Shortcuts change
    // the actions.
    if (KXMLGUIFactory* factory = _window->guiFactory()) {
        connect(factory, &KXMLGUIFactory::clientAdded, this, &WindowController::scheduleShortcuts);
        connect(factory, &KXMLGUIFactory::clientRemoved, this, &WindowController::scheduleShortcuts);
        connect(factory, &KXMLGUIFactory::shortcutsSaved, this, &WindowController::scheduleShortcuts);
    }
    scheduleShortcuts();
}

void WindowController::followItem()
{
    if (!_window || !_item) {
        return;
    }
    QWindow* shown = shownWindow();
    if (shown != _itemWindow) {
        if (_itemWindow) {
            _itemWindow->removeEventFilter(this);
            _itemWindow->disconnect(this);
        }
        _itemWindow = shown;
        CompatWindows::setShownWindow(_window, shown);
        if (shown) {
            shown->installEventFilter(this);
            connect(shown, &QWindow::activeChanged, this, [this, shown] {
                CompatWindows::setActive(_window, shown->isActive());
            });
            CompatWindows::setActive(_window, shown->isActive());
        }
        scheduleShortcuts();
    }
    const QRect geometry(_item->mapToGlobal(QPointF(0, 0)).toPoint(), _item->size().toSize());
    if (!geometry.isEmpty() && geometry != _window->geometry()) {
        _syncing = true;
        setCompatGeometry(geometry);
        _syncing = false;
    }
}

void WindowController::scheduleShortcuts()
{
    if (_shortcutsScheduled) {
        return;
    }
    _shortcutsScheduled = true;
    QMetaObject::invokeMethod(
        this,
        [this] {
            _shortcutsScheduled = false;
            rebuildShortcuts();
        },
        Qt::QueuedConnection);
}

void WindowController::rebuildShortcuts()
{
    for (const QPointer<QShortcut>& shortcut : std::as_const(_shortcuts)) {
        delete shortcut.data();
    }
    _shortcuts.clear();
    KXMLGUIFactory* factory = _window ? _window->guiFactory() : nullptr;
    // On a host's top-level widget when it has one, whose context Qt checks as a widget's.
    QObject* owner = _qmlWindow;
    if (QWindow* shown = _item ? shownWindow() : nullptr) {
        owner = shown;
        const QWidgetList widgets = QApplication::topLevelWidgets();
        for (QWidget* widget : widgets) {
            if (widget->windowHandle() == shown) {
                owner = widget;
                break;
            }
        }
    }
    if (!owner || !factory) {
        return;
    }
    QSet<QAction*> bound;
    const auto bind = [this, &bound, owner](const QList<QAction*>& actions) {
        for (QAction* action : actions) {
            // Enabled, visible and key changes all come with changed().
            connect(action, &QAction::changed, this, &WindowController::scheduleShortcuts, Qt::UniqueConnection);
            if (action->shortcuts().isEmpty() || bound.contains(action)) {
                continue;
            }
            bound.insert(action);
            auto* shortcut = new QShortcut(owner);
            shortcut->setKeys(action->shortcuts());
            shortcut->setAutoRepeat(action->autoRepeat());
            shortcut->setEnabled(action->isEnabled() && action->isVisible() && (!_item || _item->hasActiveFocus()));
            connect(shortcut, &QShortcut::activated, action, &QAction::trigger);
            _shortcuts.append(shortcut);
        }
    };
    SessionController* activeController = _window->viewManager() ? _window->viewManager()->activeViewController() : nullptr;
    const QList<KXMLGUIClient*> clients = factory->clients();
    for (KXMLGUIClient* client : clients) {
        KActionCollection* collection = client->actionCollection();
        // In a host, as with the part: only the session's. The window's would answer for the
        // host's own keys.
        if (!collection || (_item && !_fullTerminal && client != activeController)) {
            continue;
        }
        connect(collection, &KActionCollection::inserted, this, &WindowController::scheduleShortcuts, Qt::UniqueConnection);
        connect(collection, &KActionCollection::changed, this, &WindowController::scheduleShortcuts, Qt::UniqueConnection);
        bind(collection->actions());
    }
    // What else fires in the widget window: actions on the active view (the plugins' quick
    // access), and the Plugins menu's while the menu bar shows.
    if (activeController && activeController->view()) {
        bind(activeController->view()->actions());
    }
    if (_window->menuBar()->isVisibleTo(_window)) {
        if (auto* plugins = qobject_cast<QMenu*>(factory->container(QStringLiteral("plugins"), _window))) {
            bind(plugins->actions());
        }
    }
    sonicDebug() << "shortcuts bound:" << _shortcuts.size();
}

void WindowController::followQmlGeometry()
{
    if (_syncing || !_window || !_qmlWindow) {
        return;
    }
    // The user moved or resized the QML window: the compat window lays out at that size.
    _syncing = true;
    setCompatGeometry(_qmlWindow->geometry());
    _syncing = false;
}

void WindowController::followCompatGeometry()
{
    // The window manager places the window unless the program positioned it.
    if (_window->testAttribute(Qt::WA_Moved)) {
        _qmlWindow->setGeometry(_window->geometry());
    } else {
        _qmlWindow->resize(_window->size());
    }
}

void WindowController::setCompatGeometry(const QRect& geometry)
{
    // Following the QML window isn't the program positioning it, and keeps its state
    // (setGeometry() drops maximized and full screen).
    const bool moved = _window->testAttribute(Qt::WA_Moved);
    const Qt::WindowStates states = _window->windowState();
    _window->setGeometry(geometry);
    _window->setAttribute(Qt::WA_Moved, moved);
    if (_window->windowState() != states) {
        _window->setWindowState(states);
    }
    // An off-screen window's native geometry doesn't follow setGeometry(); moving it keeps
    // mapToGlobal()/mapFromGlobal() of its widgets true (menus, KToolBar, SearchTabs).
    if (QWindow* handle = _window->windowHandle(); handle && handle->geometry() != geometry) {
        handle->setGeometry(geometry);
    }
}

void WindowController::followQmlWindowState()
{
    if (_syncing || !_window || !_qmlWindow) {
        return;
    }
    // Maximized, full screen: saved with the compat window as before. Never minimized: Qt
    // hides a minimized widget (hiding this window) and maps it when restoring it.
    _syncing = true;
    _window->setWindowState(_qmlWindow->windowStates() & ~Qt::WindowMinimized);
    _syncing = false;
}

void WindowController::requestClose()
{
    if (_window) {
        _window->close();
    }
}

void WindowController::openCommandBar(const QList<KCommandBar::ActionGroup>& groups)
{
    commandBarClosed();
    auto* model = new CommandBarModel(this);
    model->setActions(groups);
    _commandBar = model;
    sonicDebug() << "command bar opened," << model->rowCount() << "rows";
    Q_EMIT commandBarRequested(model);
}

void WindowController::commandBarClosed()
{
    if (_commandBar) {
        _commandBar->deleteLater();
    }
}

void WindowController::connectFocusBridge()
{
    QWindow* window = _window ? _window->windowHandle() : nullptr;
    if (window == _bridgedWindow) {
        return;
    }
    if (_bridgedWindow) {
        _bridgedWindow->disconnect(this);
    }
    _bridgedWindow = window;
    if (!window) {
        return;
    }
    // The compat window is never active, so setFocus() on a facade takes the inactive
    // path, which still emits focusObjectChanged.
    connect(window, &QWindow::focusObjectChanged, this, &WindowController::bridgeFocus);
}

void WindowController::bridgeFocus(QObject* object)
{
    auto* display = qobject_cast<TerminalDisplay*>(object);
    TerminalView* view = display ? TerminalViewRegistry::view(display) : nullptr;
    // A hidden facade (another tab's) gets no focus, as a hidden widget doesn't.
    if (!view || !view->sceneMode() || (!_qmlWindow && !_item) || !display->isEnabled() || !display->isVisible()) {
        return;
    }
    sonicDebug() << "focus bridge: session view" << display->id();
    // A host decides which of its windows is active.
    if (_qmlWindow) {
        _qmlWindow->requestActivate();
    }
    // A new tab's view isn't in the scene yet; another view's focus since replaces it.
    if (_pendingFocus && _pendingFocus != display) {
        if (TerminalView* pending = TerminalViewRegistry::view(_pendingFocus)) {
            pending->dropFacadeFocus();
        }
    }
    _pendingFocus = view->window() ? nullptr : display;
    view->takeFacadeFocus();
}

bool WindowController::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _qmlWindow && event->type() == QEvent::PlatformSurface
        && static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
        // A new native window starts without blur.
        QMetaObject::invokeMethod(this, &WindowController::applyBlur, Qt::QueuedConnection);
    }
    if (_item) {
        const QEvent::Type type = event->type();
        if ((watched == _itemWindow || watched == _window) && (type == QEvent::Move || type == QEvent::Resize)) {
            // The host's window moved, or the program sized the compat window (first show).
            if (!_syncing) {
                followItem();
            }
        } else if (watched == _window && (type == QEvent::Show || type == QEvent::WinIdChange)) {
            connectFocusBridge();
            if (type == QEvent::Show) {
                QMetaObject::invokeMethod(
                    this,
                    [this] {
                        if (_window) {
                            followItem();
                            bridgeFocus(_window->focusWidget());
                        }
                    },
                    Qt::QueuedConnection);
            }
        }
        return QObject::eventFilter(watched, event);
    }
    if (watched != _window || !_qmlWindow) {
        return QObject::eventFilter(watched, event);
    }
    switch (event->type()) {
    case QEvent::Show:
        connectFocusBridge();
        // After MainWindow::showEvent(), which sizes the window on its first show, so the
        // window manager places it at its size. That resize drops the requested state (a
        // widget window gets it back from the window manager), so it is taken now.
        QMetaObject::invokeMethod(
            this,
            [this, states = _window->windowState()] {
                if (!_window || !_window->isVisible() || _qmlWindow->isVisible()) {
                    return;
                }
                _syncing = true;
                followCompatGeometry();
                _qmlWindow->setWindowStates(states);
                if (_window->windowState() != states) {
                    _window->setWindowState(states);
                }
                setCompatGeometry(_window->geometry());
                _syncing = false;
                // Not show(), which is showNormal() for a top-level window.
                _qmlWindow->setVisible(true);
                // The terminal focused before the bridge was connected (the first one).
                bridgeFocus(_window->focusWidget());
            },
            Qt::QueuedConnection);
        break;
    case QEvent::Hide:
        _qmlWindow->hide();
        break;
    case QEvent::Move:
    case QEvent::Resize:
        // The program sized or moved the compat window (first show, detach, restore).
        if (!_syncing && _qmlWindow->geometry() != _window->geometry()) {
            _syncing = true;
            followCompatGeometry();
            setCompatGeometry(_window->geometry());
            _syncing = false;
        }
        break;
    case QEvent::WindowTitleChange:
        _qmlWindow->setTitle(_window->windowTitle());
        break;
    case QEvent::WindowIconChange:
        _qmlWindow->setIcon(_window->windowIcon());
        break;
    case QEvent::WindowStateChange:
        // The program changed it (full screen action, restore).
        if (!_syncing && _qmlWindow->windowStates() != _window->windowState()) {
            _syncing = true;
            _qmlWindow->setWindowStates(_window->windowState());
            _syncing = false;
        }
        break;
    case QEvent::WinIdChange:
        connectFocusBridge();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}
}

#include "moc_WindowController.cpp"
