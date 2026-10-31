/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "WidgetMirror.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPalette>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QAbstractButton;

namespace Konsole {
class TerminalHeaderBar;
class TerminalView;

/**
 * The facade's (hidden) TerminalHeaderBar as data for the QML header bar: where its layout
 * put each child, what they show, and the bar's own painting. Buttons and the bar's mouse
 * input act through the header bar's own buttons and drag rule. Child geometry is relative
 * to the header bar.
 */
class KONSOLEPRIVATE_EXPORT HeaderBarAdapter : public WidgetMirror {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Konsole::TerminalView* view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY changed)
    Q_PROPERTY(QColor textColor READ textColor NOTIFY changed)
    Q_PROPERTY(QPalette palette READ palette NOTIFY changed)
    Q_PROPERTY(bool focused READ focused NOTIFY changed)
    Q_PROPERTY(QColor shadowColor READ shadowColor NOTIFY changed)
    Q_PROPERTY(QVariantList pixmaps READ pixmaps NOTIFY changed)
    Q_PROPERTY(QRect titleGeometry READ titleGeometry NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QFont titleFont READ titleFont NOTIFY changed)
    Q_PROPERTY(QColor titleColor READ titleColor NOTIFY changed)
    Q_PROPERTY(QVariantList buttons READ buttons NOTIFY changed)
    Q_PROPERTY(int toolTipDelay READ toolTipDelay CONSTANT)

public:
    explicit HeaderBarAdapter(QObject* parent = nullptr);

    TerminalView* view() const;
    void setView(TerminalView* view);

    QColor backgroundColor() const;
    QColor textColor() const;
    QPalette palette() const;
    bool focused() const;
    QColor shadowColor() const;
    // The shown icon labels: {geometry, image, alignment}.
    QVariantList pixmaps() const;
    QRect titleGeometry() const;
    QString title() const;
    QFont titleFont() const;
    QColor titleColor() const;
    // The shown tool buttons: {geometry, iconName, iconWidth, iconHeight, enabled, checked, toolTip}.
    QVariantList buttons() const;
    // QToolTip's wake-up delay.
    int toolTipDelay() const;

    Q_INVOKABLE void clickButton(int index);
    Q_INVOKABLE void press(QPointF position);
    Q_INVOKABLE void move(QPointF position);
    Q_INVOKABLE void doubleClick(int button);

Q_SIGNALS:
    void viewChanged();
    void changed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QList<QAbstractButton*> shownButtons() const;
    QRect childGeometry(const QWidget* child) const;

    QPointer<TerminalView> _view;
    QPointer<TerminalHeaderBar> _headerBar;
};
}
