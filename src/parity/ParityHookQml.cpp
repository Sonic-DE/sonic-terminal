// SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ParityHookQml.h"

#include "MainWindow.h"
#include "quick/ChromeAdapter.h"
#include "quick/WindowController.h"

#include <QApplication>
#include <QJSValue>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QQmlEngine>
#include <QQuickItem>
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

namespace {
    bool isDialog(const QWindow* window)
    {
        for (const QMetaObject* meta = window->metaObject(); meta; meta = meta->superClass()) {
            if (QByteArray(meta->className()).startsWith("DialogWindow_QMLTYPE")) {
                return true;
            }
        }
        return false;
    }
}

QList<QWindow*> qmlDialogs()
{
    QList<QWindow*> dialogs;
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* window : windows) {
        if (window->isVisible() && isDialog(window)) {
            dialogs.append(window);
        }
    }
    return dialogs;
}

QWindow* qmlDialog(const QString& title)
{
    QWindow* focused = QGuiApplication::focusWindow();
    if (focused && isDialog(focused) && (title.isEmpty() || focused->title() == title)) {
        return focused;
    }
    if (!title.isEmpty()) {
        const QList<QWindow*> dialogs = qmlDialogs();
        for (QWindow* dialog : dialogs) {
            if (dialog->title() == title) {
                return dialog;
            }
        }
    }
    return nullptr;
}

QObject* qmlDialogButton(QWindow* dialog, const QString& object, const QString& text, int standardButton)
{
    auto* quick = qobject_cast<QQuickWindow*>(dialog);
    if (!quick) {
        return nullptr;
    }
    if (!object.isEmpty()) {
        return quick->findChild<QQuickItem*>(object);
    }
    if (standardButton) {
        // DialogButtonBox::standardButton(), through the engine (its return type is private).
        QQuickItem* box = nullptr;
        const auto items = quick->findChildren<QQuickItem*>();
        for (QQuickItem* item : items) {
            box = box ? box : (item->inherits("QQuickDialogButtonBox") ? item : nullptr);
        }
        QQmlEngine* engine = box ? qmlEngine(box) : nullptr;
        if (!engine) {
            return nullptr;
        }
        QJSValue boxValue = engine->toScriptValue(box);
        return boxValue.property(QStringLiteral("standardButton")).callWithInstance(boxValue, {QJSValue(standardButton)}).toQObject();
    }
    QList<QQuickItem*> items = {quick->contentItem()};
    while (!items.isEmpty()) {
        QQuickItem* item = items.takeFirst();
        if (item->inherits("QQuickAbstractButton") && item->isVisible() && item->property("text").toString().remove(u'&') == text) {
            return item;
        }
        items += item->childItems();
    }
    return nullptr;
}

QPixmap grabQmlDialog(QWindow* dialog)
{
    auto* quick = qobject_cast<QQuickWindow*>(dialog);
    return quick ? QPixmap::fromImage(quick->grabWindow()) : QPixmap();
}

void rejectQmlDialog(QWindow* dialog)
{
    QMetaObject::invokeMethod(dialog, "reject");
}
}
