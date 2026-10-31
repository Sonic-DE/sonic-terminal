/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "StyleElement.h"

#include <QApplication>
#include <QPainter>
#include <QQuickWindow>
#include <QStyle>
#include <QStyleOptionFrame>
#include <QStyleOptionTabBarBase>
#include <QTabBar>

namespace Konsole {
StyleElement::StyleElement(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
}

QString StyleElement::element() const
{
    return _element;
}

void StyleElement::setElement(const QString& element)
{
    if (_element != element) {
        _element = element;
        update();
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
        update();
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
    update();
    Q_EMIT changed();
}

QFont StyleElement::styleFont() const
{
    return _font;
}

void StyleElement::setStyleFont(const QFont& font)
{
    _font = font;
    update();
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
        update();
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
        update();
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
        update();
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
