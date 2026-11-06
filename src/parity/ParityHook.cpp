// SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ParityHook.h"

#include "MainWindow.h"
#include "ViewManager.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "terminalDisplay/TerminalFonts.h"

#include <KAcceleratorManager>
#include <KActionCollection>
#include <KLocalizedString>
#include <KPageDialog>
#include <KPageView>
#include <KXMLGUIFactory>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QStyleHints>
#include <QTest>
#include <QTimer>
#include <QWindow>

#include <Sonic/Logging/Log>

#ifdef KONSOLE_PARITY_QML
#include "ParityHookQml.h"
#else
namespace Konsole::ParityHook {
static QWindow* qmlWindow(const MainWindow*)
{
    return nullptr;
}
static bool popupQmlMenu(const MainWindow*, const QMenu*)
{
    return false;
}
static QPixmap grabQmlWindow(const MainWindow*)
{
    return QPixmap();
}
static QList<QWindow*> qmlDialogs()
{
    return {};
}
static QWindow* qmlDialog(const QString&)
{
    return nullptr;
}
static QObject* qmlDialogButton(QWindow*, const QString&, const QString&, int)
{
    return nullptr;
}
static QPixmap grabQmlDialog(QWindow*)
{
    return QPixmap();
}
static void rejectQmlDialog(QWindow*)
{
}
static QStringList qmlDialogPages(QWindow*)
{
    return {};
}
static bool showQmlDialogPage(QWindow*, const QString&)
{
    return false;
}
static bool selectQmlRow(QWindow*, const QString&, int, const QString&)
{
    return false;
}
static bool setQmlText(QWindow*, const QString&, const QString&)
{
    return false;
}
static bool clickQmlItem(QWindow*, const QString&, int, int, const QString&)
{
    return false;
}
static bool allowedWidget(const QWidget*)
{
    return true;
}
static QWidget* dockContent(const QDockWidget*)
{
    return nullptr;
}
static QObject* qmlPanel(const MainWindow*, const QString&)
{
    return nullptr;
}
static QObject* qmlPanelButton(QObject*, const QString&)
{
    return nullptr;
}
static bool setQmlPanelText(QObject*, const QString&, const QString&)
{
    return false;
}
static bool clickQmlPanelItem(QObject*, const QString&, const QString&, const QString&)
{
    return false;
}
}
#endif

namespace Konsole::ParityHook {
namespace {
    class Backdrop : public QWidget {
    public:
        Backdrop(const QString& fill, const QRect& geometry)
            : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnBottomHint | Qt::WindowDoesNotAcceptFocus)
            , _fill(fill)
        {
            setAttribute(Qt::WA_ShowWithoutActivating);
            setGeometry(geometry);
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            if (_fill != QLatin1String("checker")) {
                painter.fillRect(rect(), QColor(_fill));
                return;
            }
            const int cell = 8;
            for (int y = 0; y < height(); y += cell) {
                for (int x = 0; x < width(); x += cell) {
                    painter.fillRect(x, y, cell, cell, ((x + y) / cell) % 2 ? Qt::white : Qt::black);
                }
            }
        }

    private:
        QString _fill;
    };

    class Runner : public QObject {
    public:
        explicit Runner(const QString& scriptPath)
        {
            QFile file(scriptPath);
            if (!file.open(QIODevice::ReadOnly)) {
                sonicCritical() << "parity: cannot read script" << scriptPath;
                return;
            }
            for (const QByteArray& line : file.readAll().split('\n')) {
                if (!line.trimmed().isEmpty() && !line.trimmed().startsWith('#')) {
                    _steps.append(QJsonDocument::fromJson(line).object());
                }
            }
            sonicInfo() << "parity: loaded" << _steps.size() << "steps from" << scriptPath;
        }

        void next(int delayMs = 0)
        {
            QTimer::singleShot(delayMs, this, [this] {
                run();
            });
        }

    private:
        MainWindow* window(int index = -1)
        {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (auto* mainWindow = qobject_cast<MainWindow*>(widget); mainWindow && mainWindow->isVisible() && !_windows.contains(mainWindow)) {
                    _windows.append(mainWindow);
                }
            }
            _windows.removeAll(nullptr);
            if (index < 0) {
                index = _currentWindow;
            }
            return index < _windows.size() ? _windows.at(index).data() : nullptr;
        }

        SessionController* controller()
        {
            MainWindow* mainWindow = window();
            return mainWindow ? mainWindow->viewManager()->activeViewController() : nullptr;
        }

        TerminalDisplay* view()
        {
            SessionController* active = controller();
            return active ? active->view().data() : nullptr;
        }

        QAction* findAction(const QString& id)
        {
            MainWindow* mainWindow = window();
            if (!mainWindow) {
                return nullptr;
            }
            const auto clients = mainWindow->guiFactory()->clients();
            for (KXMLGUIClient* client : clients) {
                if (QAction* action = client->actionCollection()->action(id)) {
                    return action;
                }
            }
            return nullptr;
        }

        static QString safeName(const QString& name)
        {
            QString result = name;
            for (QChar& character : result) {
                const bool allowed = (character >= u'a' && character <= u'z') || (character >= u'A' && character <= u'Z')
                    || (character >= u'0' && character <= u'9') || character == u'.' || character == u'_' || character == u'-';
                if (!allowed) {
                    character = u'_';
                }
            }
            return result;
        }

        void trace(const QString& line)
        {
            QFile file(QDir(_output).filePath(QStringLiteral("trace.txt")));
            if (!file.open(QIODevice::Append | QIODevice::Text) || file.write(line.toUtf8() + '\n') < 0) {
                fail(QStringLiteral("cannot write trace.txt"));
            }
        }

        bool save(const QPixmap& pixmap, const QString& name)
        {
            const QString file = safeName(name) + QStringLiteral(".png");
            if (!pixmap.toImage().convertToFormat(QImage::Format_ARGB32).save(QDir(_output).filePath(file))) {
                fail(QStringLiteral("cannot save ") + file);
                return false;
            }
            trace(QStringLiteral("  saved ") + file);
            return true;
        }

        // The overlays (scroll bars, find bars, messages, labels) drawn over the terminals,
        // relative to `origin` (a window) or in screen coordinates.
        QList<QRect> overlays(const QWidget* origin)
        {
            window();
            QList<QRect> result;
            for (const QPointer<MainWindow>& mainWindow : std::as_const(_windows)) {
                if (!mainWindow || !mainWindow->isVisible() || (origin && mainWindow != origin)) {
                    continue;
                }
                const auto displays = mainWindow->findChildren<TerminalDisplay*>();
                for (TerminalDisplay* display : displays) {
                    QList<QWidget*> widgets = display->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
                    widgets.removeIf([display](QWidget* widget) {
                        return widget == static_cast<QWidget*>(display->headerBar()) || widget->inherits("QQuickWidget") || !widget->isVisible();
                    });
                    for (QWidget* widget : std::as_const(widgets)) {
                        const QRect rect(widget->mapToGlobal(QPoint(0, 0)), widget->size());
                        result.append(origin ? rect.translated(-origin->mapToGlobal(QPoint(0, 0))) : rect);
                    }
                }
            }
            if (!origin) {
                // Menus and dialogs cover the windows behind them.
                QRegion covered;
                for (QWidget* widget : QApplication::topLevelWidgets()) {
                    if (widget->isVisible() && !qobject_cast<MainWindow*>(widget) && !_backdrops.contains(widget)) {
                        covered += widget->frameGeometry();
                    }
                }
                for (QWindow* dialog : qmlDialogs()) {
                    covered += dialog->frameGeometry();
                }
                QList<QRect> visible;
                for (const QRect& rect : std::as_const(result)) {
                    for (const QRect& part : QRegion(rect) - covered) {
                        visible.append(part);
                    }
                }
                result = visible;
            }
            return result;
        }

        void saveOverlays(const QString& name, const QList<QRect>& rects)
        {
            QStringList lines;
            for (const QRect& rect : rects) {
                lines.append(QStringLiteral("%1 %2 %3 %4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
            }
            QFile file(QDir(_output).filePath(safeName(name) + QStringLiteral(".png.overlays")));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text) || file.write((lines.join(u'\n') + u'\n').toUtf8()) < 0) {
                fail(QStringLiteral("cannot write overlays of ") + name);
            }
        }

