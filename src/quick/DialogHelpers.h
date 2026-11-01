/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QColor>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQuickItem;

namespace Konsole {
/** What the QML dialogs need from widget code: the colour and title format choosers. */
class KONSOLEPRIVATE_EXPORT DialogHelpers : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit DialogHelpers(QObject* parent = nullptr);

    // KColorCombo's colours, and RenameTabWidget's "None" (the base colour, transparent).
    Q_INVOKABLE static QVariantList standardColors();
    Q_INVOKABLE static QColor noneColor();
    // QColorDialog, transient for the item's window; null when cancelled.
    Q_INVOKABLE static QVariant chooseColor(const QColor& initial, QQuickItem* from);
    // TabTitleFormatButton's menu below the item (Session::TabTitleContext); the chosen
    // element, empty when none.
    Q_INVOKABLE static QString chooseTitleFormat(int context, QQuickItem* below);
};
}
