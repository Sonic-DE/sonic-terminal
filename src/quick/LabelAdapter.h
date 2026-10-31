/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "WidgetMirror.h"

#include <QColor>
#include <QFont>
#include <QtQml/qqmlregistration.h>

class QLabel;

namespace Konsole {
class TerminalView;

/**
 * One of the facade's (hidden) style-sheet labels as data for its QML label: "hoverLink"
 * (the link under the pointer, bottom left) or "resize" (the size notification, centred).
 * Their style sheets give the background (palette(window)), padding and border.
 */
class KONSOLEPRIVATE_EXPORT LabelAdapter : public WidgetMirror {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Konsole::TerminalView* view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY viewChanged)
    Q_PROPERTY(QString text READ text NOTIFY changed)
    Q_PROPERTY(QFont font READ font NOTIFY changed)
    Q_PROPERTY(int alignment READ alignment NOTIFY changed)
    Q_PROPERTY(QRect contentsGeometry READ contentsGeometry NOTIFY changed)
    Q_PROPERTY(int baseline READ baseline NOTIFY changed)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY changed)
    Q_PROPERTY(QColor textColor READ textColor NOTIFY changed)
    Q_PROPERTY(QColor borderColor READ borderColor NOTIFY changed)
    Q_PROPERTY(int borderWidth READ borderWidth NOTIFY changed)

public:
    explicit LabelAdapter(QObject* parent = nullptr);

    TerminalView* view() const;
    void setView(TerminalView* view);
    QString label() const;
    void setLabel(const QString& label);

    QString text() const;
    QFont font() const;
    int alignment() const;
    // Where the label lays out its text, relative to the label.
    QRect contentsGeometry() const;
    // Where QPainter puts the (single) line's baseline in contentsGeometry, relative to the
    // label; Text's own vertical centring rounds differently (QQuickTextUtil::alignedY()).
    int baseline() const;
    QColor backgroundColor() const;
    QColor textColor() const;
    // The resize label's 1 px palette(dark) border; 0 width for the hover-link label.
    QColor borderColor() const;
    int borderWidth() const;

Q_SIGNALS:
    void viewChanged();
    void changed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void resolve();

    QPointer<TerminalView> _view;
    QPointer<QLabel> _label;
    QString _kind;
};
}