        // The terminals' rectangles, relative to `origin` or on screen, ordered top to bottom, left to right; on screen also the menus and dialogs
        // covering them.
        void savePanes(const QString& name, const QWidget* origin)
        {
            window();
            QStringList lines;
            for (const QPointer<MainWindow>& mainWindow : std::as_const(_windows)) {
                if (!mainWindow || !mainWindow->isVisible() || (origin && mainWindow != origin)) {
                    continue;
                }
                QList<QRect> rects;
                const auto displays = mainWindow->findChildren<TerminalDisplay*>();
                for (TerminalDisplay* display : displays) {
                    if (display->isVisible()) {
                        const QPoint topLeft = origin ? display->mapTo(mainWindow, QPoint(0, 0)) : display->mapToGlobal(QPoint(0, 0));
                        rects.append(QRect(topLeft, display->size()));
                    }
                }
                std::sort(rects.begin(), rects.end(), [](const QRect& a, const QRect& b) {
                    return a.y() != b.y() ? a.y() < b.y() : a.x() < b.x();
                });
                for (const QRect& rect : std::as_const(rects)) {
                    lines.append(QStringLiteral("pane %1 %2 %3 %4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
                }
            }
            if (!origin) {
                // The windows' shadows (as below), which can fall on another window's terminals;
                // the ring outside each frame, so its own terminals stay compared.
                for (const QPointer<MainWindow>& mainWindow : std::as_const(_windows)) {
                    if (!mainWindow || !mainWindow->isVisible()) {
                        continue;
                    }
                    QWindow* qml = qmlWindow(mainWindow);
                    const QRect frame = qml ? qml->frameGeometry() : mainWindow->frameGeometry();
                    const QRect shadow = frame.adjusted(-80, -80, 80, 80);
                    const QList<QRect> ring = {QRect(shadow.left(), shadow.top(), shadow.width(), frame.top() - shadow.top()),
                        QRect(shadow.left(), frame.bottom() + 1, shadow.width(), shadow.bottom() - frame.bottom()),
                        QRect(shadow.left(), frame.top(), frame.left() - shadow.left(), frame.height()),
                        QRect(frame.right() + 1, frame.top(), shadow.right() - frame.right(), frame.height())};
                    for (const QRect& rect : ring) {
                        lines.append(QStringLiteral("covered %1 %2 %3 %4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
                    }
                }
                for (QWidget* widget : QApplication::topLevelWidgets()) {
                    // Not the windows, the backdrops, or widgets embedded in a QML window (plugin docks).
                    const bool embedded = widget->windowHandle() && widget->windowHandle()->parent();
                    if (widget->isVisible() && !qobject_cast<MainWindow*>(widget) && !_backdrops.contains(widget) && !embedded) {
                        // Its frame (its geometry while the window manager hasn't framed it yet)
                        // and its shadow: Silver's largest reaches 80 px (64 blur, 16 offset).
                        QRect rect = widget->frameGeometry();
                        if (rect.width() <= 1 || rect.height() <= 1) {
                            rect = widget->geometry();
                        }
                        rect.adjust(-80, -80, 80, 80);
                        lines.append(QStringLiteral("covered %1 %2 %3 %4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
                    }
                }
                for (QWindow* dialog : qmlDialogs()) {
                    QRect rect = dialog->frameGeometry();
                    if (rect.width() <= 1 || rect.height() <= 1) {
                        rect = dialog->geometry();
                    }
                    rect.adjust(-80, -80, 80, 80);
                    lines.append(QStringLiteral("covered %1 %2 %3 %4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
                }
            }
            QFile file(QDir(_output).filePath(safeName(name) + QStringLiteral(".png.panes")));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text) || file.write((lines.join(u'\n') + u'\n').toUtf8()) < 0) {
                fail(QStringLiteral("cannot write panes of ") + name);
            }
        }

        void saveScreen(const QString& name)
        {
            const QString screenName = name + QStringLiteral(".screen");
            if (save(QGuiApplication::primaryScreen()->grabWindow(0), screenName)) {
                saveOverlays(screenName, overlays(nullptr));
                savePanes(screenName, nullptr);
            }
        }

        void dumpMenu(const QMenu* menu, const QString& indent)
        {
            for (QAction* action : menu->actions()) {
                if (!action->isVisible()) {
                    continue;
                }
                if (action->isSeparator()) {
                    trace(indent + QStringLiteral("----") + (action->text().isEmpty() ? QString() : QStringLiteral(" ") + action->text()));
                    continue;
                }
                trace(indent + QStringLiteral("%1 [%2]%3%4").arg(action->text(), action->shortcut().toString(QKeySequence::PortableText)).arg(action->isEnabled() ? QString() : QStringLiteral(" disabled")).arg(action->isCheckable() ? (action->isChecked() ? QStringLiteral(" checked") : QStringLiteral(" unchecked")) : QString()));
                if (action->menu()) {
                    dumpMenu(action->menu(), indent + QStringLiteral("  "));
                }
            }
        }

        void capturePopup(const QString& name, int remainingMs)
        {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) {
                if (remainingMs <= 0) {
                    return fail(QStringLiteral("no popup menu for ") + name);
                }
                QTimer::singleShot(50, this, [this, name, remainingMs] {
                    capturePopup(name, remainingMs - 50);
                });
                return;
            }
            QTimer::singleShot(300, this, [this, menu = QPointer<QMenu>(menu), name] {
                if (!menu) {
                    return fail(QStringLiteral("popup closed early: ") + name);
                }
                save(menu->grab(), QStringLiteral("menu-") + name);
                saveScreen(QStringLiteral("menu-") + name);
                trace(QStringLiteral("menu ") + name);
                dumpMenu(menu, QStringLiteral("  "));
                menu->close();
                next(200);
            });
        }

        QPoint cellCenter(TerminalDisplay* display, int column, int line) const
        {
            const int width = display->terminalFont()->fontWidth();
            const int height = display->terminalFont()->fontHeight();
            return display->contentRect().topLeft() + QPoint(column * width + width / 2, line * height + height / 2);
        }

        static Qt::KeyboardModifiers modifiers(const QJsonObject& step)
        {
            Qt::KeyboardModifiers result;
            const QString names = step.value(QStringLiteral("modifiers")).toString();
            if (names.contains(QLatin1String("ctrl"))) {
                result |= Qt::ControlModifier;
            }
            if (names.contains(QLatin1String("alt"))) {
                result |= Qt::AltModifier;
            }
            if (names.contains(QLatin1String("shift"))) {
                result |= Qt::ShiftModifier;
            }
            return result;
        }

        void sendMouse(const QJsonObject& step)
        {
            TerminalDisplay* display = view();
            if (!display) {
                return fail(QStringLiteral("mouse: no active view"));
            }
            static const QHash<QString, QEvent::Type> types = {{QStringLiteral("press"), QEvent::MouseButtonPress},
                {QStringLiteral("move"), QEvent::MouseMove},
                {QStringLiteral("release"), QEvent::MouseButtonRelease},
                {QStringLiteral("double"), QEvent::MouseButtonDblClick}};
            const QEvent::Type type = types.value(step.value(QStringLiteral("event")).toString(), QEvent::None);
            const QPoint local = cellCenter(display, step.value(QStringLiteral("column")).toInt(), step.value(QStringLiteral("line")).toInt());
            if (QWindow* qml = qmlWindow(qobject_cast<MainWindow*>(display->window()))) {
                // To the window, which delivers them to its items as Qt Quick gets a double click:
                // the second press, then the double click. Sent, like the widget path's, so no
                // button counts as held (a drag of the selection ends at once).
                const QPointF pos = display->mapTo(display->window(), local);
                const auto send = [qml, pos, &step](QEvent::Type eventType, Qt::MouseButton button, Qt::MouseButtons buttons, ulong timestamp) {
                    QMouseEvent event(eventType, pos, pos, qml->mapToGlobal(pos), button, buttons, modifiers(step));
                    event.setTimestamp(timestamp);
                    QCoreApplication::sendEvent(qml, &event);
                };
                const int interval = QGuiApplication::styleHints()->mouseDoubleClickInterval();
                switch (type) {
                case QEvent::MouseButtonPress:
                    _mouseTime += interval + 1;
                    send(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton, _mouseTime);
                    break;
                case QEvent::MouseButtonDblClick:
                    _mouseTime += 10;
                    send(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton, _mouseTime);
                    send(QEvent::MouseButtonDblClick, Qt::LeftButton, Qt::LeftButton, _mouseTime);
                    break;
                case QEvent::MouseMove:
                    _mouseTime += 10;
                    send(QEvent::MouseMove, Qt::NoButton, Qt::LeftButton, _mouseTime);
                    break;
                case QEvent::MouseButtonRelease:
                    _mouseTime += 10;
                    send(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton, _mouseTime);
                    break;
                default:
                    return fail(QStringLiteral("mouse: bad event"));
                }
                return next(step.value(QStringLiteral("delay")).toInt(50));
            }
            const Qt::MouseButton button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
            const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
            QMouseEvent event(type, local, display->mapToGlobal(local), button, buttons, modifiers(step));
            QCoreApplication::sendEvent(display, &event);
            next(step.value(QStringLiteral("delay")).toInt(50));
        }

        void sendText(const QString& text, bool toFocus)
        {
            QObject* target = toFocus ? QApplication::focusWidget() : view();
            if (toFocus && !target) {
                // A QML window delivers it to its focus item (the find bar's field).
                target = QGuiApplication::focusWindow();
            }
            if (!target) {
                return fail(QStringLiteral("type: no target"));
            }
            for (const QChar character : text) {
                const int key = character == u'\r' ? Qt::Key_Return : 0;
                const QString keyText = character == u'\r' ? QString() : QString(character);
                QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, keyText);
                QCoreApplication::sendEvent(target, &press);
                QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, keyText);
                QCoreApplication::sendEvent(target, &release);
            }
            next(50);
        }

        void sendKey(const QString& sequence)
        {
            const QKeySequence keys = QKeySequence::fromString(sequence, QKeySequence::PortableText);
            if (keys.isEmpty()) {
                return fail(QStringLiteral("bad key sequence ") + sequence);
            }
            QWidget* target = QApplication::focusWidget() ? QApplication::focusWidget() : window();
            QWindow* handle = target ? target->window()->windowHandle() : nullptr;
            if (QWindow* qml = target ? qmlWindow(qobject_cast<MainWindow*>(target->window())) : nullptr) {
                handle = qml;
            }
            // A focused QML dialog (its key recorder, its fields).
            if (QWindow* dialog = qmlDialog(QString())) {
                handle = dialog;
            }
            if (!handle) {
                return fail(QStringLiteral("key: no window"));
            }
            // QTest's window path goes through the window system, where shortcuts are matched.
            QTest::keyClick(handle, keys[0].key(), keys[0].keyboardModifiers());
            next(300);
        }

        // KAcceleratorManager gives the window's menus their accelerators only when a tab
        // page is shown while a window is active, which depends on activation timing.
        // Applying it once the window is active makes menu dumps the same on every run.
        void waitActive(QPointer<MainWindow> mainWindow, int remainingMs)
        {
            if (!mainWindow) {
                return fail(QStringLiteral("waitWindow: window closed"));
            }
            QWindow* qml = qmlWindow(mainWindow);
            const bool active = qml ? qml->isActive() : mainWindow->isActiveWindow();
            if (!active && remainingMs > 0) {
                QTimer::singleShot(50, this, [this, mainWindow, remainingMs] {
                    waitActive(mainWindow, remainingMs - 50);
                });
                return;
            }
            if (active) {
                KAcceleratorManager::manage(mainWindow);
            }
            trace(QStringLiteral("window %1").arg(active ? QStringLiteral("active") : QStringLiteral("inactive")));
            next(300);
        }

        void waitText(const QString& text, int remainingMs)
        {
            SessionController* active = controller();
            if (active && active->session() && active->session()->getAllDisplayedText().contains(text)) {
                return next();
            }
            if (remainingMs <= 0) {
                const QString shown = active && active->session() ? active->session()->getAllDisplayedText() : QStringLiteral("<no session>");
                return fail(QStringLiteral("waitText timed out: ") + text + QStringLiteral("\nscreen:\n") + shown);
            }
            QTimer::singleShot(50, this, [this, text, remainingMs] {
                waitText(text, remainingMs - 50);
            });
        }

        QDialog* visibleDialog() const
        {
            // A step's "in" names its dialog by window title, where several are open.
            if (!_targetTitle.isEmpty()) {
                for (QWidget* widget : QApplication::topLevelWidgets()) {
                    if (auto* dialog = qobject_cast<QDialog*>(widget); dialog && dialog->isVisible() && dialog->windowTitle() == _targetTitle) {
                        return dialog;
                    }
                }
                return nullptr;
            }
            // Sub-dialogs such as the color scheme editor aren't modal; the one in front is active.
            if (auto* active = qobject_cast<QDialog*>(QApplication::activeWindow()); active && active->isVisible()) {
                return active;
            }
            if (auto* modal = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                return modal;
            }
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (auto* dialog = qobject_cast<QDialog*>(widget); dialog && dialog->isVisible()) {
                    return dialog;
                }
            }
            return nullptr;
        }

        // A step's "panel" names a side panel (dock) by title instead of a dialog.
        QWidget* targetRoot() const
        {
            if (_targetPanel.isEmpty()) {
                return visibleDialog();
            }
            const auto widgets = QApplication::allWidgets();
            for (QWidget* widget : widgets) {
                auto* dock = qobject_cast<QDockWidget*>(widget);
                // KAcceleratorManager adds mnemonics to dock titles.
                if (dock && dock->isVisible() && KLocalizedString::removeAcceleratorMarker(dock->windowTitle()) == _targetPanel) {
                    QWidget* content = dockContent(dock);
                    return content ? content : dock;
                }
            }
            return nullptr;
        }

        // A step's "panel" when it is a QML plugin's.
        QObject* targetQmlPanel()
        {
            return _targetPanel.isEmpty() ? nullptr : qmlPanel(window(), _targetPanel);
        }

        // The QML dialog a step acts on: the focused or named one, else any shown while no
        // widget dialog is.
        QWindow* frontQmlDialog() const
        {
            if (!_targetPanel.isEmpty()) {
                return nullptr;
            }
            if (QWindow* dialog = qmlDialog(_targetTitle)) {
                return dialog;
            }
            const QList<QWindow*> dialogs = qmlDialogs();
            return _targetTitle.isEmpty() && !dialogs.isEmpty() && !visibleDialog() ? dialogs.constLast() : nullptr;
        }

        void clickQml(QObject* button, const QString& what, int delayMs)
        {
            if (!button) {
                return fail(QStringLiteral("no button %1 in %2").arg(what, frontQmlDialog() ? frontQmlDialog()->title() : QStringLiteral("<no dialog>")));
            }
            if (!button->property("enabled").toBool()) {
                return fail(QStringLiteral("button disabled: ") + what);
            }
            rememberShownDialogs();
            // Deferred: a button may open a menu or modal dialog that doesn't return until closed.
            QTimer::singleShot(0, button, [button] {
                QMetaObject::invokeMethod(button, "click");
            });
            next(delayMs);
        }

        // A QML dialog's capture.
        void captureQmlDialog(QPointer<QWindow> dialog, const QString& name, bool close, bool image)
        {
            trace(QStringLiteral("dialog ") + name + QStringLiteral(" ") + dialog->title());
            if (!image) {
                if (close) {
                    rejectQmlDialog(dialog);
                }
                return next(300);
            }
            captureQmlPages(dialog, name, qmlDialogPages(dialog), close, false);
        }

        // Each page in turn, as capturePages() for a widget page dialog.
        void captureQmlPages(QPointer<QWindow> dialog, const QString& name, QStringList pages, bool close, bool paged)
        {
            QTimer::singleShot(paged ? 300 : 400, this, [this, dialog, name, pages, close]() mutable {
                if (!dialog) {
                    return fail(QStringLiteral("dialog closed early: ") + name);
                }
                QString file = QStringLiteral("dialog-") + name;
                if (!pages.isEmpty()) {
                    const QString page = pages.takeFirst();
                    if (!showQmlDialogPage(dialog, page)) {
                        return fail(QStringLiteral("no page ") + page);
                    }
                    trace(QStringLiteral("  page ") + page);
                    file += QStringLiteral("-") + QString(page).remove(u'&');
                    QTimer::singleShot(300, this, [this, dialog, name, pages, close, file] {
                        if (!dialog) {
                            return fail(QStringLiteral("dialog closed early: ") + name);
                        }
                        saveQmlDialog(dialog, file);
                        if (!pages.isEmpty()) {
                            return captureQmlPages(dialog, name, pages, close, true);
                        }
                        if (close) {
                            rejectQmlDialog(dialog);
                        }
                        next(300);
                    });
                    return;
                }
                saveQmlDialog(dialog, file);
                if (close) {
                    rejectQmlDialog(dialog);
                }
                next(300);
            });
        }

        // The dialog's capture, and the screen around it.
        void saveQmlDialog(QWindow* dialog, const QString& file)
        {
            if (save(grabQmlDialog(dialog), file)) {
                QFile marker(QDir(_output).filePath(safeName(file) + QStringLiteral(".png.qml")));
                if (!marker.open(QIODevice::WriteOnly)) {
                    return fail(QStringLiteral("cannot write ") + marker.fileName());
                }
            }
            saveScreen(file);
        }

        template <typename T>
        T* dialogChild(const QString& object, const QString& text = QString())
        {
            QWidget* dialog = targetRoot();
            if (!dialog) {
                return nullptr;
            }
            // "@Class": the first visible widget of that class, or the T inside it.
            if (object.startsWith(u'@')) {
                const QByteArray className = object.mid(1).toLatin1();
                const auto widgets = dialog->findChildren<QWidget*>();
                for (QWidget* widget : widgets) {
                    if (!widget->isVisible() || !widget->inherits(className.constData())) {
                        continue;
                    }
                    if (auto* match = qobject_cast<T*>(widget)) {
                        return match;
                    }
                    if (auto* inner = widget->findChild<T*>()) {
                        return inner;
                    }
                }
                return nullptr;
            }
            if (!object.isEmpty()) {
                return dialog->findChild<T*>(object);
            }
            const auto children = dialog->findChildren<T*>();
            for (T* child : children) {
                if constexpr (std::is_base_of_v<QAbstractButton, T>) {
                    if (child->isVisible() && child->text().remove(u'&') == text) {
                        return child;
                    }
                }
            }
            return nullptr;
        }

        // Clicks are deferred: a button may open a modal dialog that doesn't return until closed.
        void click(QAbstractButton* button, const QString& what, int delayMs)
        {
            if (!button) {
                QDialog* dialog = visibleDialog();
                return fail(QStringLiteral("no button %1 in %2").arg(what, dialog ? dialog->windowTitle() : QStringLiteral("<no dialog>")));
            }
            if (!button->isEnabled()) {
                return fail(QStringLiteral("button disabled: ") + what);
            }
            rememberShownDialogs();
            QTimer::singleShot(0, button, &QAbstractButton::click);
            next(delayMs);
        }

        void selectPage(const QString& title)
        {
            if (QWindow* qml = frontQmlDialog()) {
                if (!showQmlDialogPage(qml, title)) {
                    return fail(QStringLiteral("no page ") + title);
                }
                return next(300);
            }
            QDialog* dialog = visibleDialog();
            KPageView* view = dialog ? dialog->findChild<KPageView*>() : nullptr;
            if (!view) {
                return fail(QStringLiteral("page: no page dialog"));
            }
            QList<QModelIndex> pending = {QModelIndex()};
            while (!pending.isEmpty()) {
                const QModelIndex parent = pending.takeFirst();
                for (int row = 0; row < view->model()->rowCount(parent); ++row) {
                    const QModelIndex index = view->model()->index(row, 0, parent);
                    if (index.data(Qt::DisplayRole).toString().remove(u'&') == title) {
                        view->setCurrentPage(index);
                        return next(300);
                    }
                    pending.append(index);
                }
            }
            fail(QStringLiteral("no page ") + title);
        }

        void selectRow(const QString& object, int row, const QString& text)
        {
            if (QWindow* qml = frontQmlDialog()) {
                if (!selectQmlRow(qml, object, row, text)) {
                    return fail(QStringLiteral("select: no row %1 (%2) in %3").arg(row).arg(text, object));
                }
                return next(300);
            }
            if (auto* combo = dialogChild<QComboBox>(object)) {
                if (row < 0) {
                    row += combo->count();
                }
                if (row < 0 || row >= combo->count()) {
                    return fail(QStringLiteral("select: no row %1 in %2").arg(row).arg(object));
                }
                combo->setCurrentIndex(row);
                return next(300);
            }
            if (auto* itemView = dialogChild<QAbstractItemView>(object)) {
                QModelIndex index = itemView->model()->index(row, 0, itemView->rootIndex());
                if (!text.isEmpty()) {
                    const QModelIndexList found = itemView->model()->match(itemView->model()->index(0, 0, itemView->rootIndex()), Qt::DisplayRole, text, 1,
                        Qt::MatchStartsWith | Qt::MatchRecursive);
                    index = found.value(0);
                }
                if (!index.isValid()) {
                    return fail(QStringLiteral("select: no row %1 in %2").arg(row).arg(object));
                }
                itemView->setCurrentIndex(index);
                return next(300);
            }
            fail(QStringLiteral("select: no list ") + object);
        }

        void rememberShownDialogs()
        {
            _shownBeforeClick.clear();
            const QList<QWindow*> shown = qmlDialogs();
            for (QWindow* window : shown) {
                _shownBeforeClick.append(window);
            }
        }

        void captureDialog(const QString& name, bool close, int remainingMs, bool image = true)
        {
            QWindow* qml = frontQmlDialog();
            // A dialog the click opens (KNewStuff's, a widget dialog) can take a moment to show:
            // wait for it rather than take the dialog the click was made in, until the timeout.
            // Shown, it is the one, even before it has the focus.
            if (qml && _shownBeforeClick.contains(qml)) {
                if (visibleDialog()) {
                    qml = nullptr;
                } else if (remainingMs > 0) {
                    QTimer::singleShot(100, this, [this, name, close, remainingMs, image] {
                        captureDialog(name, close, remainingMs - 100, image);
                    });
                    return;
                }
            }
            if (qml) {
                _shownBeforeClick.clear();
                return captureQmlDialog(qml, name, close, image);
            }
            QDialog* dialog = visibleDialog();
            if (!dialog) {
                if (remainingMs <= 0) {
                    return fail(QStringLiteral("no dialog visible for ") + name);
                }
                QTimer::singleShot(100, this, [this, name, close, remainingMs, image] {
                    captureDialog(name, close, remainingMs - 100, image);
                });
                return;
            }
            _shownBeforeClick.clear();
            trace(QStringLiteral("dialog ") + name + QStringLiteral(" ") + dialog->windowTitle());
            if (!image) {
                // Contents depend on the outside world (KNewStuff's network); only the dialog's opening is checked.
                if (close) {
                    dialog->reject();
                }
                return next(300);
            }
            QList<QModelIndex> pages;
            if (auto* view = dialog->findChild<KPageView*>()) {
                QList<QModelIndex> pending = {QModelIndex()};
                while (!pending.isEmpty()) {
                    const QModelIndex parent = pending.takeFirst();
                    for (int row = 0; row < view->model()->rowCount(parent); ++row) {
                        const QModelIndex index = view->model()->index(row, 0, parent);
                        pages.append(index);
                        pending.append(index);
                    }
                }
            }
            capturePages(QPointer<QDialog>(dialog), name, pages, close);
        }

        void capturePages(QPointer<QDialog> dialog, const QString& name, QList<QModelIndex> pages, bool close)
        {
            QTimer::singleShot(400, this, [this, dialog, name, pages, close]() mutable {
                if (!dialog) {
                    return fail(QStringLiteral("dialog closed early: ") + name);
                }
                QString suffix;
                if (auto* view = dialog->findChild<KPageView*>(); view && !pages.isEmpty()) {
                    const QModelIndex page = pages.takeFirst();
                    view->setCurrentPage(page);
                    suffix = QStringLiteral("-") + page.data(Qt::DisplayRole).toString().remove(u'&');
                    trace(QStringLiteral("  page ") + page.data(Qt::DisplayRole).toString());
                    QTimer::singleShot(300, this, [this, dialog, name, pages, close, suffix] {
                        if (!dialog) {
                            return fail(QStringLiteral("dialog closed early: ") + name);
                        }
                        save(dialog->grab(), QStringLiteral("dialog-") + name + suffix);
                        saveScreen(QStringLiteral("dialog-") + name + suffix);
                        if (!pages.isEmpty()) {
                            return capturePages(dialog, name, pages, close);
                        }
                        if (close) {
                            dialog->reject();
                        }
                        next(300);
                    });
                    return;
                }
                save(dialog->grab(), QStringLiteral("dialog-") + name);
                saveScreen(QStringLiteral("dialog-") + name);
                if (close) {
                    dialog->reject();
                }
                next(300);
            });
        }

        void writeFile(const QString& name, const QString& text)
        {
            const QString safe = safeName(name);
            QFile file(QDir(_output).filePath(safe));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text) || file.write(text.toUtf8()) < 0) {
                return fail(QStringLiteral("cannot write ") + safe);
            }
            trace(QStringLiteral("wrote ") + safe);
        }

