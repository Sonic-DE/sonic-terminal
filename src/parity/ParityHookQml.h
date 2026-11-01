// SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QList>
#include <QString>

class QMenu;
class QObject;
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

    // The QML dialogs (DialogWindow) shown.
    QList<QWindow*> qmlDialogs();
    // The focused QML dialog, else the one titled `title` when given.
    QWindow* qmlDialog(const QString& title);
    // A button of a QML dialog by object name, else by text without accelerator markers,
    // else by standard button (a QDialogButtonBox::StandardButton); null if none.
    QObject* qmlDialogButton(QWindow* dialog, const QString& object, const QString& text, int standardButton);
    QPixmap grabQmlDialog(QWindow* dialog);
    void rejectQmlDialog(QWindow* dialog);
}
}
