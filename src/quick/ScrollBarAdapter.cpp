/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ScrollBarAdapter.h"

#include "CompatWindows.h"
#include "TerminalView.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "terminalDisplay/TerminalScrollBar.h"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QStyleOptionSlider>

namespace Konsole {
namespace {
    // QScrollBar's delays (qscrollbar.cpp: initialDelay, activateControl()'s default).
    constexpr int InitialRepeatDelay = 500;
    constexpr int RepeatDelay = 50;
}

ScrollBarAdapter::ScrollBarAdapter(QObject* parent)
    : WidgetMirror(parent)
{
    connect(CompatWindowsNotifier::instance(), &CompatWindowsNotifier::activeChanged, this, &ScrollBarAdapter::paletteChanged);
}

TerminalView* ScrollBarAdapter::view() const
{
    return _view;
}

void ScrollBarAdapter::setView(TerminalView* view)
{
    if (_view == view) {
        return;
    }
    if (_scrollBar) {
        stopRepeatAction();
        _scrollBar->removeEventFilter(this);
        _scrollBar->disconnect(this);
    }
    _view = view;
    _scrollBar = view && view->display() ? view->display()->scrollBar() : nullptr;
    mirror(_scrollBar, view ? view->display() : nullptr);
    if (_scrollBar) {
        _scrollBar->installEventFilter(this);
        connect(_scrollBar, &QAbstractSlider::valueChanged, this, &ScrollBarAdapter::stateChanged);
        connect(_scrollBar, &QAbstractSlider::rangeChanged, this, &ScrollBarAdapter::stateChanged);
        connect(_scrollBar, &QAbstractSlider::sliderMoved, this, &ScrollBarAdapter::stateChanged);
        connect(_scrollBar, &TerminalScrollBar::overlaysChanged, this, &ScrollBarAdapter::overlaysChanged);
    }
    Q_EMIT viewChanged();
    Q_EMIT stateChanged();
    Q_EMIT paletteChanged();
    Q_EMIT overlaysChanged();
}

bool ScrollBarAdapter::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _scrollBar) {
        switch (event->type()) {
        case QEvent::Resize:
            Q_EMIT stateChanged();
            Q_EMIT overlaysChanged();
            break;
        case QEvent::PaletteChange:
            Q_EMIT paletteChanged();
            break;
        case QEvent::EnabledChange:
        case QEvent::StyleChange:
            Q_EMIT stateChanged();
            break;
        default:
            break;
        }
    }
    return WidgetMirror::eventFilter(watched, event);
}

void ScrollBarAdapter::option(QStyleOptionSlider* option) const
{
    _scrollBar->initStyleOption(option);
}

QStyle::SubControl ScrollBarAdapter::hitTest(QPointF position) const
{
    QStyleOptionSlider opt;
    option(&opt);
    return _scrollBar->style()->hitTestComplexControl(QStyle::CC_ScrollBar, &opt, position.toPoint(), nullptr);
}

bool ScrollBarAdapter::enabled() const
{
    return _scrollBar && _scrollBar->isEnabled();
}

int ScrollBarAdapter::styleMaximum() const
{
    if (!_scrollBar || _scrollBar->pageStep() <= 0) {
        return 0;
    }
    const int length = _scrollBar->orientation() == Qt::Horizontal ? _scrollBar->width() : _scrollBar->height();
    return qRound(double(length) * (_scrollBar->maximum() - _scrollBar->minimum()) / _scrollBar->pageStep());
}

int ScrollBarAdapter::styleValue() const
{
    if (!_scrollBar || _scrollBar->pageStep() <= 0) {
        return 0;
    }
    const int length = _scrollBar->orientation() == Qt::Horizontal ? _scrollBar->width() : _scrollBar->height();
    return qRound(double(length) * (_scrollBar->sliderPosition() - _scrollBar->minimum()) / _scrollBar->pageStep());
}

QString ScrollBarAdapter::activeControl() const
{
    switch (_pressedControl != QStyle::SC_None ? _pressedControl : _hoverControl) {
    case QStyle::SC_ScrollBarSubLine:
        return QStringLiteral("up");
    case QStyle::SC_ScrollBarAddLine:
        return QStringLiteral("down");
    case QStyle::SC_ScrollBarSlider:
        return QStringLiteral("handle");
    case QStyle::SC_ScrollBarSubPage:
        return QStringLiteral("upPage");
    case QStyle::SC_ScrollBarAddPage:
        return QStringLiteral("downPage");
    default:
        return QStringLiteral("none");
    }
}

bool ScrollBarAdapter::sunken() const
{
    return _pressedControl != QStyle::SC_None && !_pointerOutsidePressedControl;
}

bool ScrollBarAdapter::hovered() const
{
    return _hovered;
}

