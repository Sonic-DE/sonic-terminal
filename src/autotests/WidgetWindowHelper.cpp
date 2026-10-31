/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// A widget MainWindow (the window before the QML one) for QmlWindowDBusTest:
// - save <session id> <session key> <profile> <dir> <dir>: opens two tabs in those
//   directories, splits the second, places the window and saves session-management data
//   through KMWSessionManager::saveState()'s calls, as at logout;
// - serve <profile> <dir>: shows one tab and stays on the session bus until terminated.

#include "../MainWindow.h"
#include "../ViewManager.h"
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
#include <QDBusConnection>
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

// A shell starts once its terminal is shown.
bool shellsRunning()
{
    const QList<Session*> sessions = SessionManager::instance()->sessions();
    return waitFor([&sessions] {
        return std::all_of(sessions.cbegin(), sessions.cend(), [](Session* session) {
            return session->isRunning() && !session->currentWorkingDirectory().isEmpty();
        });
    });
}

MainWindow* showWindow(const Profile::Ptr& profile, const QString& directory)
{
    auto* window = new MainWindow();
    window->createSession(profile, directory);
    window->resize(700, 500);
    window->show();
    if (!QTest::qWaitForWindowExposed(window) || !shellsRunning()) {
        qCritical() << "window or shell didn't start";
        return nullptr;
    }
    return window;
}

int save(const QStringList& args, const Profile::Ptr& profile)
{
    MainWindow* window = showWindow(profile, args.at(5));
    if (!window) {
        return 1;
    }
    window->createSession(profile, args.at(6));
    window->actionCollection()->action(QStringLiteral("split-view-left-right"))->trigger();
    // The new tab's terminals may have grown the window.
    const QRect placed(120, 90, 700, 500);
    window->setGeometry(placed);
    if (!shellsRunning() || !waitFor([window, placed] {
            return window->windowHandle()->geometry() == placed;
        })) {
        qCritical() << "shells not running or window not placed";
        return 1;
    }

    KConfigGui::setSessionConfig(args.at(2), args.at(3));
    KConfig* config = KConfigGui::sessionConfig();
    (window->*(&SessionSaver::saveGlobalProperties))(config);
    (window->*(&SessionSaver::savePropertiesInternal))(config, 1);
    KConfigGroup(config, QStringLiteral("Number")).writeEntry("NumberOfWindows", 1);
    config->sync();
    // As the window's closeEvent() after the save at logout.
    KSharedConfig::openStateConfig()->group(QStringLiteral("MainWindow")).deleteEntry("RestorePositionForNextInstance");
    KSharedConfig::openStateConfig()->sync();

    QTextStream out(stdout);
    const QList<Session*> sessions = SessionManager::instance()->sessions();
    for (Session* session : sessions) {
        out << "directory " << session->currentWorkingDirectory() << '\n';
    }
    const QStringList hierarchy = window->viewManager()->viewHierarchy();
    for (const QString& line : hierarchy) {
        out << "hierarchy " << line << '\n';
    }
    const QRect geometry = window->windowHandle()->geometry();
    out << "window " << geometry.x() << ' ' << geometry.y() << ' ' << geometry.width() << ' ' << geometry.height() << '\n';
    delete window;
    return 0;
}

int serve(QApplication& app, const QStringList& args, const Profile::Ptr& profile)
{
    if (!showWindow(profile, args.at(3))) {
        return 1;
    }
    QTextStream(stdout) << "ready " << QDBusConnection::sessionBus().baseService() << Qt::endl;
    return app.exec();
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    // As konsole's KAboutData: D-Bus interface names and paths derive from both.
    QCoreApplication::setApplicationName(QStringLiteral("konsole"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    const QStringList args = app.arguments();
    const bool saving = args.value(1) == QLatin1String("save") && args.size() == 7;
    const bool serving = args.value(1) == QLatin1String("serve") && args.size() == 4;
    if (!saving && !serving) {
        qCritical() << "usage: WidgetWindowHelper save <session id> <session key> <profile> <dir> <dir>"
                    << "| serve <profile> <dir>";
        return 2;
    }
    const QString profileName = saving ? args.at(4) : args.at(2);
    const Profile::Ptr profile = ProfileManager::instance()->loadProfile(profileName);
    if (!profile) {
        qCritical() << "no profile" << profileName;
        return 1;
    }
    return saving ? save(args, profile) : serve(app, args, profile);
}
