/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// With a global menu in the session, the menu bar is registered for the QML window on screen,
// not the off-screen compat window.

#include "TestTheme.h"

#include "../MainWindow.h"
#include "../profile/ProfileManager.h"
#include "../quick/WindowController.h"

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDir>
#include <QMenuBar>
#include <QPointer>
#include <QQuickWindow>
#include <QSet>
#include <QTest>

using namespace Konsole;

namespace {
// What Qt's QDBusMenuBar calls: the registrar a global menu provides.
class Registrar : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.AppMenu.Registrar")
public:
    QSet<uint> windows;

public Q_SLOTS:
    Q_SCRIPTABLE void RegisterWindow(uint windowId, const QDBusObjectPath&)
    {
        windows.insert(windowId);
    }
    Q_SCRIPTABLE void UnregisterWindow(uint windowId)
    {
        windows.remove(windowId);
    }
};
}

class QmlWindowGlobalMenuTest : public QObject {
    Q_OBJECT

public:
    static void initMain()
    {
        TestTheme::setUp();
    }

private Q_SLOTS:
    void initTestCase();
    void testMenuBarIsRegisteredForTheQmlWindow();

private:
    Registrar _registrar;
};

void QmlWindowGlobalMenuTest::initTestCase()
{
    QCoreApplication::setApplicationName(QStringLiteral("konsole"));
    // Qt uses the registrar on X11 only.
    QCOMPARE(QGuiApplication::platformName(), QStringLiteral("xcb"));
    // Before the first menu bar: Qt checks for the registrar once.
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.registerObject(QStringLiteral("/com/canonical/AppMenu/Registrar"), &_registrar, QDBusConnection::ExportScriptableSlots));
    QVERIFY(bus.registerService(QStringLiteral("com.canonical.AppMenu.Registrar")));
}

void QmlWindowGlobalMenuTest::testMenuBarIsRegisteredForTheQmlWindow()
{
    QPointer<MainWindow> window = new MainWindow();
    auto* controller = new WindowController(window);
    QQuickWindow* qml = controller->qmlWindow();
    QVERIFY(qml);
    window->createSession(ProfileManager::instance()->defaultProfile(), QDir::tempPath());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(qml));

    QVERIFY(window->menuBar()->isNativeMenuBar());
    QTRY_COMPARE(_registrar.windows, QSet<uint>{uint(qml->winId())});
    delete window;
}

QTEST_MAIN(QmlWindowGlobalMenuTest)

#include "QmlWindowGlobalMenuTest.moc"
