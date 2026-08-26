/*
    SPDX-FileCopyrightText: 2025 SonicDE

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "BlurTest.h"
#include <QCoreApplication>
#include <QEvent>
#include <QSignalSpy>
#include <QTest>
#include <QWindow>

#include "../MainWindow.h"
#include "../ViewManager.h"
#include "../profile/ProfileManager.h"
#include "../session/Session.h"
#include "../session/SessionManager.h"

using namespace Konsole;

void BlurTest::initTestCase()
{
    m_testDir = new QTemporaryDir(QDir::tempPath() + QDir::separator() + QStringLiteral("konsoleblurtest-XXXXXX"));
}

void BlurTest::cleanupTestCase()
{
    delete m_testDir;
}

void BlurTest::testWinIdChangeSafeWithNoController()
{
    auto mw = MainWindow();

    // reapplyBlur should be safe to call before any controller is plugged.
    QEvent winIdEvent(QEvent::WinIdChange);
    QCoreApplication::sendEvent(&mw, &winIdEvent);
}

void BlurTest::testWinIdChangeWithControllerDoesNotCrash()
{
    auto mw = MainWindow();
    mw.viewManager()->newSession(mw.viewManager()->defaultProfile(), m_testDir->path());

    // Simulate Silver ToolsAreaManager toggling WA_TranslucentBackground.
    mw.setAttribute(Qt::WA_TranslucentBackground, true);
    mw.setAttribute(Qt::WA_TranslucentBackground, false);

    QEvent winIdEvent(QEvent::WinIdChange);
    QCoreApplication::sendEvent(&mw, &winIdEvent);
}

void BlurTest::testMultipleWinIdChangesDoNotCrash()
{
    auto mw = MainWindow();
    mw.viewManager()->newSession(mw.viewManager()->defaultProfile(), m_testDir->path());

    for (int i = 0; i < 5; ++i) {
        mw.setAttribute(Qt::WA_TranslucentBackground, true);
        mw.setAttribute(Qt::WA_TranslucentBackground, false);

        QEvent winIdEvent(QEvent::WinIdChange);
        QCoreApplication::sendEvent(&mw, &winIdEvent);
    }
}

void BlurTest::testSetBlurEarlyReturnBlocksReapplication()
{
    // This test documents the core bug:
    // setBlur(true) sets _blurEnabled = true and applies blur.
    // If the window is then recreated (WA_TranslucentBackground toggle),
    // WinIdChange fires and reapplyBlur() re-applies.
    // But if blurSettingChanged(true) fires AFTER the recreation,
    // setBlur(true) returns early because _blurEnabled is already true.
    // If the window was recreated AGAIN after that, the blur is lost
    // and never re-applied.
    //
    // The fix: reapplyBlur() must be the ONLY path that applies blur,
    // and setBlur() must update _blurEnabled and trigger reapplyBlur()
    // rather than calling enableBlurBehind directly.

    auto mw = MainWindow();
    mw.viewManager()->newSession(mw.viewManager()->defaultProfile(), m_testDir->path());

    // Simulate: blur was applied, then window recreated, then reapplyBlur
    // re-applied, then another recreation happened.
    // With the early-return bug, the second recreation loses blur.
    QEvent winIdEvent1(QEvent::WinIdChange);
    QCoreApplication::sendEvent(&mw, &winIdEvent1);

    // Toggle translucency (causes window recreation on X11)
    mw.setAttribute(Qt::WA_TranslucentBackground, true);
    mw.setAttribute(Qt::WA_TranslucentBackground, false);

    QEvent winIdEvent2(QEvent::WinIdChange);
    QCoreApplication::sendEvent(&mw, &winIdEvent2);

    // The test passes if we don't crash. The actual blur verification
    // requires a running compositor, but the code path must not skip
    // re-application due to the early return in setBlur.
}

void BlurTest::testSetBlurCalledBeforeWindowShown()
{
    // Startup scenario: setBlur(true) is called before the window has
    // a valid windowHandle(). _blurEnabled becomes true, but blur is
    // not actually applied. Later calls return early.
    //
    // reapplyBlur() on WinIdChange should handle this by applying
    // _blurEnabled to whatever windowHandle() is current.

    auto mw = MainWindow();
    mw.viewManager()->newSession(mw.viewManager()->defaultProfile(), m_testDir->path());

    // At this point the window may not be shown. WinIdChange should
    // still be safe and should apply _blurEnabled when the window
    // eventually gets a handle.
    QEvent winIdEvent(QEvent::WinIdChange);
    QCoreApplication::sendEvent(&mw, &winIdEvent);

    // Now show the window
    mw.show();
    QVERIFY(mw.windowHandle() != nullptr);

    // Another WinIdChange after show should re-apply correctly
    QEvent winIdEvent2(QEvent::WinIdChange);
    QCoreApplication::sendEvent(&mw, &winIdEvent2);
}

void BlurTest::testReapplyBlurAfterTranslucencyToggleWithController()
{
    // The real-world scenario: Silver's ToolsAreaManager toggles
    // WA_TranslucentBackground after blur is enabled. This recreates
    // the window on X11. reapplyBlur() must re-apply the blur state.
    auto mw = MainWindow();
    mw.viewManager()->newSession(mw.viewManager()->defaultProfile(), m_testDir->path());
    mw.show();

    QVERIFY(mw.windowHandle() != nullptr);

    // Toggle translucency as Silver ToolsAreaManager would
    mw.setAttribute(Qt::WA_TranslucentBackground, true);

    // WinIdChange should fire and reapplyBlur should run
    QEvent winIdEvent(QEvent::WinIdChange);
    QCoreApplication::sendEvent(&mw, &winIdEvent);

    // Toggle back
    mw.setAttribute(Qt::WA_TranslucentBackground, false);

    QEvent winIdEvent2(QEvent::WinIdChange);
    QCoreApplication::sendEvent(&mw, &winIdEvent2);

    // Window should still be valid
    QVERIFY(mw.windowHandle() != nullptr);
}

QTEST_MAIN(BlurTest)

#include "moc_BlurTest.cpp"
