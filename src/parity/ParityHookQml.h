// SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class QDockWidget;
class QMenu;
class QObject;
class QPixmap;
class QWidget;
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
    // A PageDialog's or TabbedPageDialog's pages (DialogPage children), as their titles;
    // empty for a dialog without pages. Unavailable pages aren't listed.
    QStringList qmlDialogPages(QWindow* dialog);
    // Makes the page titled `title` current; false if there is none.
    bool showQmlDialogPage(QWindow* dialog, const QString& title);
    // Makes a row current in a list or combo box by object name: `row`, or the first row
    // whose text starts with `text`; false if there is no such list or row.
    bool selectQmlRow(QWindow* dialog, const QString& object, int row, const QString& text);
    // Sets a text field's text by object name; false if there is none.
    bool setQmlText(QWindow* dialog, const QString& object, const QString& text);
    // Clicks a table's cell through its clickItem(row, column), after this returns (the cell
    // may open a modal dialog); the row whose text is `text` when given; false if there is
    // no such table or row.
    bool clickQmlItem(QWindow* dialog, const QString& object, int row, int column, const QString& text);

    // A widget the QML application may have (NoWidgetsTest): in a compat window, a plugin
    // dock's content, in a menu, or opened inside a FrameworkDialogScope.
    bool allowedWidget(const QWidget* widget);
    // A plugin dock's content, which the QML window embeds outside the dock; null if none.
    QWidget* dockContent(const QDockWidget* dock);

    // A QML plugin panel by title, shown in the window's QML window; null if none.
    QObject* qmlPanel(const MainWindow* window, const QString& title);
    // A panel's button (or the first one in a control) by object name.
    QObject* qmlPanelButton(QObject* panel, const QString& object);
    bool setQmlPanelText(QObject* panel, const QString& object, const QString& text);
    // Acts on a PanelTree's row by text after this returns: kind "click", "double", "context".
    bool clickQmlPanelItem(QObject* panel, const QString& object, const QString& text, const QString& kind);
}
}
