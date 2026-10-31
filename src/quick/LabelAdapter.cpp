/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "LabelAdapter.h"

#include "CompatWindows.h"
#include "TerminalView.h"
#include "terminalDisplay/TerminalDisplay.h"

#include <QEvent>
#include <QLabel>
#include <QStyle>

namespace Konsole {
LabelAdapter::LabelAdapter(QObject* parent)
    : WidgetMirror(parent)
{
    connect(CompatWindowsNotifier::instance(), &CompatWindowsNotifier::activeChanged, this, &LabelAdapter::changed);
}

TerminalView* LabelAdapter::view() const
{
    return _view;
}

void LabelAdapter::setView(TerminalView* view)
{
    if (_view == view) {
        return;
    }
    if (_view) {
        _view->disconnect(this);
        if (_view->display()) {
            _view->display()->removeEventFilter(this);
        }
    }
    _view = view;
    if (_view) {
        connect(_view, &TerminalView::labelsChanged, this, [this] {
            resolve();
            Q_EMIT changed();
        });
        if (_view->display()) {
            // The resize label is created on the first size notification.
            _view->display()->installEventFilter(this);
        }
    }
    resolve();
    Q_EMIT viewChanged();
}

QString LabelAdapter::label() const
{
    return _kind;
}

void LabelAdapter::setLabel(const QString& label)
{
    if (_kind != label) {
        _kind = label;
        resolve();
        Q_EMIT viewChanged();
    }
}

void LabelAdapter::resolve()
{
    QLabel* label = nullptr;
    if (_view) {
        label = _kind == QLatin1String("resize") ? _view->resizeLabel() : _kind == QLatin1String("hoverLink") ? _view->hoverLinkLabel()
                                                                                                              : nullptr;
    }
    if (label == _label) {
        return;
    }
    if (_label) {
        _label->removeEventFilter(this);
    }
    _label = label;
    if (_label) {
        _label->installEventFilter(this);
    }
    mirror(_label, _view ? _view->display() : nullptr);
    Q_EMIT changed();
}

bool LabelAdapter::eventFilter(QObject* watched, QEvent* event)
{
    if (_view && watched == _view->display()) {
        if (event->type() == QEvent::ChildAdded || event->type() == QEvent::ChildPolished) {
            resolve();
        }
    } else if (watched == _label) {
        switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::ShowToParent:
        case QEvent::PaletteChange:
        case QEvent::FontChange:
        case QEvent::StyleChange:
        case QEvent::ContentsRectChange:
            // setText() resizes or shows the label before anyone could see it.
            Q_EMIT changed();
            break;
        default:
            break;
        }
    }
    return WidgetMirror::eventFilter(watched, event);
}

QString LabelAdapter::text() const
{
    return _label ? _label->text() : QString();
}

QFont LabelAdapter::font() const
{
    return _label ? _label->font() : QFont();
}

int LabelAdapter::alignment() const
{
    return _label ? int(_label->alignment()) : int(Qt::AlignLeft | Qt::AlignVCenter);
}

QRect LabelAdapter::contentsGeometry() const
{
    if (!_label) {
        return {};
    }
    // QLabelPrivate::documentRect(): the style sheet's padding and border are the frame, and
    // a framed label without an indent indents by half an 'x'.
    QRect rect = _label->contentsRect().adjusted(_label->margin(), _label->margin(), -_label->margin(), -_label->margin());
    int indent = _label->indent();
    if (indent < 0 && _label->frameWidth()) {
        indent = _label->fontMetrics().horizontalAdvance(u'x') / 2 - _label->margin();
    }
    if (indent > 0) {
        const auto align = QStyle::visualAlignment(_label->layoutDirection(), _label->alignment());
        if (align & Qt::AlignLeft) {
            rect.setLeft(rect.left() + indent);
        }
        if (align & Qt::AlignRight) {
            rect.setRight(rect.right() - indent);
        }
        if (align & Qt::AlignTop) {
            rect.setTop(rect.top() + indent);
        }
        if (align & Qt::AlignBottom) {
            rect.setBottom(rect.bottom() - indent);
        }
    }
    return rect;
}

int LabelAdapter::baseline() const
{
    if (!_label) {
        return 0;
    }
    // qt_format_text() for one line: the line is ascent + descent high.
    const QRect rect = contentsGeometry();
    const QFontMetricsF metrics(_label->font());
    const qreal height = metrics.ascent() + metrics.descent();
    qreal offset = 0;
    if (_label->alignment() & Qt::AlignBottom) {
        offset = rect.height() - height;
    } else if (_label->alignment() & Qt::AlignVCenter) {
        offset = (rect.height() - height) / 2;
    }
    return qRound(rect.y() + offset + metrics.ascent());
}

QColor LabelAdapter::backgroundColor() const
{
    return _label ? CompatWindows::palette(_label).color(QPalette::Window) : QColor();
}

QColor LabelAdapter::textColor() const
{
    return _label ? CompatWindows::palette(_label).color(_label->foregroundRole()) : QColor();
}

QColor LabelAdapter::borderColor() const
{
    return _label ? CompatWindows::palette(_label).color(QPalette::Dark) : QColor();
}

int LabelAdapter::borderWidth() const
{
    return _kind == QLatin1String("resize") ? 1 : 0;
}
}

#include "moc_LabelAdapter.cpp"
