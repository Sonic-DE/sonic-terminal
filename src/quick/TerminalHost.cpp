/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "TerminalHost.h"

#include "CompatWindows.h"
#include "MainWindow.h"
#include "PanelOpen.h"
#include "ShortcutStore.h"
#include "ViewManager.h"
#include "WindowController.h"
#include "WindowSystemInfo.h"
#include "profile/ProfileManager.h"
#include "session/Session.h"
#include "session/SessionController.h"

#include <Sonic/Ipc/Connection>
#include <Sonic/Ipc/Server>
#include <Sonic/Logging/Log>

#include <KActionCollection>
#include <KStandardAction>
#include <KX11Extras>

#include <QApplication>
#include <QMenuBar>
#include <QQuickWindow>
#include <QToolBar>

namespace Konsole {
namespace {
    bool s_creatingWindow = false;

    Profile::Ptr profileNamed(const QString& name)
    {
        const auto profiles = ProfileManager::instance()->allProfiles();
        for (const Profile::Ptr& candidate : profiles) {
            if (candidate->name() == name) {
                return candidate;
            }
        }
        return ProfileManager::instance()->defaultProfile();
    }
}

TerminalHost::TerminalHost(QQuickItem* parent)
    : QQuickItem(parent)
{
    // Its terminal's focus counts as its own: that's when its shortcuts apply.
    setFlag(ItemIsFocusScope);
    setFiltersChildMouseEvents(true);
    CompatWindows::setHostedProcess();
}

bool TerminalHost::childMouseEventFilter(QQuickItem* item, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        Q_EMIT pressedInside();
    }
    return QQuickItem::childMouseEventFilter(item, event);
}

TerminalHost::~TerminalHost()
{
    if (_window) {
        disconnect(_window, nullptr, this, nullptr);
        delete _window;
    }
}

bool TerminalHost::creatingWindow()
{
    return s_creatingWindow;
}

void TerminalHost::componentComplete()
{
    QQuickItem::componentComplete();
    if (!qobject_cast<QApplication*>(QCoreApplication::instance())) {
        sonicCritical() << "a terminal needs a QApplication in its host";
        return;
    }
    if (_fullTerminal) {
        CompatWindows::setFullTerminalHost();
        // The profile's opacity: what is behind the item shows through.
        WindowSystemInfo::HAVE_TRANSPARENCY = true;
    }
    s_creatingWindow = true;
    _window = new MainWindow();
    s_creatingWindow = false;
    connect(new WindowController(_window, this, _fullTerminal), &WindowController::blurChanged, this, &TerminalHost::blurChanged);
    // The session ended: the window closes itself (ViewManager::empty, WA_DeleteOnClose).
    connect(_window, &QObject::destroyed, this, [this] {
        // A widget says so before ~QObject clears the pointer.
        _window.clear();
        updateServer();
        Q_EMIT compatWindowChanged();
        Q_EMIT finished();
    });
    // KMainWindow holds a QEventLoopLocker while it exists; it mustn't keep the host running.
    connect(qApp, &QGuiApplication::lastWindowClosed, this, [this] {
        if (_window && QGuiApplication::quitOnLastWindowClosed()) {
            disconnect(_window, nullptr, this, nullptr);
            delete _window;
            Q_EMIT compatWindowChanged();
        }
    });
    ViewManager* manager = _window->viewManager();
    if (!_fullTerminal) {
        manager->setNavigationMethod(ViewManager::NoNavigation);
        manager->setNavigationVisibility(ViewManager::AlwaysHideNavigation);
    }
    connect(manager, &ViewManager::activeViewChanged, this, &TerminalHost::activeViewChanged);
    _window->createSession(profileNamed(_profile), QString());
    // Last: plugging the session's client can bring a toolbar.
    _window->menuBar()->hide();
    const auto toolBars = _window->findChildren<QToolBar*>();
    for (QToolBar* toolBar : toolBars) {
        toolBar->setVisible(_fullTerminal);
    }
    if (_fullTerminal) {
        setUpFullTerminal();
    }
    _window->show();
    connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, &TerminalHost::checkOwnWindows);
    sonicDebug() << "terminal hosted in" << QCoreApplication::applicationName();
    updateServer();
    Q_EMIT compatWindowChanged();
}