bool ScrollBarAdapter::customPalette() const
{
    if (!_scrollBar) {
        return false;
    }
    const QPalette application = QGuiApplication::palette();
    const QPalette own = CompatWindows::palette(_scrollBar);
    return own.color(QPalette::Window) != application.color(QPalette::Window) || own.color(QPalette::WindowText) != application.color(QPalette::WindowText);
}

QColor ScrollBarAdapter::backgroundColor() const
{
    return _scrollBar ? CompatWindows::palette(_scrollBar).color(QPalette::Window) : QColor();
}

QColor ScrollBarAdapter::textColor() const
{
    return _scrollBar ? CompatWindows::palette(_scrollBar).color(QPalette::WindowText) : QColor();
}

QVariantList ScrollBarAdapter::searchLines() const
{
    QVariantList lines;
    if (_scrollBar) {
        for (int line : std::as_const(_scrollBar->_searchLines)) {
            lines.append((line * _scrollBar->height()) / _scrollBar->_terminalLines);
        }
    }
    return lines;
}

QColor ScrollBarAdapter::searchLineColor() const
{
    if (!_scrollBar) {
        return {};
    }
    QColor color = _scrollBar->_searchHighlightLineColor;
    color.setAlpha(_scrollBar->_lineOpacity);
    return color;
}

QVariantList ScrollBarAdapter::markers() const
{
    QVariantList markers;
    if (_scrollBar) {
        for (const TerminalScrollBar::Marker* marker : std::as_const(_scrollBar->_markers)) {
            markers.append(marker->geometry);
        }
    }
    return markers;
}

QColor ScrollBarAdapter::markerColor() const
{
    return _scrollBar ? _scrollBar->_markerColor : QColor();
}

QString ScrollBarAdapter::toolTip() const
{
    return _toolTip;
}

QPointF ScrollBarAdapter::toolTipPosition() const
{
    return _toolTipPosition;
}

int ScrollBarAdapter::pixelPosToRangeValue(int position) const
{
    QStyleOptionSlider opt;
    option(&opt);
    QStyle* style = _scrollBar->style();
    const QRect groove = style->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarGroove, nullptr);
    const QRect slider = style->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, nullptr);
    int sliderMin;
    int sliderMax;
    if (_scrollBar->orientation() == Qt::Horizontal) {
        sliderMin = groove.x();
        sliderMax = groove.right() - slider.width() + 1;
        if (_scrollBar->layoutDirection() == Qt::RightToLeft) {
            opt.upsideDown = !opt.upsideDown;
        }
    } else {
        sliderMin = groove.y();
        sliderMax = groove.bottom() - slider.height() + 1;
    }
    return QStyle::sliderValueFromPosition(_scrollBar->minimum(), _scrollBar->maximum(), position - sliderMin, sliderMax - sliderMin, opt.upsideDown);
}

void ScrollBarAdapter::activateControl(QStyle::SubControl control, int threshold)
{
    QAbstractSlider::SliderAction action = QAbstractSlider::SliderNoAction;
    switch (control) {
    case QStyle::SC_ScrollBarAddPage:
        action = QAbstractSlider::SliderPageStepAdd;
        break;
    case QStyle::SC_ScrollBarSubPage:
        action = QAbstractSlider::SliderPageStepSub;
        break;
    case QStyle::SC_ScrollBarAddLine:
        action = QAbstractSlider::SliderSingleStepAdd;
        break;
    case QStyle::SC_ScrollBarSubLine:
        action = QAbstractSlider::SliderSingleStepSub;
        break;
    case QStyle::SC_ScrollBarFirst:
        action = QAbstractSlider::SliderToMinimum;
        break;
    case QStyle::SC_ScrollBarLast:
        action = QAbstractSlider::SliderToMaximum;
        break;
    default:
        break;
    }
    if (action != QAbstractSlider::SliderNoAction) {
        // The slider's own repeat timer runs while its widget is hidden.
        _scrollBar->setRepeatAction(action, threshold, RepeatDelay);
        _scrollBar->triggerAction(action);
    }
}

void ScrollBarAdapter::stopRepeatAction()
{
    if (!_scrollBar) {
        return;
    }
    const QStyle::SubControl pressed = _pressedControl;
    _scrollBar->setRepeatAction(QAbstractSlider::SliderNoAction);
    _pressedControl = QStyle::SC_None;
    if (pressed == QStyle::SC_ScrollBarSlider) {
        _scrollBar->setSliderDown(false);
    }
    Q_EMIT stateChanged();
}

