/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "MessageFrameItem.h"

#include <QPainter>

namespace Konsole {
namespace {
    // kmessagewidget.cpp's borderSize.
    constexpr int BorderSize = 2;
}

MessageFrameItem::MessageFrameItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
}

QColor MessageFrameItem::color() const
{
    return _color;
}

void MessageFrameItem::setColor(const QColor& color)
{
    _color = color;
    update();
    Q_EMIT changed();
}

QColor MessageFrameItem::parentColor() const
{
    return _parentColor;
}

void MessageFrameItem::setParentColor(const QColor& color)
{
    _parentColor = color;
    update();
    Q_EMIT changed();
}

qreal MessageFrameItem::frameOpacity() const
{
    return _frameOpacity;
}

void MessageFrameItem::setFrameOpacity(qreal opacity)
{
    if (!qFuzzyCompare(_frameOpacity, opacity)) {
        _frameOpacity = opacity;
        update();
        Q_EMIT changed();
    }
}

void MessageFrameItem::paint(QPainter* painter)
{
    // KMessageWidget::paintEvent() for Position::Inline.
    painter->setOpacity(_frameOpacity);
    constexpr float radius = 4 * 0.6;
    const QRect innerRect = boundingRect().toRect().marginsRemoved(QMargins() + BorderSize / 2);
    constexpr float alpha = 0.2;
    const int red = (_color.red() * alpha) + (_parentColor.red() * (1 - alpha));
    const int green = (_color.green() * alpha) + (_parentColor.green() * (1 - alpha));
    const int blue = (_color.blue() * alpha) + (_parentColor.blue() * (1 - alpha));
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(QColor(red, green, blue));
    painter->setPen(QPen(_color, BorderSize));
    painter->drawRoundedRect(innerRect, radius, radius);
}
}

#include "moc_MessageFrameItem.cpp"
