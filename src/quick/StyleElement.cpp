/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "StyleElement.h"

#include <QApplication>
#include <QPainter>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QStyle>
#include <QStyleOptionFrame>
#include <QStyleOptionTabBarBase>
#include <QTabBar>

namespace Konsole {
StyleElement::StyleElement(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
}

void StyleElement::updatePolish()
{
    if (isVisible() && window() && width() >= 1 && height() >= 1) {
        const qreal ratio = window()->effectiveDevicePixelRatio();
        _image = QImage((size() * ratio).toSize(), QImage::Format_ARGB32_Premultiplied);
        _image.setDevicePixelRatio(ratio);
        _image.fill(Qt::transparent);
        QPainter painter(&_image);
        paint(&painter);
    } else {
        _image = QImage();
    }
    update();
}

QSGNode* StyleElement::updatePaintNode(QSGNode* node, UpdatePaintNodeData*)
{
    // A new node owns each new texture, so the old one goes with the old node.
    delete node;
    if (_image.isNull()) {
        return nullptr;
    }
    QSGImageNode* imageNode = window()->createImageNode();
    imageNode->setTexture(window()->createTextureFromImage(_image));
    imageNode->setOwnsTexture(true);
    imageNode->setRect(boundingRect());
    return imageNode;
}

void StyleElement::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        polish();
    }
}

void StyleElement::itemChange(ItemChange change, const ItemChangeData& value)
{
    if (change == ItemVisibleHasChanged || change == ItemEnabledHasChanged || change == ItemDevicePixelRatioHasChanged
        || change == ItemSceneChange) {
        polish();
    }
    QQuickItem::itemChange(change, value);
}

QString StyleElement::element() const
{
    return _element;
}

void StyleElement::setElement(const QString& element)
{
    if (_element != element) {
        _element = element;
        polish();
        Q_EMIT changed();
    }
}

int StyleElement::shape() const
{
    return _shape;
}

void StyleElement::setShape(int shape)
{
    if (_shape != shape) {
        _shape = shape;
        polish();
        Q_EMIT changed();
    }
}

QPalette StyleElement::stylePalette() const
{
    return _palette;
}

void StyleElement::setStylePalette(const QPalette& palette)
{
    _palette = palette;
    polish();
    Q_EMIT changed();
}

QFont StyleElement::styleFont() const
{
    return _font;
}

void StyleElement::setStyleFont(const QFont& font)
{
    _font = font;
    polish();
    Q_EMIT changed();
}

bool StyleElement::focused() const
{
    return _focused;
}

void StyleElement::setFocused(bool focus)
{
    if (_focused != focus) {
        _focused = focus;
        polish();
        Q_EMIT changed();
    }
}

bool StyleElement::hover() const
{
    return _hover;
}

void StyleElement::setHover(bool hover)
{
    if (_hover != hover) {
        _hover = hover;
        polish();
        Q_EMIT changed();
    }
}

int StyleElement::lineWidth() const
{
    return _lineWidth;
}

void StyleElement::setLineWidth(int width)
{
    if (_lineWidth != width) {
        _lineWidth = width;
        polish();
        Q_EMIT changed();
    }
}

void StyleElement::paint(QPainter* painter)
{
    if (_element == QLatin1String("lineedit")) {
        // QLineEdit::initStyleOption() from QStyleOption::initFrom(), for a framed edit.
        QStyle* style = QApplication::style();
        QStyleOptionFrame option;
        option.rect = boundingRect().toRect();
        option.palette = _palette;
        option.fontMetrics = QFontMetrics(_font);
        option.state = isEnabled() ? QStyle::State_Enabled : QStyle::State_None;
        if (window() && window()->isActive()) {
            option.state |= QStyle::State_Active;
        }
        if (_focused) {
            option.state |= QStyle::State_HasFocus;
        }
        if (_hover) {
            option.state |= QStyle::State_MouseOver;
        }
        option.state |= QStyle::State_Sunken;
        option.lineWidth = _lineWidth;
        option.midLineWidth = 0;
        option.styleObject = this;
        style->drawPrimitive(QStyle::PE_PanelLineEdit, &option, painter);
    } else if (_element == QLatin1String("splitter")) {
        // QSplitterHandle::paintEvent(); shape is the handle's Qt::Orientation.
        QStyleOption option;
        option.rect = boundingRect().toRect();
        option.palette = _palette;
        option.state = _shape == Qt::Horizontal ? QStyle::State_Horizontal : QStyle::State_None;
        if (_hover) {
            option.state |= QStyle::State_MouseOver;
        }
        if (isEnabled()) {
            option.state |= QStyle::State_Enabled;
        }
        QApplication::style()->drawControl(QStyle::CE_Splitter, &option, painter);
    } else if (_element == QLatin1String("tabbarbase")) {
        // TerminalHeaderBar::paintEvent()'s option, from QStyleOption::initFrom().
        QStyleOptionTabBarBase option;
        option.rect = boundingRect().toRect();
        option.palette = _palette;
        option.state = isEnabled() ? QStyle::State_Enabled : QStyle::State_None;
        if (window() && window()->isActive()) {
            option.state |= QStyle::State_Active;
        }
        option.shape = QTabBar::Shape(_shape);
        option.documentMode = false;
        QApplication::style()->drawPrimitive(QStyle::PE_FrameTabBarBase, &option, painter);
    }
}
}

#include "moc_StyleElement.cpp"
