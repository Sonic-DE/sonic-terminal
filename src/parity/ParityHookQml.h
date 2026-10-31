// SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class QMenu;
class QPixmap;
class QWindow;

namespace Konsole {
class MainWindow;

namespace ParityHook {
    // The QML window shown for a compat window; null for a widget window.
    QWindow* qmlWindow(const MainWindow* window);
    // Opens a menu bar menu under its entry in the QML window's menu bar; false if there is none.
    bool popupQmlMenu(const MainWindow* window, const QMenu* menu);
    // The QML window's client area (alpha kept); null for a widget window.
    QPixmap grabQmlWindow(const MainWindow* window);
}
}
