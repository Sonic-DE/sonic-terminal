/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "WidgetMirror.h"

#include <QColor>
#include <QPointF>
#include <QStyle>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QStyleOptionSlider;

namespace Konsole {
class TerminalScrollBar;
class TerminalView;

/**
 * The facade's (hidden) TerminalScrollBar as data for the QML scroll bar, which the
 * desktop style's StyleItem draws. Pointer input ports QScrollBar's handlers and acts
 * through the slider's own API, so valueChanged/sliderMoved fire as they do today.
 * Positions are in the scroll bar's coordinates.
 */
class KONSOLEPRIVATE_EXPORT ScrollBarAdapter : public WidgetMirror {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Konsole::TerminalView* view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(int styleMaximum READ styleMaximum NOTIFY stateChanged)
    Q_PROPERTY(int styleValue READ styleValue NOTIFY stateChanged)
    Q_PROPERTY(QString activeControl READ activeControl NOTIFY stateChanged)
    Q_PROPERTY(bool sunken READ sunken NOTIFY stateChanged)
    Q_PROPERTY(bool hovered READ hovered NOTIFY stateChanged)
    Q_PROPERTY(bool customPalette READ customPalette NOTIFY paletteChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor textColor READ textColor NOTIFY paletteChanged)
    Q_PROPERTY(QVariantList searchLines READ searchLines NOTIFY overlaysChanged)
    Q_PROPERTY(QColor searchLineColor READ searchLineColor NOTIFY overlaysChanged)
    Q_PROPERTY(QVariantList markers READ markers NOTIFY overlaysChanged)
    Q_PROPERTY(QColor markerColor READ markerColor NOTIFY overlaysChanged)
    Q_PROPERTY(QString toolTip READ toolTip NOTIFY toolTipChanged)
    Q_PROPERTY(QPointF toolTipPosition READ toolTipPosition NOTIFY toolTipChanged)

public:
    explicit ScrollBarAdapter(QObject* parent = nullptr);

    TerminalView* view() const;
    void setView(TerminalView* view);

    bool enabled() const;
    // StyleItem takes the page step as its length in pixels; these scale range and value to it.
    int styleMaximum() const;
    int styleValue() const;
    QString activeControl() const;
    bool sunken() const;
    bool hovered() const;
    bool customPalette() const;
    QColor backgroundColor() const;
    QColor textColor() const;
    QVariantList searchLines() const;
    QColor searchLineColor() const;
    QVariantList markers() const;
    QColor markerColor() const;
    QString toolTip() const;
    QPointF toolTipPosition() const;

    Q_INVOKABLE bool press(QPointF position, int button, int buttons, int modifiers, int flags);
    Q_INVOKABLE void move(QPointF position, int buttons);
    Q_INVOKABLE void release(int buttons);
    Q_INVOKABLE void doubleClick(QPointF position);
    Q_INVOKABLE void hover(QPointF position);
    Q_INVOKABLE void exit();
    Q_INVOKABLE bool wheel(QPointF angleDelta, int modifiers);
    // False when the style has no scroll bar menu, so the terminal's own menu opens.
    Q_INVOKABLE bool contextMenu(QPointF position, QPointF globalPosition);

Q_SIGNALS:
    void viewChanged();
    void stateChanged();
    void paletteChanged();
    void overlaysChanged();
    void toolTipChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void option(QStyleOptionSlider* option) const;
    QStyle::SubControl hitTest(QPointF position) const;
    int pixelPosToRangeValue(int position) const;
    void activateControl(QStyle::SubControl control, int threshold);
    void stopRepeatAction();
    void setHoverControl(QStyle::SubControl control);
    void setToolTip(const QString& text, QPointF position);

    QPointer<TerminalView> _view;
    QPointer<TerminalScrollBar> _scrollBar;
    // QScrollBarPrivate's input state.
    QStyle::SubControl _pressedControl = QStyle::SC_None;
    QStyle::SubControl _hoverControl = QStyle::SC_None;
    bool _pointerOutsidePressedControl = false;
    bool _hovered = false;
    int _clickOffset = 0;
    int _snapBackPosition = 0;
    qreal _offsetAccumulated = 0;
    QString _toolTip;
    QPointF _toolTipPosition;
};
}
