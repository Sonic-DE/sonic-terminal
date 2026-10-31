/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QObject>
#include <QPalette>

class QWidget;
class QWindow;

namespace Konsole {
/** A compat window's QML window became active or inactive. */
class KONSOLEPRIVATE_EXPORT CompatWindowsNotifier : public QObject {
    Q_OBJECT
public:
    static CompatWindowsNotifier* instance();

Q_SIGNALS:
    void activeChanged(const QObject* window);
    // The tab search was asked for in the window (TabbedViewContainer::searchTabs()); its QML
    // window shows it.
    void searchTabsRequested(const QObject* window);
};

/**
 * The off-screen compatibility windows a QML window shows (WindowController, konsoleapp).
 * Their terminals are created for a QML scene (TerminalView::SceneModeGuard). They are
 * never the active window, so their widgets' palettes report the Inactive colour group;
 * palette() gives a widget's palette in the group its QML window's activation implies.
 */
namespace CompatWindows {
    KONSOLEPRIVATE_EXPORT void add(const QObject* window);
    // Its QML window, once created.
    KONSOLEPRIVATE_EXPORT void setShownWindow(const QObject* window, QWindow* shown);
    KONSOLEPRIVATE_EXPORT void remove(const QObject* window);
    KONSOLEPRIVATE_EXPORT bool contains(const QObject* window);
    KONSOLEPRIVATE_EXPORT void setActive(const QObject* window, bool active);
    KONSOLEPRIVATE_EXPORT QPalette::ColorGroup colorGroup(const QWidget* widget);
    KONSOLEPRIVATE_EXPORT QPalette palette(const QWidget* widget);
    // QApplication::activeWindow(), or else the compat window whose QML window is active
    // (QApplication never makes a compat window its active window).
    KONSOLEPRIVATE_EXPORT QWidget* activeWindow();
    // The window on screen for a top-level widget: a compat window's QML window, else its own.
    KONSOLEPRIVATE_EXPORT QWindow* shownWindow(const QWidget* window);
}
}