        void introspect(const QStringList& paths)
        {
            QDBusConnection bus = QDBusConnection::sessionBus();
            const QString self = bus.baseService();
            QStringList names;
            const QStringList all = bus.interface()->registeredServiceNames().value();
            for (const QString& name : all) {
                if (!name.startsWith(u':') && bus.interface()->serviceOwner(name).value() == self) {
                    names.append(QString(name).replace(QString::number(QCoreApplication::applicationPid()), QStringLiteral("<pid>")));
                }
            }
            names.sort();
            QString text = QStringLiteral("names: ") + names.join(QStringLiteral(", ")) + u'\n';
            for (const QString& path : paths) {
                QDBusMessage call = QDBusMessage::createMethodCall(self, path, QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"));
                const QDBusMessage reply = bus.call(call);
                text += QStringLiteral("== ") + path + u'\n'
                    + (reply.type() == QDBusMessage::ReplyMessage ? reply.arguments().value(0).toString() : QStringLiteral("ERROR ") + reply.errorName()) + u'\n';
            }
            writeFile(QStringLiteral("dbus.txt"), text);
            next();
        }

        void xprop(const QString& name)
        {
            MainWindow* mainWindow = window();
            if (!mainWindow) {
                return fail(QStringLiteral("xprop: no window"));
            }
            static const QStringList perRun = {QStringLiteral("_NET_WM_PID"), QStringLiteral("_NET_WM_USER_TIME"), QStringLiteral("WM_CLIENT_LEADER"),
                QStringLiteral("_NET_WM_SYNC_REQUEST_COUNTER"), QStringLiteral("_KDE_NET_WM_USER_CREATION_TIME"),
                QStringLiteral("WM_CLIENT_MACHINE"), QStringLiteral("_NET_WM_USER_TIME_WINDOW"),
                QStringLiteral("_NET_WM_ICON"), QStringLiteral("WM_COMMAND")};
            QProcess process;
            QWindow* qml = qmlWindow(mainWindow);
            process.start(QStringLiteral("xprop"), {QStringLiteral("-id"), QString::number(qml ? qml->winId() : mainWindow->winId())});
            process.waitForFinished(5000);
            QStringList lines;
            const QStringList output = QString::fromUtf8(process.readAllStandardOutput()).split(u'\n');
            for (const QString& line : output) {
                const QString key = line.section(u'(', 0, 0).section(u':', 0, 0).trimmed();
                if (line.startsWith(u'\t') || perRun.contains(key)) {
                    continue;
                }
                lines.append(QString(line).replace(QRegularExpression(QStringLiteral("0x[0-9a-f]+")), QStringLiteral("0xID")));
            }
            writeFile(QStringLiteral("xprop-") + name + QStringLiteral(".txt"), lines.join(u'\n') + u'\n');
            next();
        }

        void accessible(const QString& name)
        {
            TerminalDisplay* display = view();
            QAccessibleInterface* interface = display ? QAccessible::queryAccessibleInterface(display) : nullptr;
            if (!interface) {
                return fail(QStringLiteral("accessible: no interface"));
            }
            QString text = QStringLiteral("role %1\nname %2\ndescription %3\n")
                               .arg(int(interface->role()))
                               .arg(interface->text(QAccessible::Name), interface->text(QAccessible::Description));
            if (QAccessibleTextInterface* textInterface = interface->textInterface()) {
                int start = 0;
                int end = 0;
                textInterface->selection(0, &start, &end);
                text += QStringLiteral("characters %1\ncursor %2\nselections %3 (%4-%5)\ntext:\n%6\n")
                            .arg(textInterface->characterCount())
                            .arg(textInterface->cursorPosition())
                            .arg(textInterface->selectionCount())
                            .arg(start)
                            .arg(end)
                            .arg(textInterface->text(0, textInterface->characterCount()));
            }
            writeFile(QStringLiteral("accessible-") + name + QStringLiteral(".txt"), text);
            next();
        }

        void environment(const QString& fileName, int remainingMs)
        {
            QFile file(QDir::home().filePath(fileName));
            if (!file.exists() || file.size() == 0) {
                if (remainingMs <= 0) {
                    return fail(QStringLiteral("env: no ") + fileName);
                }
                QTimer::singleShot(100, this, [this, fileName, remainingMs] {
                    environment(fileName, remainingMs - 100);
                });
                return;
            }
            static const QStringList kept = {QStringLiteral("TERM"), QStringLiteral("COLORTERM"), QStringLiteral("SHELL_SESSION_ID"),
                QStringLiteral("WINDOWID"), QStringLiteral("FLATPAK_TTY_PROGRESS"), QStringLiteral("PS1"),
                QStringLiteral("LANGUAGE")};
            static const QStringList masked = {QStringLiteral("SHELL_SESSION_ID"), QStringLiteral("WINDOWID"), QStringLiteral("KONSOLE_DBUS_SERVICE"),
                QStringLiteral("KONSOLE_DBUS_ACTIVATION_COOKIE"), QStringLiteral("SONIC_TERMINAL_DBUS_SERVICE")};
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                return fail(QStringLiteral("env: cannot read ") + fileName);
            }
            QStringList lines;
            const QStringList all = QString::fromUtf8(file.readAll()).split(u'\n');
            for (const QString& line : all) {
                const QString key = line.section(u'=', 0, 0);
                if (!key.startsWith(QLatin1String("KONSOLE_")) && !key.startsWith(QLatin1String("SONIC_TERMINAL_DBUS_")) && !kept.contains(key)) {
                    continue;
                }
                lines.append(masked.contains(key) ? key + (line.section(u'=', 1).isEmpty() ? QStringLiteral("=") : QStringLiteral("=<set>")) : line);
            }
            writeFile(QStringLiteral("env-masked.txt"), lines.join(u'\n') + u'\n');
            next();
        }