bool ScrollBarAdapter::press(QPointF position, int button, int buttons, int modifiers, int flags)
{
    if (!_scrollBar) {
        return false;
    }
    // A widget gets this press as a double click, which TerminalScrollBar takes for its
    // markers only.
    if ((flags & Qt::MouseEventCreatedDoubleClick) != 0) {
        doubleClick(position);
        return true;
    }

    if (_scrollBar->repeatAction() != QAbstractSlider::SliderNoAction) {
        stopRepeatAction();
    }
    QStyle* style = _scrollBar->style();
    const bool middleButtonAbsolute = style->styleHint(QStyle::SH_ScrollBar_MiddleClickAbsolutePosition, nullptr, nullptr);
    QStyleOptionSlider opt;
    option(&opt);
    opt.keyboardModifiers = Qt::KeyboardModifiers(modifiers);

    if (_scrollBar->maximum() == _scrollBar->minimum() || (buttons & ~button) != 0
        || !(button == Qt::LeftButton || (middleButtonAbsolute && button == Qt::MiddleButton))) {
        return false;
    }

    const bool horizontal = _scrollBar->orientation() == Qt::Horizontal;
    const QPoint click = position.toPoint();
    _pressedControl = style->hitTestComplexControl(QStyle::CC_ScrollBar, &opt, click, nullptr);
    _pointerOutsidePressedControl = false;
    const QRect slider = style->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, nullptr);
    if (_pressedControl == QStyle::SC_ScrollBarSlider) {
        _clickOffset = horizontal ? click.x() - slider.x() : click.y() - slider.y();
        _snapBackPosition = _scrollBar->sliderPosition();
    }
    if ((_pressedControl == QStyle::SC_ScrollBarAddPage || _pressedControl == QStyle::SC_ScrollBarSubPage)
        && ((middleButtonAbsolute && button == Qt::MiddleButton)
            || (style->styleHint(QStyle::SH_ScrollBar_LeftClickAbsolutePosition, &opt, nullptr) && button == Qt::LeftButton))) {
        const int sliderLength = horizontal ? slider.width() : slider.height();
        _scrollBar->setSliderPosition(pixelPosToRangeValue((horizontal ? click.x() : click.y()) - sliderLength / 2));
        _pressedControl = QStyle::SC_ScrollBarSlider;
        _clickOffset = sliderLength / 2;
    }
    activateControl(_pressedControl, InitialRepeatDelay);
    if (_pressedControl == QStyle::SC_ScrollBarSlider) {
        _scrollBar->setSliderDown(true);
    }
    Q_EMIT stateChanged();
    return true;
}

void ScrollBarAdapter::move(QPointF position, int buttons)
{
    hover(position);
    if (!_scrollBar || _pressedControl == QStyle::SC_None) {
        return;
    }
    QStyle* style = _scrollBar->style();
    QStyleOptionSlider opt;
    option(&opt);
    if (!((buttons & Qt::LeftButton) != 0
            || ((buttons & Qt::MiddleButton) != 0 && style->styleHint(QStyle::SH_ScrollBar_MiddleClickAbsolutePosition, &opt, nullptr)))) {
        return;
    }

    const QPoint click = position.toPoint();
    if (_pressedControl == QStyle::SC_ScrollBarSlider) {
        int newPosition = pixelPosToRangeValue((_scrollBar->orientation() == Qt::Horizontal ? click.x() : click.y()) - _clickOffset);
        const int m = style->pixelMetric(QStyle::PM_MaximumDragDistance, &opt, nullptr);
        if (m >= 0 && !_scrollBar->rect().adjusted(-m, -m, m, m).contains(click)) {
            newPosition = _snapBackPosition;
        }
        _scrollBar->setSliderPosition(newPosition);
    } else if (!style->styleHint(QStyle::SH_ScrollBar_ScrollWhenPointerLeavesControl, &opt, nullptr)) {
        const auto lines = QStyle::SC_ScrollBarAddLine | QStyle::SC_ScrollBarSubLine;
        if (style->styleHint(QStyle::SH_ScrollBar_RollBetweenButtons, &opt, nullptr) && (_pressedControl & lines) != 0) {
            const QStyle::SubControl control = style->hitTestComplexControl(QStyle::CC_ScrollBar, &opt, click, nullptr);
            if (control == _pressedControl && !_pointerOutsidePressedControl) {
                return;
            }
            if ((control & lines) != 0) {
                _pointerOutsidePressedControl = false;
                _pressedControl = control;
                activateControl(_pressedControl, RepeatDelay);
                Q_EMIT stateChanged();
                return;
            }
        }
        // Stop scrolling when the pointer leaves the pressed control, as push buttons do.
        const QRect pressed = style->subControlRect(QStyle::CC_ScrollBar, &opt, _pressedControl, nullptr);
        if (pressed.contains(click) == _pointerOutsidePressedControl) {
            _pointerOutsidePressedControl = !_pointerOutsidePressedControl;
            if (_pointerOutsidePressedControl) {
                _scrollBar->setRepeatAction(QAbstractSlider::SliderNoAction);
            } else {
                activateControl(_pressedControl, RepeatDelay);
            }
            Q_EMIT stateChanged();
        }
    }
}

