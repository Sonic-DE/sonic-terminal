/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Scripting and session restore of the QML window, against konsole processes on a private
// X server and session bus: both D-Bus service names answer, /konsole/MainWindow_1 is the
// widget window's object (WidgetWindowHelper) with the QML window's id, and a session the
// widget window saved restores as it was saved.

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <utility>
#include <vector>

namespace {
const QString profileName = QStringLiteral("dbus-test");

struct XWindow {
    bool viewable = false;
    QRect geometry;
};

// What the window shows after a restore.
struct Restored {
    int sessions = 0;
    QStringList hierarchy;
    QStringList titles;
    QRect geometry;
};
}

class QmlWindowDBusTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    void testServiceNames();
    void testMainWindowObject();
    void testSessionRestore();

private:
    struct Konsole {
        std::unique_ptr<QProcess> process;
        QString service;
        bool helper = false;
    };

    // Both quit at the test's cleanup unless before.
    Konsole& start(const QStringList& arguments = {});
    Konsole& startWidgetWindow();
    void quit(Konsole& konsole);
    XWindow xwindow(qlonglong id);
    Restored restored(const Konsole& konsole);
    QString introspect(const Konsole& konsole, const QString& path);

    QTemporaryDir _home;
    // Outside the home, so tab titles show them whole.
    QTemporaryDir _directories;
    QProcess _xvfb;
    QProcessEnvironment _environment;
    std::vector<std::unique_ptr<Konsole>> _running;
};

void QmlWindowDBusTest::initTestCase()
{
    QVERIFY(QDBusConnection::sessionBus().isConnected());
    QVERIFY(_home.isValid());
    _xvfb.start(QStringLiteral(XVFB_EXECUTABLE),
        {QStringLiteral("-displayfd"), QStringLiteral("1"), QStringLiteral("-screen"), QStringLiteral("0"), QStringLiteral("1024x768x24"), QStringLiteral("-ac"),
            QStringLiteral("-nolisten"), QStringLiteral("tcp")});
    QVERIFY2(_xvfb.waitForStarted(), qPrintable(_xvfb.errorString()));
    QVERIFY2(_xvfb.waitForReadyRead(10000), qPrintable(QString::fromLocal8Bit(_xvfb.readAllStandardError())));
    const QString display = QStringLiteral(":%1").arg(QString::fromLatin1(_xvfb.readLine()).trimmed());

    // Nothing reaches the desktop: a private home, display and session bus.
    const QProcessEnvironment system = QProcessEnvironment::systemEnvironment();
    for (const char* name : {"PATH", "LD_LIBRARY_PATH", "QT_PLUGIN_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH", "XDG_DATA_DIRS", "LANG", "DBUS_SESSION_BUS_ADDRESS"}) {
        if (system.contains(QLatin1String(name))) {
            _environment.insert(QLatin1String(name), system.value(QLatin1String(name)));
        }
    }
    const QString home = _home.path();
    for (const auto& [name, dir] : {std::pair{"XDG_CONFIG_HOME", ".config"}, {"XDG_DATA_HOME", ".local/share"}, {"XDG_STATE_HOME", ".local/state"},
             {"XDG_CACHE_HOME", ".cache"}, {"XDG_RUNTIME_DIR", "runtime"}}) {
        QVERIFY(QDir(home).mkpath(QLatin1String(dir)));
        _environment.insert(QLatin1String(name), home + u'/' + QLatin1String(dir));
    }
    QVERIFY(QFile::setPermissions(home + QStringLiteral("/runtime"), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    _environment.insert(QStringLiteral("HOME"), home);
    _environment.insert(QStringLiteral("DISPLAY"), display);
    _environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("xcb"));
    _environment.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
    _environment.insert(QStringLiteral("QT_NO_XDG_DESKTOP_PORTAL"), QStringLiteral("1"));
    _environment.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), QStringLiteral("unix:path=") + home + QStringLiteral("/no-system-bus"));

    // The tab title is the session's directory.
    QVERIFY(QDir(home).mkpath(QStringLiteral(".local/share/konsole")));
    QFile profile(home + QStringLiteral("/.local/share/konsole/") + profileName + QStringLiteral(".profile"));
    QVERIFY(profile.open(QIODevice::WriteOnly));
    profile.write("[General]\nName=" + profileName.toUtf8() + "\nCommand=/bin/sh\nLocalTabTitleFormat=%D\n");
    profile.close();
    QFile rc(home + QStringLiteral("/.config/konsolerc"));
    QVERIFY(rc.open(QIODevice::WriteOnly));
    rc.write("[Desktop Entry]\nDefaultProfile=" + profileName.toUtf8() + ".profile\n");
}