        void describe(const QString& name)
        {
            window();
            QString text;
            for (int i = 0; i < _windows.size(); ++i) {
                MainWindow* mainWindow = _windows.at(i);
                const auto sessions = mainWindow->viewManager()->sessions();
                text += QStringLiteral("window %1: %2 sessions, menubar %3, state %4\n")
                            .arg(i)
                            .arg(sessions.size())
                            .arg(mainWindow->menuBar()->isVisible() ? QStringLiteral("shown") : QStringLiteral("hidden"))
                            .arg(int(mainWindow->windowState()));
                QStringList lines;
                for (Session* session : sessions) {
                    lines.append(QStringLiteral("  session %1 | %2 | %3\n")
                            .arg(session->title(Session::NameRole), session->program(), QString(session->currentWorkingDirectory()).replace(QDir::homePath(), QStringLiteral("~"))));
                }
                lines.sort();
                text += lines.join(QString());
            }
            writeFile(QStringLiteral("describe-") + name + QStringLiteral(".txt"), text);
            next();
        }

        static QAction* actionByText(QMenu* menu, const QString& text)
        {
            const auto actions = menu->actions();
            for (QAction* action : actions) {
                if (KLocalizedString::removeAcceleratorMarker(action->text()) == text) {
                    return action;
                }
            }
            return nullptr;
        }