void TerminalHost::setUpFullTerminal()
{
    // They need the terminal application or its own window, or set up what a host doesn't show.
    KActionCollection* collection = _window->actionCollection();
    const QStringList hidden{QStringLiteral("new-window"),
        QStringLiteral("close-window"),
        QStringLiteral("detach-tab"),
        QStringLiteral("detach-view"),
        QStringLiteral("options_show_toolbar"),
        KStandardAction::name(KStandardAction::ShowMenubar),
        KStandardAction::name(KStandardAction::FullScreen),
        KStandardAction::name(KStandardAction::ConfigureToolbars)};
    for (const QString& name : hidden) {
        if (QAction* action = collection->action(name)) {
            action->setVisible(false);
            action->setEnabled(false);
        }
    }
    if (QAction* toolBars = _window->toolBarMenuAction()) {
        toolBars->setVisible(false);
    }
    placeToolBars();
}

void TerminalHost::placeToolBars()
{
    if (!_window || !_fullTerminal) {
        return;
    }
    const auto toolBars = _window->findChildren<QToolBar*>();
    for (QToolBar* toolBar : toolBars) {
        toolBar->setMovable(false);
        _window->addToolBar(_toolBarsAtBottom ? Qt::BottomToolBarArea : Qt::TopToolBarArea, toolBar);
        // Adding it again hides it.
        toolBar->show();
    }
}

bool TerminalHost::toolBarsAtBottom() const
{
    return _toolBarsAtBottom;
}

void TerminalHost::setToolBarsAtBottom(bool atBottom)
{
    if (atBottom != _toolBarsAtBottom) {
        _toolBarsAtBottom = atBottom;
        placeToolBars();
        Q_EMIT toolBarsAtBottomChanged();
    }
}

QObject* TerminalHost::compatWindow() const
{
    return _window;
}

QObject* TerminalHost::controller() const
{
    return _window ? WindowController::controllerOf(_window) : nullptr;
}

bool TerminalHost::ownWindowOpen()
{
    checkOwnWindows();
    return _ownWindowOpen;
}

void TerminalHost::checkOwnWindows()
{
    const QWindow* shown = window();
    const QWindow* compat = _window ? _window->windowHandle() : nullptr;
    bool open = false;
    bool dialog = false;
    // A toolbar's menu has no parent: QMenu drops the transient parent ChromeAdapter gave it. A
    // popup in this process is modal anyway, so it came from what has the focus.
    if (const QWidget* popup = shown && shown->flags().testFlag(Qt::WindowDoesNotAcceptFocus) ? nullptr : QApplication::activePopupWidget()) {
        if (QWindow* handle = popup->windowHandle()) {
            open = true;
            connect(handle, &QWindow::visibleChanged, this, &TerminalHost::checkOwnWindows, Qt::ConnectionType(Qt::UniqueConnection | Qt::QueuedConnection));
        }
    }
    const QWindowList windows = QGuiApplication::topLevelWindows();
    for (QWindow* candidate : windows) {
        if (candidate == shown || candidate == compat || !candidate->isVisible()) {
            continue;
        }
        for (const QWindow* parent = candidate->transientParent(); parent; parent = parent->transientParent()) {
            if (parent == shown || parent == compat) {
                open = true;
                dialog = dialog || !candidate->flags().testFlag(Qt::Popup);
                // Queued: what hides it isn't done with it, and the focus moves after.
                connect(candidate, &QWindow::visibleChanged, this, &TerminalHost::checkOwnWindows, Qt::ConnectionType(Qt::UniqueConnection | Qt::QueuedConnection));
                break;
            }
        }
    }
    const bool closedDialog = !open && _ownDialogOpen;
    _ownDialogOpen = dialog;
    if (open == _ownWindowOpen) {
        return;
    }
    _ownWindowOpen = open;
    if (!open) {
        // The window manager picks another window when a dialog closes; a menu leaves the active
        // window alone unless something else was clicked.
        const WId active = KX11Extras::activeWindow();
        Q_EMIT ownWindowsClosed(closedDialog || !active || (shown && active == shown->winId()));
    }
}