void QmlWindowDBusTest::cleanupTestCase()
{
    _xvfb.terminate();
    if (!_xvfb.waitForFinished(3000)) {
        _xvfb.kill();
        _xvfb.waitForFinished();
    }
}

void QmlWindowDBusTest::cleanup()
{
    for (const auto& konsole : _running) {
        quit(*konsole);
    }
    _running.clear();
}

QmlWindowDBusTest::Konsole& QmlWindowDBusTest::startWidgetWindow()
{
    Konsole& konsole = *_running.emplace_back(std::make_unique<Konsole>());
    konsole.helper = true;
    konsole.process = std::make_unique<QProcess>();
    konsole.process->setProcessEnvironment(_environment);
    konsole.process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    konsole.process->start(QStringLiteral(WIDGET_WINDOW_HELPER), {QStringLiteral("serve"), profileName + QStringLiteral(".profile"), _home.path()});
    if (konsole.process->waitForReadyRead(20000)) {
        const QString line = QString::fromLatin1(konsole.process->readLine()).trimmed();
        if (line.startsWith(QLatin1String("ready "))) {
            konsole.service = line.section(u' ', 1);
        }
    }
    return konsole;
}

QmlWindowDBusTest::Konsole& QmlWindowDBusTest::start(const QStringList& arguments)
{
    Konsole& konsole = *_running.emplace_back(std::make_unique<Konsole>());
    konsole.process = std::make_unique<QProcess>();
    konsole.process->setProcessEnvironment(_environment);
    konsole.process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    konsole.process->start(QStringLiteral(KONSOLE_EXECUTABLE), QStringList{QStringLiteral("--separate")} + arguments);
    if (!konsole.process->waitForStarted()) {
        return konsole;
    }
    const QString service = QStringLiteral("org.kde.konsole-%1").arg(konsole.process->processId());
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 20000 && konsole.process->state() == QProcess::Running) {
        QDBusInterface window(service, QStringLiteral("/Windows/1"), QStringLiteral("org.kde.konsole.Window"));
        const QDBusReply<QStringList> sessions = window.isValid() ? window.call(QStringLiteral("sessionList")) : QDBusReply<QStringList>();
        if (sessions.isValid() && !sessions.value().isEmpty()) {
            konsole.service = service;
            break;
        }
        QTest::qWait(50);
    }
    return konsole;
}

void QmlWindowDBusTest::quit(Konsole& konsole)
{
    if (konsole.helper && konsole.process) {
        konsole.process->terminate();
    } else if (!konsole.service.isEmpty()) {
        QDBusInterface(konsole.service, QStringLiteral("/MainApplication"), QStringLiteral("org.qtproject.Qt.QCoreApplication")).asyncCall(QStringLiteral("quit"));
        konsole.service.clear();
    }
    if (konsole.process && !konsole.process->waitForFinished(5000)) {
        konsole.process->kill();
        konsole.process->waitForFinished();
    }
}

XWindow QmlWindowDBusTest::xwindow(qlonglong id)
{
    QProcess xwininfo;
    xwininfo.setProcessEnvironment(_environment);
    xwininfo.start(QStringLiteral(XWININFO_EXECUTABLE), {QStringLiteral("-id"), QString::number(id)});
    xwininfo.waitForFinished();
    const QString text = QString::fromLocal8Bit(xwininfo.readAllStandardOutput());
    const auto value = [&text](const char* label) {
        return QRegularExpression(QLatin1String(label) + QStringLiteral(":\\s*(-?\\d+)")).match(text).captured(1).toInt();
    };
    return {text.contains(QLatin1String("Map State: IsViewable")),
        QRect(value("Absolute upper-left X"), value("Absolute upper-left Y"), value("Width"), value("Height"))};
}

