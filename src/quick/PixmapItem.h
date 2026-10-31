/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QImage>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

namespace Konsole {
/** A pixmap drawn as a QLabel draws one (QStyle::drawItemPixmap() with its alignment). */
class KONSOLEPRIVATE_EXPORT PixmapItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(int alignment READ alignment WRITE setAlignment NOTIFY alignmentChanged)

public:
    explicit PixmapItem(QQuickItem* parent = nullptr);

    QImage image() const;
    void setImage(const QImage& image);
    int alignment() const;
    void setAlignment(int alignment);

    void paint(QPainter* painter) override;

Q_SIGNALS:
    void imageChanged();
    void alignmentChanged();

private:
    QImage _image;
    int _alignment = Qt::AlignCenter;
};
}
