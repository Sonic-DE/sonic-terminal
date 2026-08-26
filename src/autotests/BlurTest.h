/*
    SPDX-FileCopyrightText: 2025 SonicDE

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef BLURTEST_H
#define BLURTEST_H

#include <QObject>
#include <QTemporaryDir>

class BlurTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testWinIdChangeSafeWithNoController();
    void testWinIdChangeWithControllerDoesNotCrash();
    void testMultipleWinIdChangesDoNotCrash();
    void testSetBlurEarlyReturnBlocksReapplication();
    void testSetBlurCalledBeforeWindowShown();
    void testReapplyBlurAfterTranslucencyToggleWithController();

private:
    QTemporaryDir* m_testDir = nullptr;
};

#endif // BLURTEST_H
