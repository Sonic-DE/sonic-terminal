/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

namespace Konsole {
/** An inline KMessageWidget's frame: KMessageWidget::paintEvent() (kmessagewidget.cpp). */
class KONSOLEPRIVATE_EXPORT MessageFrameItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    // The message type's colour (the message's palette Window).
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY changed)
    // What the message lies on (its parent's palette Window).
    Q_PROPERTY(QColor parentColor READ parentColor WRITE setParentColor NOTIFY changed)
    // The show/hide animation's opacity (the time line's value squared), 1 when idle.
    Q_PROPERTY(qreal frameOpacity READ frameOpacity WRITE setFrameOpacity NOTIFY changed)

public:
    explicit MessageFrameItem(QQuickItem* parent = nullptr);

    QColor color() const;
    void setColor(const QColor& color);
    QColor parentColor() const;
    void setParentColor(const QColor& color);
    qreal frameOpacity() const;
    void setFrameOpacity(qreal opacity);

    void paint(QPainter* painter) override;

Q_SIGNALS:
    void changed();

private:
    QColor _color;
    QColor _parentColor;
    qreal _frameOpacity = 1;
};
}
