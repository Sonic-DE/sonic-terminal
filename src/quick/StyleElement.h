/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QFont>
#include <QImage>
#include <QPalette>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

namespace Konsole {
/**
 * A style element the desktop style's StyleItem doesn't offer, or can't give the widget's
 * palette, drawn with QApplication::style() from an option built from these properties.
 * Elements: "tabbarbase" (PE_FrameTabBarBase; shape is a QTabBar::Shape), "lineedit"
 * (PE_PanelLineEdit as QLineEdit::paintEvent() draws it), "splitter" (CE_Splitter as
 * QSplitterHandle::paintEvent() draws it; shape is the Qt::Orientation).
 * Drawn in updatePolish(): widget styles may only be used on the GUI thread.
 */
class KONSOLEPRIVATE_EXPORT StyleElement : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString element READ element WRITE setElement NOTIFY changed)
    Q_PROPERTY(int shape READ shape WRITE setShape NOTIFY changed)
    // QQuickItem has its own palette, so the option's palette has its own name.
    Q_PROPERTY(QPalette stylePalette READ stylePalette WRITE setStylePalette NOTIFY changed)
    Q_PROPERTY(QFont styleFont READ styleFont WRITE setStyleFont NOTIFY changed)
    Q_PROPERTY(bool focused READ focused WRITE setFocused NOTIFY changed)
    Q_PROPERTY(bool hover READ hover WRITE setHover NOTIFY changed)
    // The frame width the widget's style gives the widget (styles answer per widget type).
    Q_PROPERTY(int lineWidth READ lineWidth WRITE setLineWidth NOTIFY changed)

public:
    explicit StyleElement(QQuickItem* parent = nullptr);

    QString element() const;
    void setElement(const QString& element);
    int shape() const;
    void setShape(int shape);
    QPalette stylePalette() const;
    void setStylePalette(const QPalette& palette);
    QFont styleFont() const;
    void setStyleFont(const QFont& font);
    bool focused() const;
    void setFocused(bool focus);
    bool hover() const;
    void setHover(bool hover);
    int lineWidth() const;
    void setLineWidth(int width);

Q_SIGNALS:
    void changed();

protected:
    void updatePolish() override;
    QSGNode* updatePaintNode(QSGNode* node, UpdatePaintNodeData* data) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;

private:
    void paint(QPainter* painter);

    QImage _image;
    QString _element;
    int _shape = 0;
    QPalette _palette;
    QFont _font;
    bool _focused = false;
    bool _hover = false;
    int _lineWidth = 0;
};
}
