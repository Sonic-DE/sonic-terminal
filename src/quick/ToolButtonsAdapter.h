/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QToolButton;

namespace Konsole {
/**
 * The visible QToolButtons in a (hidden) widget of a compat window, e.g. the tab widget's
 * corner widgets, as data for QML tool buttons: where they are in the window and what they
 * show; clicks and menus act through the real button.
 */
class KONSOLEPRIVATE_EXPORT ToolButtonsAdapter : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject* root READ root WRITE setRoot NOTIFY rootChanged)
    // {geometry (in the window), toolTip, enabled, menuArrowGeometry (in the button, may be
    // empty), iconName, text (empty for icon-only), hasMenu}
    Q_PROPERTY(QVariantList buttons READ buttons NOTIFY changed)
    // QToolTip's wake-up delay.
    Q_PROPERTY(int toolTipDelay READ toolTipDelay CONSTANT)

public:
    explicit ToolButtonsAdapter(QObject* parent = nullptr);

    QObject* root() const;
    void setRoot(QObject* root);
    QVariantList buttons() const;
    QToolButton* button(int index) const;
    int toolTipDelay() const;

    // QToolButton's mouse rules: a press on the menu arrow, an instant-popup button or a
    // delayed-popup button held down opens the menu (at the button's screen position, which
    // the QML window shares); a click elsewhere clicks.
    Q_INVOKABLE void press(int index, QPointF position);
    Q_INVOKABLE void release(int index, QPointF position, bool inside);
    Q_INVOKABLE void holdPress(int index);

Q_SIGNALS:
    void rootChanged();
    void changed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void watch(QWidget* widget);
    void refresh();
    void popupMenu(QToolButton* button);

    QPointer<QWidget> _root;
    QList<QPointer<QToolButton>> _buttons;
    QVariantList _list;
    bool _menuOpenedOnPress = false;
};
}
