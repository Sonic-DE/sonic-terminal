/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "WidgetMirror.h"

#include <QEvent>
#include <QPainter>

namespace Konsole {
StackingNotifier* StackingNotifier::instance()
{
    static StackingNotifier notifier;
    return &notifier;
}

WidgetMirror::WidgetMirror(QObject* parent)
    : QObject(parent)
{
    // Raising one child moves its siblings down without telling them.
    connect(StackingNotifier::instance(), &StackingNotifier::changed, this, [this](QWidget* facade) {
        if (facade == _facade) {
            update();
        }
    });
}

QRect WidgetMirror::geometry() const
{
    return _geometry;
}

bool WidgetMirror::shown() const
{
    return _shown;
}

int WidgetMirror::stackOrder() const
{
    return _stackOrder;
}

int WidgetMirror::stackOrderOf(const QWidget* widget)
{
    if (!widget || !widget->parentWidget()) {
        return 0;
    }
    int order = 0;
    for (const QObject* child : widget->parentWidget()->children()) {
        if (child == widget) {
            return order;
        }
        order += child->isWidgetType() ? 1 : 0;
    }
    return order;
}

void WidgetMirror::initPainter(QPainter* painter, const QWidget* widget, QPalette::ColorGroup group)
{
    const QPalette& palette = widget->palette();
    painter->setPen(QPen(palette.brush(group, widget->foregroundRole()), 1));
    painter->setBackground(palette.brush(group, widget->backgroundRole()));
    painter->setFont(QFont(widget->font(), painter->device()));
}

QWidget* WidgetMirror::mirrored() const
{
    return _widget;
}

void WidgetMirror::mirror(QWidget* widget, QWidget* facade)
{
    if (_widget) {
        _widget->removeEventFilter(this);
    }
    if (_facade) {
        _facade->removeEventFilter(this);
    }
    _widget = widget;
    _facade = facade;
    if (_widget) {
        _widget->installEventFilter(this);
    }
    if (_facade) {
        _facade->installEventFilter(this);
    }
    update();
}

bool WidgetMirror::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _facade && watched != _widget) {
        if (event->type() == QEvent::ChildAdded || event->type() == QEvent::ChildRemoved) {
            Q_EMIT StackingNotifier::instance()->changed(_facade);
        }
        return QObject::eventFilter(watched, event);
    }
    if (watched == _widget && event->type() == QEvent::ZOrderChange) {
        Q_EMIT StackingNotifier::instance()->changed(_facade);
    }
    if (watched == _widget) {
        switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::ShowToParent:
        case QEvent::HideToParent:
        case QEvent::ZOrderChange:
            update();
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

void WidgetMirror::update()
{
    QRect geometry;
    bool shown = false;
    if (_widget && _facade) {
        geometry = QRect(_widget->mapTo(_facade, QPoint(0, 0)), _widget->size());
        shown = _widget->isVisibleTo(_facade);
    }
    if (geometry != _geometry) {
        _geometry = geometry;
        Q_EMIT geometryChanged();
    }
    if (shown != _shown) {
        _shown = shown;
        Q_EMIT shownChanged();
    }
    const int stackOrder = stackOrderOf(_widget);
    if (stackOrder != _stackOrder) {
        _stackOrder = stackOrder;
        Q_EMIT stackOrderChanged();
    }
}
}

#include "moc_WidgetMirror.cpp"
