// SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ParityHookQml.h"

#include "MainWindow.h"
#include "quick/ChromeAdapter.h"
#include "quick/WindowController.h"

#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QQuickWindow>

namespace Konsole::ParityHook {
QWindow* qmlWindow(const MainWindow* window)
{
    WindowController* controller = WindowController::controllerOf(window);
    return controller ? controller->qmlWindow() : nullptr;
}

bool popupQmlMenu(const MainWindow* window, const QMenu* menu)
{
    QWindow* qml = qmlWindow(window);
    auto* chrome = qml ? qml->findChild<ChromeAdapter*>() : nullptr;
    if (!chrome) {
        return false;
    }
    const QVariantList menus = chrome->menus();
    for (int i = 0; i < menus.size(); ++i) {
        if (menus.at(i).toMap().value(QStringLiteral("menu")).value<QObject*>() == menu) {
            chrome->popupMenu(i);
            return true;
        }
    }
    return false;
}

QPixmap grabQmlWindow(const MainWindow* window)
{
    auto* qml = qobject_cast<QQuickWindow*>(qmlWindow(window));
    if (!qml) {
        return QPixmap();
    }
    // grabWindow() keeps a translucent window's alpha; QWidget::grab() of the widget window,
    // which the baseline holds, gives it over black.
    const QImage image = qml->grabWindow();
    QPixmap pixmap(image.size());
    pixmap.fill(Qt::black);
    QPainter painter(&pixmap);
    painter.drawImage(0, 0, image);
    // grabWindow() renders the scene only; embedded widgets (plugin docks) are drawn where
    // their native windows are.
    const QList<QWindow*> children = qml->findChildren<QWindow*>(Qt::FindDirectChildrenOnly);
    const QList<QWidget*> widgets = QApplication::topLevelWidgets();
    for (QWindow* child : children) {
        for (QWidget* widget : widgets) {
            if (child->isVisible() && widget->windowHandle() == child) {
                painter.drawPixmap(child->position(), widget->grab());
            }
        }
    }
    return pixmap;
}
}