        static QString actionTexts(QMenu* menu)
        {
            QStringList texts;
            const auto actions = menu->actions();
            for (QAction* action : actions) {
                if (!action->isSeparator()) {
                    texts.append(KLocalizedString::removeAcceleratorMarker(action->text()));
                }
            }
            return texts.join(QStringLiteral(", "));
        }

        // Triggers "text" in a menu reached from "menu" through the submenus "path", or with
        // "context", clicks that entry of the item's context menu.
        void menuItem(const QJsonObject& step)
        {
            const QString name = step.value(QStringLiteral("menu")).toString();
            MainWindow* mainWindow = window();
            auto* menu = mainWindow ? qobject_cast<QMenu*>(mainWindow->guiFactory()->container(name, mainWindow)) : nullptr;
            if (!menu) {
                QAction* rootAction = findAction(name);
                menu = rootAction ? rootAction->menu() : nullptr;
            }
            if (!menu) {
                return fail(QStringLiteral("menuItem: no menu ") + name);
            }
            const QStringList path = step.value(QStringLiteral("path")).toVariant().toStringList();
            const QString text = step.value(QStringLiteral("text")).toString();
            // A KBookmarkMenu fills itself before it shows.
            Q_EMIT menu->aboutToShow();
            for (const QString& title : path) {
                QAction* submenu = actionByText(menu, title);
                if (!submenu || !submenu->menu()) {
                    return fail(QStringLiteral("menuItem: no submenu %1 in %2 (%3)").arg(title, name, actionTexts(menu)));
                }
                menu = submenu->menu();
                Q_EMIT menu->aboutToShow();
            }
            QAction* action = actionByText(menu, text);
            if (!action) {
                return fail(QStringLiteral("menuItem: no item %1 in %2 (%3)").arg(text, name, actionTexts(menu)));
            }
            const int delay = step.value(QStringLiteral("delay")).toInt(500);
            const QString context = step.value(QStringLiteral("context")).toString();
            if (context.isEmpty()) {
                QTimer::singleShot(0, action, &QAction::trigger);
                return next(delay);
            }
            Q_EMIT menu->customContextMenuRequested(menu->actionGeometry(action).center());
            clickPopupItem(context, delay, 2000);
        }