void ScrollBarAdapter::release(int buttons)
{
    if (_pressedControl == QStyle::SC_None || (buttons & (Qt::LeftButton | Qt::MiddleButton)) != 0) {
        return;
    }
    stopRepeatAction();
}

void ScrollBarAdapter::doubleClick(QPointF position)
{
    if (!_scrollBar) {
        return;
    }
    const int y = position.y();
    if (_scrollBar->markerExists(y)) {
        _scrollBar->removeMarker(y);
    } else {
        _scrollBar->registerMarker(_scrollBar->createMarker(y));
    }
}

void ScrollBarAdapter::hover(QPointF position)
{
    if (!_scrollBar) {
        return;
    }
    _hovered = true;
    setHoverControl(hitTest(position));

    // TerminalScrollBar::mouseMoveEvent()'s line tooltip.
    const int y = position.y();
    for (int line : std::as_const(_scrollBar->_searchLines)) {
        if (std::abs((line * _scrollBar->height()) / _scrollBar->_terminalLines - y) <= 3) {
            setToolTip(QStringLiteral("line %1").arg(line), position);
            return;
        }
    }
    setToolTip(QString(), position);
}

void ScrollBarAdapter::exit()
{
    _hovered = false;
    setHoverControl(QStyle::SC_None);
    setToolTip(QString(), _toolTipPosition);
}

void ScrollBarAdapter::setHoverControl(QStyle::SubControl control)
{
    _hoverControl = control;
    Q_EMIT stateChanged();
}

void ScrollBarAdapter::setToolTip(const QString& text, QPointF position)
{
    if (text == _toolTip && (text.isEmpty() || position == _toolTipPosition)) {
        return;
    }
    _toolTip = text;
    _toolTipPosition = position;
    Q_EMIT toolTipChanged();
}

bool ScrollBarAdapter::contextMenu(QPointF position, QPointF globalPosition)
{
    if (!_scrollBar || _scrollBar->contextMenuPolicy() != Qt::DefaultContextMenu
        || !_scrollBar->style()->styleHint(QStyle::SH_ScrollBar_ContextMenu, nullptr, nullptr)) {
        return false;
    }
    // QScrollBar::contextMenuEvent(); the menu's actions act on the slider.
    QMenu* menu = _scrollBar->createStandardContextMenu(position.toPoint());
    if (!menu) {
        return false;
    }
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(globalPosition.toPoint());
    return true;
}

bool ScrollBarAdapter::wheel(QPointF angleDelta, int modifiers)
{
    if (!_scrollBar) {
        return false;
    }
    // QScrollBar::wheelEvent() and QAbstractSliderPrivate::scrollByDelta().
    const bool horizontal = qAbs(angleDelta.x()) > qAbs(angleDelta.y());
    if (!horizontal && angleDelta.x() != 0 && _scrollBar->orientation() == Qt::Horizontal) {
        return false;
    }
    int delta = horizontal ? -angleDelta.x() : angleDelta.y();
    if (horizontal) {
        delta = -delta;
    }
    const qreal offset = qreal(delta) / 120;
    const int pageStep = _scrollBar->pageStep();
    int stepsToScroll = 0;
    if ((modifiers & (Qt::ControlModifier | Qt::ShiftModifier)) != 0) {
        stepsToScroll = qBound(-pageStep, int(offset * pageStep), pageStep);
        _offsetAccumulated = 0;
    } else {
        const qreal steps = QApplication::wheelScrollLines() * offset * _scrollBar->singleStep();
        if (_offsetAccumulated != 0 && (offset / _offsetAccumulated) < 0) {
            _offsetAccumulated = 0;
        }
        _offsetAccumulated += steps;
        stepsToScroll = qBound(-pageStep, int(_offsetAccumulated), pageStep);
        _offsetAccumulated -= int(_offsetAccumulated);
        if (stepsToScroll == 0) {
            const qreal effective = _scrollBar->invertedControls() ? -_offsetAccumulated : _offsetAccumulated;
            if ((effective > 0 && _scrollBar->value() < _scrollBar->maximum()) || (effective < 0 && _scrollBar->value() > _scrollBar->minimum())) {
                return true;
            }
            _offsetAccumulated = 0;
            return false;
        }
    }
    if (_scrollBar->invertedControls()) {
        stepsToScroll = -stepsToScroll;
    }
    const int previous = _scrollBar->value();
    _scrollBar->setSliderPosition(qBound(_scrollBar->minimum(), previous + stepsToScroll, _scrollBar->maximum()));
    if (previous == _scrollBar->value()) {
        _offsetAccumulated = 0;
        return false;
    }
    return true;
}
}

#include "moc_ScrollBarAdapter.cpp"
