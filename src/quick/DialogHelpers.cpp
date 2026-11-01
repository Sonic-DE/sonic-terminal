/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DialogHelpers.h"

#include "widgets/TabTitleFormatButton.h"

#include <QColorDialog>
#include <QCursor>
#include <QGuiApplication>
#include <QMenu>
#include <QPalette>
#include <QQuickItem>
#include <QQuickWindow>

namespace Konsole {
DialogHelpers::DialogHelpers(QObject* parent)
    : QObject(parent)
{
}

QVariantList DialogHelpers::standardColors()
{
    // KColorCombo's standard palette (kcolorcombo.cpp), in its order.
    static const uchar palette[][3] = {{255, 255, 255}, {192, 192, 192}, {160, 160, 160}, {128, 128, 128}, {0, 0, 0}, {255, 128, 128}, {255, 192, 128},
        {255, 255, 128}, {128, 255, 128}, {128, 255, 255}, {128, 128, 255}, {255, 128, 255}, {255, 0, 0}, {255, 128, 0}, {255, 255, 0}, {0, 255, 0},
        {0, 255, 255}, {0, 0, 255}, {255, 0, 255}, {128, 0, 0}, {128, 64, 0}, {128, 128, 0}, {0, 128, 0}, {0, 128, 128}, {0, 0, 128}, {128, 0, 128}};
    QVariantList colors;
    for (const auto& entry : palette) {
        colors.append(QColor(entry[0], entry[1], entry[2]));
    }
    return colors;
}

QColor DialogHelpers::noneColor()
{
    QColor color = QGuiApplication::palette().base().color();
    color.setAlpha(0);
    return color;
}

QVariant DialogHelpers::chooseColor(const QColor& initial, QQuickItem* from)
{
    QColorDialog dialog(initial);
    if (from && from->window()) {
        dialog.winId();
        dialog.windowHandle()->setTransientParent(from->window());
    }
    return dialog.exec() == QDialog::Accepted ? QVariant(dialog.selectedColor()) : QVariant();
}

QString DialogHelpers::chooseTitleFormat(int context, QQuickItem* below)
{
    QMenu menu;
    menu.addActions(TabTitleFormatButton::elementActions(static_cast<Session::TabTitleContext>(context), &menu));
    const QPoint position = below ? below->mapToGlobal(QPointF(0, below->height())).toPoint() : QCursor::pos();
    QAction* chosen = menu.exec(position);
    return chosen ? chosen->data().toString() : QString();
}
}

#include "moc_DialogHelpers.cpp"
