/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "TerminalHost.h"

#include "MainWindow.h"
#include "ShortcutStore.h"
#include "ViewManager.h"
#include "WindowController.h"
#include "profile/ProfileManager.h"
#include "session/Session.h"
#include "session/SessionController.h"

#include <Sonic/Logging/Log>

#include <KActionCollection>

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>

namespace Konsole {
namespace {
    bool s_creatingWindow = false;
}

TerminalHost::TerminalHost(QQuickItem* parent)
    : QQuickItem(parent)
{
    // Its terminal's focus counts as its own: that's when its shortcuts apply.
    setFlag(ItemIsFocusScope);
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
    s_creatingWindow = true;
    _window = new MainWindow();
    s_creatingWindow = false;
    new WindowController(_window, this);
    // The session ended: the window closes itself (ViewManager::empty, WA_DeleteOnClose).
    connect(_window, &QObject::destroyed, this, [this] {
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
    manager->setNavigationMethod(ViewManager::NoNavigation);
    manager->setNavigationVisibility(ViewManager::AlwaysHideNavigation);
    connect(manager, &ViewManager::activeViewChanged, this, &TerminalHost::activeViewChanged);
    _window->createSession(ProfileManager::instance()->defaultProfile(), QString());
    // Last: plugging the session's client can bring a toolbar.
    _window->menuBar()->hide();
    const auto toolBars = _window->findChildren<QToolBar*>();
    for (QToolBar* toolBar : toolBars) {
        toolBar->hide();
    }
    _window->show();
    sonicDebug() << "terminal hosted in" << QCoreApplication::applicationName();
    Q_EMIT compatWindowChanged();
}

QObject* TerminalHost::compatWindow() const
{
    return _window;
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