QString QmlWindowDBusTest::introspect(const Konsole& konsole, const QString& path)
{
    const QDBusMessage reply = QDBusConnection::sessionBus().call(
        QDBusMessage::createMethodCall(konsole.service, path, QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect")));
    return reply.arguments().value(0).toString();
}

Restored QmlWindowDBusTest::restored(const Konsole& konsole)
{
    Restored result;
    QDBusInterface window(konsole.service, QStringLiteral("/Windows/1"), QStringLiteral("org.kde.konsole.Window"));
    result.sessions = QDBusReply<int>(window.call(QStringLiteral("sessionCount"))).value();
    result.hierarchy = QDBusReply<QStringList>(window.call(QStringLiteral("viewHierarchy"))).value();
    const QStringList sessions = QDBusReply<QStringList>(window.call(QStringLiteral("sessionList"))).value();
    for (const QString& id : sessions) {
        QDBusInterface session(konsole.service, QStringLiteral("/Sessions/") + id, QStringLiteral("org.kde.konsole.Session"));
        result.titles.append(QDBusReply<QString>(session.call(QStringLiteral("title"), 1)).value());
    }
    result.titles.sort();
    QDBusInterface mainWindow(konsole.service, QStringLiteral("/konsole/MainWindow_1"), QStringLiteral("org.kde.KMainWindow"));
    result.geometry = xwindow(QDBusReply<qlonglong>(mainWindow.call(QStringLiteral("winId"))).value()).geometry;
    return result;
}

void QmlWindowDBusTest::testServiceNames()
{
    Konsole& konsole = start();
    QVERIFY(!konsole.service.isEmpty());
    const qint64 pid = konsole.process->processId();
    const QString owner = QDBusConnection::sessionBus().interface()->serviceOwner(konsole.service).value();
    for (const auto& [service, prefix] : {std::pair{QStringLiteral("org.kde.konsole-%1").arg(pid), QStringLiteral("org.kde.konsole")},
             std::pair{QStringLiteral("org.sonicde.terminal-%1").arg(pid), QStringLiteral("org.sonicde.terminal")}}) {
        QCOMPARE(QDBusConnection::sessionBus().interface()->serviceOwner(service).value(), owner);
        QDBusInterface window(service, QStringLiteral("/Windows/1"), prefix + QStringLiteral(".Window"));
        const QDBusReply<int> newSession = window.call(QStringLiteral("newSession"));
        QVERIFY2(newSession.isValid(), qPrintable(service + u' ' + newSession.error().message()));
        QVERIFY(newSession.value() > 1);
        QDBusInterface session(service, QStringLiteral("/Sessions/1"), prefix + QStringLiteral(".Session"));
        const QDBusReply<QString> title = session.call(QStringLiteral("title"), 0);
        QVERIFY2(title.isValid(), qPrintable(service + u' ' + title.error().message()));
    }
    // The shell is told both names' variables.
    QDBusInterface session(konsole.service, QStringLiteral("/Sessions/1"), QStringLiteral("org.kde.konsole.Session"));
    const QStringList environment = QDBusReply<QStringList>(session.call(QStringLiteral("environment"))).value();
    QVERIFY(environment.contains(QStringLiteral("SONIC_TERMINAL_DBUS_SERVICE=") + owner));
    QVERIFY(environment.contains(QStringLiteral("SONIC_TERMINAL_DBUS_SESSION=/Sessions/1")));
    QVERIFY(environment.contains(QStringLiteral("KONSOLE_DBUS_SESSION=/Sessions/1")));
}

void QmlWindowDBusTest::testMainWindowObject()
{
    Konsole& widget = startWidgetWindow();
    Konsole& qml = start();
    QVERIFY(!widget.service.isEmpty());
    QVERIFY(!qml.service.isEmpty());

    const QString path = QStringLiteral("/konsole/MainWindow_1");
    const QString widgetXml = introspect(widget, path);
    QVERIFY(widgetXml.contains(QLatin1String("<interface name=\"org.kde.KMainWindow\">")));
    QCOMPARE(introspect(qml, path), widgetXml);
    QDBusInterface widgetWindow(widget.service, path, QStringLiteral("org.kde.KMainWindow"));
    QDBusInterface mainWindow(qml.service, path, QStringLiteral("org.kde.KMainWindow"));
    // konsole's KAboutData also gives the help menu "Report Bug" and "Donate", which the
    // helper (no KAboutData) lacks.
    const QStringList qmlActions = QDBusReply<QStringList>(mainWindow.call(QStringLiteral("actions"))).value();
    const QStringList widgetActions = QDBusReply<QStringList>(widgetWindow.call(QStringLiteral("actions"))).value();
    QVERIFY(!widgetActions.isEmpty());
    // ZModem is dropped.
    QVERIFY(!qmlActions.contains(QStringLiteral("zmodem-upload")));
    for (const QString& action : widgetActions) {
        QVERIFY2(qmlActions.contains(action), qPrintable(action));
    }

    const QDBusReply<qlonglong> winId = mainWindow.call(QStringLiteral("winId"));
    QVERIFY2(winId.isValid(), qPrintable(winId.error().message()));
    // The window on screen, which the shell gets as WINDOWID.
    QTRY_VERIFY(xwindow(winId.value()).viewable);
    QDBusInterface session(qml.service, QStringLiteral("/Sessions/1"), QStringLiteral("org.kde.konsole.Session"));
    const QStringList environment = QDBusReply<QStringList>(session.call(QStringLiteral("environment"))).value();
    QVERIFY2(environment.contains(QStringLiteral("WINDOWID=%1").arg(winId.value())), qPrintable(environment.join(u' ')));

    QDBusInterface window(qml.service, QStringLiteral("/Windows/1"), QStringLiteral("org.kde.konsole.Window"));
    const int sessions = QDBusReply<int>(window.call(QStringLiteral("sessionCount"))).value();
    QVERIFY(QDBusReply<bool>(mainWindow.call(QStringLiteral("actionIsEnabled"), QStringLiteral("new-tab"))).value());
    QVERIFY(QDBusReply<bool>(mainWindow.call(QStringLiteral("activateAction"), QStringLiteral("new-tab"))).value());
    QTRY_COMPARE(QDBusReply<int>(window.call(QStringLiteral("sessionCount"))).value(), sessions + 1);
}

void QmlWindowDBusTest::testSessionRestore()
{
    QVERIFY(_directories.isValid());
    const QStringList directories{_directories.filePath(QStringLiteral("tab-one")), _directories.filePath(QStringLiteral("tab-two"))};
    for (const QString& directory : directories) {
        QVERIFY(QDir().mkpath(directory));
    }
    QProcess writer;
    writer.setProcessEnvironment(_environment);
    writer.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    writer.start(QStringLiteral(WIDGET_WINDOW_HELPER),
        QStringList{QStringLiteral("save"), QStringLiteral("restoretest"), QStringLiteral("1"), profileName + QStringLiteral(".profile")} + directories);
    QVERIFY(writer.waitForFinished(30000));
    QCOMPARE(writer.exitCode(), 0);
    QStringList savedDirectories;
    QStringList savedHierarchy;
    QRect savedGeometry;
    const QStringList lines = QString::fromLocal8Bit(writer.readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        qInfo() << "saved" << line;
        const QStringList words = line.split(u' ');
        if (words.constFirst() == QLatin1String("directory")) {
            savedDirectories.append(line.section(u' ', 1));
        } else if (words.constFirst() == QLatin1String("hierarchy")) {
            savedHierarchy.append(line.section(u' ', 1));
        } else if (words.constFirst() == QLatin1String("window") && words.size() == 5) {
            savedGeometry = QRect(words.at(1).toInt(), words.at(2).toInt(), words.at(3).toInt(), words.at(4).toInt());
        }
    }
    savedDirectories.sort();
    QCOMPARE(savedDirectories.size(), 3);
    QVERIFY(savedDirectories.contains(directories.at(0)));
    QVERIFY(savedDirectories.contains(directories.at(1)));
    QCOMPARE(savedGeometry, QRect(120, 90, 700, 500));

    Konsole& konsole = start({QStringLiteral("-session"), QStringLiteral("restoretest_1")});
    QVERIFY(!konsole.service.isEmpty());
    // Titles follow the shells' directories once they run.
    QTRY_VERIFY_WITH_TIMEOUT(restored(konsole).titles.filter(QStringLiteral("tab-")).size() == 3, 10000);
    QTRY_VERIFY(!restored(konsole).geometry.isEmpty());
    const Restored qml = restored(konsole);
    // What was saved: two tabs, the second split, in those directories, at that place and size.
    QCOMPARE(qml.sessions, 3);
    QCOMPARE(qml.hierarchy, savedHierarchy);
    QCOMPARE(qml.titles, savedDirectories);
    QCOMPARE(qml.geometry, savedGeometry);
}

QTEST_GUILESS_MAIN(QmlWindowDBusTest)

#include "QmlWindowDBusTest.moc"
