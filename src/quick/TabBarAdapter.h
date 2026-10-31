/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "WidgetMirror.h"

#include <QPoint>
#include <QPointF>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QTabBar;

namespace Konsole {
class DetachableTabBar;
class TabbedViewContainer;

/**
 * A compat window's (hidden) tab bar as data for the QML tab bar (QQC2 TabBar): its geometry,
 * its tabs where the hidden bar lays them out, and its close and scroll buttons. Pointer
 * input ports the tab bar's handlers and acts through its API. Positions are the tab bar's.
 */
class KONSOLEPRIVATE_EXPORT TabBarAdapter : public WidgetMirror {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject* window READ window WRITE setWindow NOTIFY windowChanged)
    // The tabs as the QML tab bar draws them, in the tab bar: {rect, stripRect (colour and
    // progress bar), text (elided as the style does), label (text without mnemonics), textRect
    // (in the tab), iconSize, iconName, toolTip, textColor (invalid: the style's), color
    // (invalid: none), progress (-1: none), visible}.
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY changed)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY changed)
    // True when the tabs sit below the terminals (QTabWidget::South).
    Q_PROPERTY(bool atBottom READ atBottom NOTIFY changed)
    // {geometry, index, toolTip}
    Q_PROPERTY(QVariantList closeButtons READ closeButtons NOTIFY changed)
    // {geometry, enabled}
    Q_PROPERTY(QVariantList scrollButtons READ scrollButtons NOTIFY changed)
    // The tab widget's corner widgets (for ToolButtonsAdapter).
    Q_PROPERTY(QObject* leftCorner READ leftCorner NOTIFY cornersChanged)
    Q_PROPERTY(QObject* rightCorner READ rightCorner NOTIFY cornersChanged)

public:
    explicit TabBarAdapter(QObject* parent = nullptr);

    QObject* window() const;
    void setWindow(QObject* window);
    QVariantList tabs() const;
    int currentIndex() const;
    bool atBottom() const;
    QVariantList closeButtons() const;
    QVariantList scrollButtons() const;
    QObject* leftCorner() const;
    QObject* rightCorner() const;

    Q_INVOKABLE int tabAt(QPointF position) const;
    Q_INVOKABLE void press(QPointF position, int button);
    Q_INVOKABLE void release(QPointF position, int button, int pressedIndex);
    Q_INVOKABLE void doubleClick(QPointF position, int button);
    Q_INVOKABLE void wheel(QPointF angleDelta);
    Q_INVOKABLE void contextMenu(QPointF position);
    // Index into closeButtons() then scrollButtons().
    Q_INVOKABLE void clickButton(int index);

Q_SIGNALS:
    void windowChanged();
    void changed();
    void cornersChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QList<QWidget*> buttons() const;

    QPointer<QWidget> _window;
    QPointer<TabbedViewContainer> _container;
    QPointer<DetachableTabBar> _tabBar;
    QPoint _accumulatedAngleDelta;
};
}
