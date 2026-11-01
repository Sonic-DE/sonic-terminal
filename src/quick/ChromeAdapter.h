/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QAction;
class QMainWindow;
class QMenu;
class QQuickItem;
class QToolBar;

namespace Konsole {
/**
 * A compat window's KXmlGui menu bar and toolbars as data for Kirigami/QQC2 chrome: the
 * top-level menus, and the toolbars by area and line with their actions in order. The QML
 * bars trigger the real QActions and pop up the real QMenus; the hidden bars take the QML
 * bars' extents, so the compat central widget lies where the QML central area is.
 */
class KONSOLEPRIVATE_EXPORT ChromeAdapter : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool menuBarVisible READ menuBarVisible NOTIFY changed)
    // The hidden QMenuBar (for setExtent()).
    Q_PROPERTY(QObject* menuBarWidget READ menuBarWidget NOTIFY changed)
    // {action, menu, text}
    Q_PROPERTY(QVariantList menus READ menus NOTIFY changed)
    // Per area: lines of toolbars; each {toolBar, buttonStyle, iconSize, items}, items
    // {action, iconName, separator, spacer, buttonStyle, popupMode (QToolButton::ToolButtonPopupMode
    // or -1)}.
    Q_PROPERTY(QVariantList topLines READ topLines NOTIFY changed)
    Q_PROPERTY(QVariantList bottomLines READ bottomLines NOTIFY changed)
    Q_PROPERTY(QVariantList leftLines READ leftLines NOTIFY changed)
    Q_PROPERTY(QVariantList rightLines READ rightLines NOTIFY changed)
    // The QML menu bar items, in menu order (for the neighbouring menu and hover rules).
    Q_PROPERTY(QList<QQuickItem*> menuBarItems READ menuBarItems WRITE setMenuBarItems NOTIFY menuBarItemsChanged)
    // The menu bar menu shown now, or -1 (its item is highlighted, as QMenuBar does).
    Q_PROPERTY(int openIndex READ openIndex NOTIFY openMenuChanged)

public:
    explicit ChromeAdapter(QObject* parent = nullptr);
    ~ChromeAdapter() override;

    QObject* window() const;
    void setWindow(QObject* window);
    bool menuBarVisible() const;
    QObject* menuBarWidget() const;
    QVariantList menus() const;
    QVariantList topLines() const;
    QVariantList bottomLines() const;
    QVariantList leftLines() const;
    QVariantList rightLines() const;
    QList<QQuickItem*> menuBarItems() const;
    void setMenuBarItems(const QList<QQuickItem*>& items);

    // Opens the menu bar's menu at its QML item's bottom-left.
    Q_INVOKABLE void popupMenu(int index);
    // QAction arguments as QObject *: QML passes them as such.
    Q_INVOKABLE void trigger(QObject* action);
    // The menu of a toolbar action's button (KActionMenu, the hamburger), under the QML button.
    Q_INVOKABLE void popupButtonMenu(QObject* action, QQuickItem* button);
    // A toolbar action fired (from anywhere): the hamburger's menu goes under its QML button.
    Q_INVOKABLE void actionTriggered(QObject* action, QQuickItem* button);
    // The open popup menu, under this button (answering buttonMenuRequested()).
    Q_INVOKABLE void moveOpenMenu(QQuickItem* button);
    // KToolBar's own context menu (toolbar settings), as a right click on the widget opens it.
    Q_INVOKABLE void toolBarContextMenu(QObject* toolBar, QQuickItem* item, QPointF position);
    // The hidden bar's height (horizontal) or width (vertical) follows the QML bar's.
    Q_INVOKABLE void setExtent(QObject* bar, int extent);
    // The Qt::ToolBarArea of the window edge nearest a point (window coordinates).
    Q_INVOKABLE int dropArea(QPointF windowPosition, QSizeF windowSize) const;
    // An unlocked toolbar dropped at the window's edge nearest the drop point.
    Q_INVOKABLE void moveToolBar(QObject* toolBar, QPointF windowPosition, QSizeF windowSize);

    // The open menu bar menu, if any.
    QMenu* openMenu() const;
    int openIndex() const;

Q_SIGNALS:
    void windowChanged();
    void changed();
    void menuBarItemsChanged();
    void openMenuChanged();
    // The button showing this action puts the open menu under itself (moveOpenMenu()).
    void buttonMenuRequested(QObject* action);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void scheduleRefresh();
    void refresh();
    void watch(QObject* object);
    QVariantList lines(int area) const;
    void popup(QMenu* menu, QQuickItem* item, int menuIndex);
    QToolBar* toolBarOf(QAction* action) const;

    QPointer<QMainWindow> _window;
    QList<QPointer<QObject>> _watched;
    QVariantList _menus;
    QVariantList _lines[4];
    QList<QQuickItem*> _menuBarItems;
    QPointer<QMenu> _openMenu;
    int _openIndex = -1;
    bool _menuBarShown = false;
    bool _refreshScheduled = false;
};
}