bool TerminalHost::blur() const
{
    const WindowController* windowController = _window ? WindowController::controllerOf(_window) : nullptr;
    return windowController && windowController->blur();
}

bool TerminalHost::servesNewTerminals() const
{
    return _servesNewTerminals;
}

void TerminalHost::setServesNewTerminals(bool serves)
{
    if (_servesNewTerminals == serves) {
        return;
    }
    _servesNewTerminals = serves;
    updateServer();
    Q_EMIT servesNewTerminalsChanged();
}

void TerminalHost::updateServer()
{
    if (!_servesNewTerminals || !_window) {
        delete _server;
        return;
    }
    if (_server) {
        return;
    }
    const QString path = PanelOpen::socketPath();
    auto* server = new Sonic::Ipc::Server(this);
    if (path.isEmpty() || !server->listen(path, Sonic::Ipc::Server::Access::User, PanelOpen::protocolVersion, PanelOpen::maxPayload)) {
        sonicInfo() << "not serving new terminals:" << server->errorString();
        delete server;
        return;
    }
    connect(server, &Sonic::Ipc::Server::newConnection, this, [this](Sonic::Ipc::Connection* connection) {
        connect(connection, &Sonic::Ipc::Connection::frameReceived, this, [this, connection](quint32 type, const QByteArray& payload) {
            if (type == PanelOpen::OpenTab && _window) {
                openNewTerminal(QString::fromUtf8(payload));
                connection->send(PanelOpen::Opened, {});
            }
        });
    });
    _server = server;
}

void TerminalHost::openNewTerminal(const QString& directory)
{
    // Not through createSession's directory: that one follows the profile's "current directory" setting.
    Session* session = _window->createSession(profileNamed(_profile), QString());
    if (!directory.isEmpty()) {
        session->setInitialWorkingDirectory(directory);
    }
    Q_EMIT newTerminalOpened();
}

void TerminalHost::configureTerminal()
{
    if (QAction* action = _controller ? _controller->actionCollection()->action(QStringLiteral("edit-current-profile")) : nullptr) {
        action->trigger();
    }
}

void TerminalHost::activeViewChanged(SessionController* controller)
{
    if (_controller) {
        disconnect(_controller, &SessionController::currentDirectoryChanged, this, &TerminalHost::currentDirectoryChanged);
    }
    _controller = controller;
    if (controller) {
        connect(controller, &SessionController::currentDirectoryChanged, this, &TerminalHost::currentDirectoryChanged);
    }
}

Session* TerminalHost::activeSession() const
{
    return _controller ? _controller->session().data() : nullptr;
}

void TerminalHost::showShellInDir(const QString& dir)
{
    Session* session = activeSession();
    // do nothing if the session has already started running
    if (!session || session->isRunning()) {
        return;
    }
    // All other checking is done in setInitialWorkingDirectory()
    if (!dir.isEmpty()) {
        session->setInitialWorkingDirectory(dir);
    }
    session->run();
}

void TerminalHost::sendInput(const QString& text)
{
    if (Session* session = activeSession()) {
        session->sendTextToTerminal(text);
    }
}

QString TerminalHost::currentWorkingDirectory() const
{
    Session* session = activeSession();
    return session ? session->currentWorkingDirectory() : QString();
}

int TerminalHost::foregroundProcessId() const
{
    Session* session = activeSession();
    return session && session->isForegroundProcessActive() ? session->foregroundProcessId() : -1;
}

QString TerminalHost::foregroundProcessName() const
{
    Session* session = activeSession();
    return session && session->isForegroundProcessActive() ? session->foregroundProcessName() : QString();
}

void TerminalHost::setContextMenuAdditionalActions(const QList<QAction*>& actions)
{
    if (_window) {
        _window->viewManager()->setContextMenuAdditionalActions(actions);
    }
}

QObject* TerminalHost::sessionActionCollection() const
{
    return _controller ? _controller->actionCollection() : nullptr;
}

void TerminalHost::saveShortcuts()
{
    if (_controller) {
        ShortcutStore::instance()->save({_controller->actionCollection()});
    }
}
}

#include "moc_TerminalHost.cpp"