        // "text" finds the item anywhere in the tree, else "row"/"column" at the top level.
        static QModelIndex clickItemIndex(QAbstractItemView* itemView, const QJsonObject& step)
        {
            if (!itemView) {
                return {};
            }
            QAbstractItemModel* model = itemView->model();
            const QString text = step.value(QStringLiteral("text")).toString();
            if (text.isEmpty()) {
                return model->index(step.value(QStringLiteral("row")).toInt(), step.value(QStringLiteral("column")).toInt());
            }
            return model->match(model->index(0, 0), Qt::DisplayRole, text, 1, Qt::MatchExactly | Qt::MatchRecursive).value(0);
        }

        void clickPopupItem(const QString& text, int delay, int remainingMs)
        {
            auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            QAction* action = popup ? actionByText(popup, text) : nullptr;
            if (!action) {
                if (remainingMs <= 0) {
                    return fail(QStringLiteral("menuItem: no context menu item %1 (%2)").arg(text, popup ? actionTexts(popup) : QStringLiteral("no popup")));
                }
                QTimer::singleShot(50, this, [this, text, delay, remainingMs] {
                    clickPopupItem(text, delay, remainingMs - 50);
                });
                return;
            }
            // A click, as the user's: the menu closes, then the action triggers (and may open a
            // modal dialog, so deferred).
            QTimer::singleShot(0, popup, [popup = QPointer<QMenu>(popup), point = popup->actionGeometry(action).center()] {
                if (popup) {
                    QTest::mouseClick(popup, Qt::LeftButton, Qt::NoModifier, point);
                }
            });
            next(delay);
        }

        void fail(const QString& message)
        {
            sonicCritical() << "parity:" << message;
            if (!_failed) {
                _failed = true;
                QFile file(QDir(_output).filePath(QStringLiteral("trace.txt")));
                if (file.open(QIODevice::Append | QIODevice::Text)) {
                    file.write((QStringLiteral("FAIL ") + message).toUtf8() + '\n');
                }
            }
            QCoreApplication::exit(3);
        }

        // NoWidgetsTest: every widget but those allowedWidget() and the backdrops pass fails.
        bool checkWidgets()
        {
            QStringList stray;
            const QWidgetList widgets = QApplication::allWidgets();
            for (const QWidget* widget : widgets) {
                if (allowedWidget(widget) || _backdrops.contains(widget->window())) {
                    continue;
                }
                QStringList chain;
                for (const QWidget* current = widget; current; current = current->parentWidget()) {
                    chain.append(QStringLiteral("%1(%2)").arg(QLatin1String(current->metaObject()->className()), current->objectName()));
                }
                QStringList children;
                for (const QObject* child : widget->children()) {
                    children.append(QLatin1String(child->metaObject()->className()));
                }
                stray.append(QStringLiteral("%1 [%2; children: %3]")
                        .arg(chain.join(QStringLiteral(" < ")),
                            widget->isVisible() ? QStringLiteral("shown") : QStringLiteral("hidden"),
                            children.join(QStringLiteral(", "))));
            }
            if (stray.isEmpty()) {
                return true;
            }
            stray.sort();
            fail(QStringLiteral("stray widgets before step %1: %2").arg(_index + 1).arg(stray.join(QStringLiteral("; "))));
            return false;
        }

