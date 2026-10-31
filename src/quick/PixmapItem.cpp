/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "PixmapItem.h"

#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QStyle>

namespace Konsole {
PixmapItem::PixmapItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
}

QImage PixmapItem::image() const
{
    return _image;
}

void PixmapItem::setImage(const QImage& image)
{
    _image = image;
    update();
    Q_EMIT imageChanged();
}

int PixmapItem::alignment() const
{
    return _alignment;
}

void PixmapItem::setAlignment(int alignment)
{
    if (_alignment != alignment) {
        _alignment = alignment;
        update();
        Q_EMIT alignmentChanged();
    }
}

void PixmapItem::paint(QPainter* painter)
{
    if (!_image.isNull()) {
        QApplication::style()->drawItemPixmap(painter, boundingRect().toRect(), _alignment, QPixmap::fromImage(_image));
    }
}
}

#include "moc_PixmapItem.cpp"
