/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QObject>
#include <QPalette>
#include <QPointer>
#include <QRect>
#include <QWidget>
#include <QtQml/qqmlregistration.h>

class QPainter;

namespace Konsole {
/** A facade's children changed stacking order (raised, added, removed). */
class KONSOLEPRIVATE_EXPORT StackingNotifier : public QObject {
    Q_OBJECT
public:
    static StackingNotifier* instance();

Q_SIGNALS:
    void changed(QWidget* facade);
};

/**
 * Where the facade's layout put one of its (hidden) child widgets, and whether it is shown,
 * so a QML part can be drawn in its place. Geometry is relative to the facade.
 */
class KONSOLEPRIVATE_EXPORT WidgetMirror : public QObject {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QRect geometry READ geometry NOTIFY geometryChanged)
    Q_PROPERTY(bool shown READ shown NOTIFY shownChanged)
    // The widget's place among its siblings (later is on top), for the QML part's z.
    Q_PROPERTY(int stackOrder READ stackOrder NOTIFY stackOrderChanged)

public:
    explicit WidgetMirror(QObject* parent = nullptr);

    QRect geometry() const;
    bool shown() const;
    int stackOrder() const;
    // The stacking rule for any child widget (QObject children order is stacking order).
    static int stackOrderOf(const QWidget* widget);
    // QWidget::initPainter(): the pen, background and font a widget's painter starts with.
    static void initPainter(QPainter* painter, const QWidget* widget, QPalette::ColorGroup group);

Q_SIGNALS:
    void geometryChanged();
    void shownChanged();
    void stackOrderChanged();

protected:
    void mirror(QWidget* widget, QWidget* facade);
    QWidget* mirrored() const;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void update();

    QPointer<QWidget> _widget;
    QPointer<QWidget> _facade;
    QRect _geometry;
    bool _shown = false;
    int _stackOrder = 0;
};
}