        void run()
        {
            if (_failed || (_checkWidgets && !checkWidgets())) {
                return;
            }
            if (_index >= _steps.size()) {
                sonicInfo() << "parity: script finished";
                QCoreApplication::exit(0);
                return;
            }
            const QJsonObject step = _steps.at(_index++);
            const QString op = step.value(QStringLiteral("op")).toString();
            _targetTitle = step.value(QStringLiteral("in")).toString();
            _targetPanel = step.value(QStringLiteral("panel")).toString();
            sonicDebug() << "parity: step" << _index << op;

            if (op == QLatin1String("output")) {
                _output = step.value(QStringLiteral("dir")).toString();
                QDir().mkpath(_output);
                return next();
            }
            if (op == QLatin1String("waitWindow")) {
                const int index = step.value(QStringLiteral("window")).toInt(0);
                if (!window(index) || !window(index)->viewManager()->activeViewController()) {
                    --_index;
                    return next(100);
                }
                return waitActive(window(index), 5000);
            }
            if (op == QLatin1String("useWindow")) {
                _currentWindow = step.value(QStringLiteral("window")).toInt();
                if (MainWindow* mainWindow = window()) {
                    if (QWindow* qml = qmlWindow(mainWindow)) {
                        qml->requestActivate();
                    } else {
                        mainWindow->activateWindow();
                    }
                }
                return next(300);
            }
            if (op == QLatin1String("wait")) {
                return next(step.value(QStringLiteral("ms")).toInt());
            }
            if (op == QLatin1String("checkWidgets")) {
                _checkWidgets = true;
                return next();
            }
            if (op == QLatin1String("strayWidgets")) {
                // NoWidgetsTest's negative case: widgets no rule allows.
                auto* dialog = new QDialog;
                dialog->setObjectName(QStringLiteral("strayDialog"));
                dialog->show();
                auto* label = new QLabel(QStringLiteral("stray"));
                label->setObjectName(QStringLiteral("strayLabel"));
                label->show();
                return next(300);
            }
            if (op == QLatin1String("geometry")) {
                if (MainWindow* mainWindow = window()) {
                    const QPoint position(step.value(QStringLiteral("x")).toInt(), step.value(QStringLiteral("y")).toInt());
                    const QSize size(step.value(QStringLiteral("width")).toInt(), step.value(QStringLiteral("height")).toInt());
                    // As QWidget::move(): the frame's position.
                    if (QWindow* qml = qmlWindow(mainWindow)) {
                        const QMargins frame = qml->frameMargins();
                        qml->setGeometry(QRect(position + QPoint(frame.left(), frame.top()), size));
                    } else {
                        mainWindow->move(position);
                        mainWindow->resize(size);
                    }
                }
                return next(300);
            }
            if (op == QLatin1String("windowState")) {
                if (MainWindow* mainWindow = window()) {
                    const Qt::WindowState state = step.value(QStringLiteral("state")).toString() == QLatin1String("maximized") ? Qt::WindowMaximized : Qt::WindowNoState;
                    if (QWindow* qml = qmlWindow(mainWindow)) {
                        qml->setWindowState(state);
                    } else {
                        mainWindow->setWindowState(state);
                    }
                }
                return next(800);
            }
            if (op == QLatin1String("backdrop")) {
                const QString fill = step.value(QStringLiteral("fill")).toString();
                if (fill.isEmpty()) {
                    qDeleteAll(_backdrops);
                    _backdrops.clear();
                    return next(300);
                }
                auto* backdrop = new Backdrop(fill, QGuiApplication::primaryScreen()->geometry());
                backdrop->show();
                _backdrops.append(backdrop);
                return next(500);
            }
            if (op == QLatin1String("type")) {
                return sendText(step.value(QStringLiteral("text")).toString(), step.value(QStringLiteral("target")).toString() == QLatin1String("focus"));
            }
            if (op == QLatin1String("key")) {
                return sendKey(step.value(QStringLiteral("sequence")).toString());
            }
            if (op == QLatin1String("waitText")) {
                return waitText(step.value(QStringLiteral("text")).toString(), step.value(QStringLiteral("timeout")).toInt(5000));
            }
            if (op == QLatin1String("mouse")) {
                return sendMouse(step);
            }
            if (op == QLatin1String("selection")) {
                TerminalDisplay* display = view();
                const QString text = display && display->screenWindow() ? display->screenWindow()->selectedText(Screen::PreserveLineBreaks) : QString();
                trace(QStringLiteral("selection %1: %2").arg(step.value(QStringLiteral("label")).toString(), text));
                return next();
            }
            if (op == QLatin1String("action")) {
                QAction* action = findAction(step.value(QStringLiteral("id")).toString());
                if (!action) {
                    return fail(QStringLiteral("no action ") + step.value(QStringLiteral("id")).toString());
                }
                // Deferred, because some actions open modal dialogs and don't return until closed.
                QTimer::singleShot(0, action, &QAction::trigger);
                return next(step.value(QStringLiteral("delay")).toInt(500));
            }
            if (op == QLatin1String("shortcut")) {
                const QString id = step.value(QStringLiteral("id")).toString();
                QAction* action = findAction(id);
                if (!action) {
                    return fail(QStringLiteral("no action ") + id);
                }
                if (step.contains(QStringLiteral("keys"))) {
                    // As the shortcuts editor does when a key is recorded.
                    action->setShortcut(QKeySequence(step.value(QStringLiteral("keys")).toString()));
                } else {
                    const QString keys = action->shortcut().toString();
                    trace(QStringLiteral("shortcut %1 window %2: %3").arg(id).arg(_currentWindow).arg(keys));
                    if (step.contains(QStringLiteral("expect")) && keys != step.value(QStringLiteral("expect")).toString()) {
                        return fail(QStringLiteral("shortcut %1 is %2, expected %3").arg(id, keys, step.value(QStringLiteral("expect")).toString()));
                    }
                }
                return next();
            }
            if (op == QLatin1String("introspect")) {
                return introspect(step.value(QStringLiteral("paths")).toVariant().toStringList());
            }
            if (op == QLatin1String("xprop")) {
                return xprop(step.value(QStringLiteral("name")).toString());
            }
            if (op == QLatin1String("accessible")) {
                return accessible(step.value(QStringLiteral("name")).toString());
            }
            if (op == QLatin1String("env")) {
                return environment(step.value(QStringLiteral("file")).toString(), step.value(QStringLiteral("timeout")).toInt(5000));
            }
            if (op == QLatin1String("homeFile")) {
                QFile file(QDir::home().filePath(step.value(QStringLiteral("file")).toString()));
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    return fail(QStringLiteral("homeFile: cannot read ") + file.fileName());
                }
                writeFile(step.value(QStringLiteral("file")).toString().replace(u'/', u'_') + QStringLiteral(".txt"), QString::fromUtf8(file.readAll()));
                return next();
            }
            if (op == QLatin1String("describe")) {
                return describe(step.value(QStringLiteral("name")).toString());
            }
            if (op == QLatin1String("listActions")) {
                if (MainWindow* mainWindow = window()) {
                    const auto clients = mainWindow->guiFactory()->clients();
                    for (KXMLGUIClient* client : clients) {
                        const auto actions = client->actionCollection()->actions();
                        for (QAction* action : actions) {
                            trace(QStringLiteral("action %1 \"%2\"").arg(action->objectName(), action->text()));
                        }
                    }
                }
                return next();
            }
            if (op == QLatin1String("capture")) {
                const QString name = step.value(QStringLiteral("name")).toString();
                if (MainWindow* mainWindow = window()) {
                    const QPixmap qml = grabQmlWindow(mainWindow);
                    if (save(qml.isNull() ? mainWindow->grab() : qml, name + QStringLiteral(".client"))) {
                        saveOverlays(name + QStringLiteral(".client"), overlays(mainWindow));
                        savePanes(name + QStringLiteral(".client"), mainWindow);
                    }
                }
                saveScreen(name);
                trace(QStringLiteral("capture ") + name);
                return next();
            }
            if (op == QLatin1String("menu")) {
                MainWindow* mainWindow = window();
                const QString name = step.value(QStringLiteral("name")).toString();
                auto* menu = mainWindow ? qobject_cast<QMenu*>(mainWindow->guiFactory()->container(name, mainWindow)) : nullptr;
                if (!menu) {
                    // Some menus (Bookmarks) are actions with their own menu, not KXmlGui containers.
                    QAction* action = findAction(name);
                    menu = action ? action->menu() : nullptr;
                }
                if (!menu) {
                    return fail(QStringLiteral("no menu ") + name);
                }
                if (!mainWindow->menuBar()->isVisible() || !popupQmlMenu(mainWindow, menu)) {
                    const QRect item = mainWindow->menuBar()->actionGeometry(menu->menuAction());
                    menu->popup(mainWindow->menuBar()->isVisible() ? mainWindow->menuBar()->mapToGlobal(item.bottomLeft()) : mainWindow->mapToGlobal(QPoint(0, 0)));
                }
                return capturePopup(name, 2000);
            }
            if (op == QLatin1String("menuItem")) {
                return menuItem(step);
            }
            if (op == QLatin1String("popup")) {
                return capturePopup(step.value(QStringLiteral("name")).toString(), step.value(QStringLiteral("timeout")).toInt(2000));
            }
            if (op == QLatin1String("contextMenu")) {
                TerminalDisplay* display = view();
                if (!display) {
                    return fail(QStringLiteral("contextMenu: no active view"));
                }
                const QPoint local = cellCenter(display, step.value(QStringLiteral("column")).toInt(), step.value(QStringLiteral("line")).toInt());
                // Posted: the context menu runs its own event loop until it's closed.
                QCoreApplication::postEvent(display, new QContextMenuEvent(QContextMenuEvent::Mouse, local, display->mapToGlobal(local), modifiers(step)));
                return capturePopup(step.value(QStringLiteral("name")).toString(), 2000);
            }
            if (op == QLatin1String("dialog")) {
                // Some lists (notification events) come in hash order; sort them so runs compare.
                if (step.value(QStringLiteral("sortColumn")).isDouble()) {
                    if (QDialog* dialog = visibleDialog()) {
                        const auto views = dialog->findChildren<QAbstractItemView*>();
                        for (QAbstractItemView* itemView : views) {
                            itemView->model()->sort(step.value(QStringLiteral("sortColumn")).toInt(), Qt::AscendingOrder);
                        }
                    }
                }
                return captureDialog(step.value(QStringLiteral("name")).toString(), step.value(QStringLiteral("close")).toBool(true),
                    step.value(QStringLiteral("timeout")).toInt(3000), step.value(QStringLiteral("image")).toBool(true));
            }
            if (op == QLatin1String("click")) {
                const QString object = step.value(QStringLiteral("object")).toString();
                const QString text = step.value(QStringLiteral("text")).toString();
                const int delay = step.value(QStringLiteral("delay")).toInt(800);
                if (QObject* panel = targetQmlPanel()) {
                    return clickQml(qmlPanelButton(panel, object), object, delay);
                }
                if (QWindow* qml = frontQmlDialog()) {
                    return clickQml(qmlDialogButton(qml, object, text, 0), object.isEmpty() ? text : object, delay);
                }
                return click(dialogChild<QAbstractButton>(object, text), object.isEmpty() ? text : object, delay);
            }
            if (op == QLatin1String("button")) {
                static const QHash<QString, QDialogButtonBox::StandardButton> roles = {{QStringLiteral("ok"), QDialogButtonBox::Ok},
                    {QStringLiteral("cancel"), QDialogButtonBox::Cancel},
                    {QStringLiteral("apply"), QDialogButtonBox::Apply}};
                const QString role = step.value(QStringLiteral("role")).toString();
                if (QWindow* qml = frontQmlDialog()) {
                    // QML's DialogButtonBox shares QDialogButtonBox's values.
                    return clickQml(qmlDialogButton(qml, QString(), QString(), int(roles.value(role, QDialogButtonBox::NoButton))), role,
                        step.value(QStringLiteral("delay")).toInt(800));
                }
                auto* box = dialogChild<QDialogButtonBox>(QString());
                if (!box) {
                    if (QDialog* dialog = visibleDialog()) {
                        box = dialog->findChild<QDialogButtonBox*>();
                    }
                }
                return click(box ? box->button(roles.value(role, QDialogButtonBox::NoButton)) : nullptr, role, step.value(QStringLiteral("delay")).toInt(800));
            }
            if (op == QLatin1String("page")) {
                return selectPage(step.value(QStringLiteral("name")).toString());
            }
            if (op == QLatin1String("select")) {
                return selectRow(step.value(QStringLiteral("object")).toString(), step.value(QStringLiteral("row")).toInt(),
                    step.value(QStringLiteral("text")).toString());
            }
            if (op == QLatin1String("clickItem")) {
                const QString object = step.value(QStringLiteral("object")).toString();
                if (QObject* panel = targetQmlPanel()) {
                    const QString context = step.value(QStringLiteral("context")).toString();
                    const QString kind = !context.isEmpty()             ? QStringLiteral("context")
                        : step.value(QStringLiteral("double")).toBool() ? QStringLiteral("double")
                                                                        : QStringLiteral("click");
                    if (!clickQmlPanelItem(panel, object, step.value(QStringLiteral("text")).toString(), kind)) {
                        return fail(QStringLiteral("clickItem: no item in ") + object);
                    }
                    const int delay = step.value(QStringLiteral("delay")).toInt(800);
                    return context.isEmpty() ? next(delay) : clickPopupItem(context, delay, 2000);
                }
                if (QWindow* qml = frontQmlDialog()) {
                    if (!clickQmlItem(qml, object, step.value(QStringLiteral("row")).toInt(), step.value(QStringLiteral("column")).toInt(),
                            step.value(QStringLiteral("text")).toString())) {
                        return fail(QStringLiteral("clickItem: no item in ") + object);
                    }
                    return next(step.value(QStringLiteral("delay")).toInt(800));
                }
                auto* itemView = dialogChild<QAbstractItemView>(object);
                const QModelIndex index = clickItemIndex(itemView, step);
                if (!index.isValid()) {
                    return fail(QStringLiteral("clickItem: no item in ") + object);
                }
                itemView->scrollTo(index);
                const QPoint point = itemView->visualRect(index).center();
                const int delay = step.value(QStringLiteral("delay")).toInt(800);
                if (!step.value(QStringLiteral("context")).toString().isEmpty()) {
                    // A right click makes the row current, as the user's does; Qt makes no context
                    // menu event from a synthetic click, so the request follows it.
                    QTimer::singleShot(0, itemView, [itemView = QPointer<QAbstractItemView>(itemView), point] {
                        if (!itemView) {
                            return;
                        }
                        QTest::lastMouseTimestamp += 10000;
                        QTest::mouseClick(itemView->viewport(), Qt::RightButton, Qt::NoModifier, point);
                        if (itemView && !QApplication::activePopupWidget()) {
                            Q_EMIT itemView->customContextMenuRequested(point);
                        }
                    });
                    return clickPopupItem(step.value(QStringLiteral("context")).toString(), delay, 2000);
                }
                // Deferred, like click(): the item may open a modal dialog.
                QTimer::singleShot(0, itemView, [viewport = QPointer<QWidget>(itemView->viewport()), point, twice = step.value(QStringLiteral("double")).toBool()] {
                    if (!viewport) {
                        return;
                    }
                    // QTest's synthetic clock: keep an earlier click from pairing with this one.
                    QTest::lastMouseTimestamp += 10000;
                    // mouseDClick() sends the first click too.
                    if (twice) {
                        QTest::mouseDClick(viewport, Qt::LeftButton, Qt::NoModifier, point);
                    } else {
                        QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, point);
                    }
                });
                return next(delay);
            }
            if (op == QLatin1String("setText")) {
                if (QObject* panel = targetQmlPanel()) {
                    if (!setQmlPanelText(panel, step.value(QStringLiteral("object")).toString(), step.value(QStringLiteral("text")).toString())) {
                        return fail(QStringLiteral("setText: no line edit ") + step.value(QStringLiteral("object")).toString());
                    }
                    return next(300);
                }
                if (QWindow* qml = frontQmlDialog()) {
                    if (!setQmlText(qml, step.value(QStringLiteral("object")).toString(), step.value(QStringLiteral("text")).toString())) {
                        return fail(QStringLiteral("setText: no line edit ") + step.value(QStringLiteral("object")).toString());
                    }
                    return next(300);
                }
                const QString object = step.value(QStringLiteral("object")).toString();
                if (auto* edit = dialogChild<QLineEdit>(object)) {
                    edit->setText(step.value(QStringLiteral("text")).toString());
                    return next(300);
                }
                if (auto* edit = dialogChild<QPlainTextEdit>(object)) {
                    edit->setPlainText(step.value(QStringLiteral("text")).toString());
                    return next(300);
                }
                return fail(QStringLiteral("setText: no line edit ") + object);
            }
            if (op == QLatin1String("clipboard")) {
                QGuiApplication::clipboard()->setText(step.value(QStringLiteral("text")).toString().repeated(step.value(QStringLiteral("repeat")).toInt(1)));
                return next(200);
            }
            if (op == QLatin1String("chmod")) {
                const QString path = QDir::home().filePath(step.value(QStringLiteral("path")).toString());
                bool ok = false;
                const uint mode = step.value(QStringLiteral("mode")).toString().toUInt(&ok, 8);
                QFileDevice::Permissions permissions;
                const QFileDevice::Permission bits[] = {QFileDevice::ExeOther, QFileDevice::WriteOther, QFileDevice::ReadOther,
                    QFileDevice::ExeGroup, QFileDevice::WriteGroup, QFileDevice::ReadGroup,
                    QFileDevice::ExeOwner, QFileDevice::WriteOwner, QFileDevice::ReadOwner};
                for (int bit = 0; bit < 9; ++bit) {
                    if (mode & (1u << bit)) {
                        permissions |= bits[bit];
                    }
                }
                if (!ok || !QFile::setPermissions(path, permissions)) {
                    return fail(QStringLiteral("chmod failed: ") + path);
                }
                return next();
            }
            if (op == QLatin1String("loadLayout")) {
                MainWindow* mainWindow = window();
                if (!mainWindow) {
                    return fail(QStringLiteral("loadLayout: no window"));
                }
                const QString path = QDir::home().filePath(step.value(QStringLiteral("file")).toString());
                // Deferred: an unreadable layout opens a modal message box.
                QTimer::singleShot(0, mainWindow->viewManager(), [manager = QPointer<ViewManager>(mainWindow->viewManager()), path] {
                    if (manager) {
                        manager->loadLayout(path);
                    }
                });
                return next(800);
            }
            if (op == QLatin1String("quit")) {
                sonicInfo() << "parity: quit";
                qDeleteAll(_backdrops);
                QCoreApplication::exit(0);
                return;
            }
            fail(QStringLiteral("unknown op ") + op);
        }

        QList<QJsonObject> _steps;
        QList<QPointer<MainWindow>> _windows;
        QList<Backdrop*> _backdrops;
        bool _failed = false;
        bool _checkWidgets = false;
        QString _targetTitle;
        QString _targetPanel;
        // QML dialogs already shown when the last click happened: not the dialog it opens.
        QList<QPointer<QWindow>> _shownBeforeClick;
        int _index = 0;
        int _currentWindow = 0;
        // The QML window's mouse event timestamps.
        ulong _mouseTime = 0;
        QString _output = QDir::currentPath();
    };
}

QCommandLineOption commandLineOption()
{
    return QCommandLineOption(QStringLiteral("parity-script"), QStringLiteral("Run a parity test script (testing only)."), QStringLiteral("file"));
}

void start(const QString& scriptPath)
{
    QApplication::setCursorFlashTime(0);
    auto* runner = new Runner(scriptPath);
    runner->setParent(qApp);
    runner->next();
}
}
