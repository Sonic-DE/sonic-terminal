/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Writes session-management data as the widget window saves it at logout, for
// QmlWindowDBusTest: SessionFileWriter <session id> <session key> <profile> <dir> <dir>
// opens two tabs in those directories, splits the second, places the window and saves
// through KMWSessionManager::saveState()'s calls.

#include "../MainWindow.h"
#include "../profile/ProfileManager.h"
#include "../session/Session.h"
#include "../session/SessionManager.h"

#include <KActionCollection>
#include <KConfig>
#include <KConfigGroup>
#include <KConfigGui>
#include <KSharedConfig>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QTest>
#include <QTextStream>

#include <algorithm>
#include <functional>

using namespace Konsole;

namespace {
// KMainWindow's session hooks are protected; KMWSessionManager calls them.
struct SessionSaver : KMainWindow {
    using KMainWindow::saveGlobalProperties;
    using KMainWindow::savePropertiesInternal;
};

bool waitFor(const std::function<bool()>& condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > 10000) {
            return false;
        }
        QTest::qWait(50);
    }
    return true;
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("konsole"));
    const QStringList args = app.arguments();
    if (args.size() != 6) {
        qCritical() << "usage: SessionFileWriter <session id> <session key> <profile> <dir> <dir>";
        return 2;
    }
    const Profile::Ptr profile = ProfileManager::instance()->loadProfile(args.at(3));
    if (!profile) {
        qCritical() << "no profile" << args.at(3);
        return 1;
    }

    // A shell starts once its terminal is shown, so each tab is opened in turn.
    auto* window = new MainWindow();
    window->createSession(profile, args.at(4));
    window->setGeometry(120, 90, 700, 500);
    window->show();
    if (!QTest::qWaitForWindowExposed(window)) {
        qCritical() << "window not exposed";
        return 1;
    }
    const auto running = [] {
        const QList<Session*> sessions = SessionManager::instance()->sessions();
        return waitFor([&sessions] {
            return std::all_of(sessions.cbegin(), sessions.cend(), [](Session* session) {
                return session->isRunning() && !session->currentWorkingDirectory().isEmpty();
            });
        });
    };
    if (!running()) {
        qCritical() << "first shell not running";
        return 1;
    }
    window->createSession(profile, args.at(5));
    window->actionCollection()->action(QStringLiteral("split-view-left-right"))->trigger();
    if (!running()) {
        qCritical() << "shells not running";
        return 1;
    }
    const QList<Session*> sessions = SessionManager::instance()->sessions();

    KConfigGui::setSessionConfig(args.at(1), args.at(2));
    KConfig* config = KConfigGui::sessionConfig();
    (window->*(&SessionSaver::saveGlobalProperties))(config);
    (window->*(&SessionSaver::savePropertiesInternal))(config, 1);
    KConfigGroup(config, QStringLiteral("Number")).writeEntry("NumberOfWindows", 1);
    config->sync();
    // As the window's closeEvent() after the save at logout.
    KSharedConfig::openStateConfig()->group(QStringLiteral("MainWindow")).deleteEntry("RestorePositionForNextInstance");
    KSharedConfig::openStateConfig()->sync();

    QTextStream out(stdout);
    for (Session* session : sessions) {
        out << "directory " << session->currentWorkingDirectory() << '\n';
    }
    const QRect geometry = window->windowHandle()->geometry();
    out << "window " << geometry.x() << ' ' << geometry.y() << ' ' << geometry.width() << ' ' << geometry.height() << '\n';
    delete window;
    return 0;
}
