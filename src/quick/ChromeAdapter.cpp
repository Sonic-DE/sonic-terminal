/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ChromeAdapter.h"

#include <Sonic/Logging/Log>

#include <KXMLGUIFactory>
#include <KXmlGuiWindow>

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>

namespace Konsole {
namespace {
    int areaIndex(Qt::ToolBarArea area)
    {
        switch (area) {
        case Qt::BottomToolBarArea:
            return 1;
        case Qt::LeftToolBarArea:
            return 2;
        case Qt::RightToolBarArea:
            return 3;
        default:
            return 0;
        }
    }

    QMenuBar* menuBarOf(QMainWindow* window)
    {
        // menuBar() would create one.
        return window ? qobject_cast<QMenuBar*>(window->menuWidget()) : nullptr;
    }
}

ChromeAdapter::ChromeAdapter(QObject* parent)
    : QObject(parent)
{
}

ChromeAdapter::~ChromeAdapter() = default;

QObject* ChromeAdapter::window() const
{
    return _window;
}

void ChromeAdapter::setWindow(QObject* window)
{
    auto* mainWindow = qobject_cast<QMainWindow*>(window);
    if (mainWindow == _window) {
        return;
    }
    for (const QPointer<QObject>& watched : std::as_const(_watched)) {
        if (watched) {
            watched->removeEventFilter(this);
        }
    }
    _watched.clear();
    if (auto* gui = qobject_cast<KXmlGuiWindow*>(_window)) {
        if (gui->guiFactory()) {
            gui->guiFactory()->disconnect(this);
        }
    }
    _window = mainWindow;
    if (_window) {
        watch(_window);
        if (auto* gui = qobject_cast<KXmlGuiWindow*>(_window); gui && gui->guiFactory()) {
            // Configure Toolbars and plugging a session's client rebuild the bars.
            connect(gui->guiFactory(), &KXMLGUIFactory::makingChanges, this, [this](bool changing) {
                if (!changing) {
                    scheduleRefresh();
                }
            });
        }
    }
    refresh();
    Q_EMIT windowChanged();
}

bool ChromeAdapter::menuBarVisible() const
{
    const QMenuBar* bar = menuBarOf(_window);
    return bar && bar->isVisibleTo(_window);
}

QObject* ChromeAdapter::menuBarWidget() const
{
    return menuBarOf(_window);
}

QVariantList ChromeAdapter::menus() const
{
    return _menus;
}

QVariantList ChromeAdapter::topLines() const
{
    return _lines[0];
}

QVariantList ChromeAdapter::bottomLines() const
{
    return _lines[1];
}

QVariantList ChromeAdapter::leftLines() const
{
    return _lines[2];
}

QVariantList ChromeAdapter::rightLines() const
{
    return _lines[3];
}

QList<QQuickItem*> ChromeAdapter::menuBarItems() const
{
    return _menuBarItems;
}

void ChromeAdapter::setMenuBarItems(const QList<QQuickItem*>& items)
{
    if (items != _menuBarItems) {
        _menuBarItems = items;
        Q_EMIT menuBarItemsChanged();
    }
}

QMenu* ChromeAdapter::openMenu() const
{
    return _openMenu;
}

int ChromeAdapter::openIndex() const
{
    return _openMenu ? _openIndex : -1;
}

void ChromeAdapter::watch(QObject* object)
{
    if (!object || _watched.contains(object)) {
        return;
    }
    object->installEventFilter(this);
    _watched.append(object);
}

void ChromeAdapter::scheduleRefresh()
{
    if (_refreshScheduled) {
        return;
    }
    _refreshScheduled = true;
    QTimer::singleShot(0, this, [this] {
        _refreshScheduled = false;
        refresh();
    });
}

void ChromeAdapter::refresh()
{
    QVariantList menus;
    QVariantList lines[4];
    if (QMenuBar* bar = menuBarOf(_window)) {
        watch(bar);
        const QList<QAction*> actions = bar->actions();
        for (QAction* action : actions) {
            if (action->menu() && action->isVisible()) {
                // As QObject *: QML doesn't unwrap a QVariant holding a QAction *.
                menus.append(QVariantMap{{QStringLiteral("action"), QVariant::fromValue<QObject*>(action)},
                    {QStringLiteral("menu"), QVariant::fromValue<QObject*>(action->menu())},
                    {QStringLiteral("text"), action->text()}});
            }
        }
    }
    if (_window) {
        QList<QToolBar*> toolBars = _window->findChildren<QToolBar*>(Qt::FindDirectChildrenOnly);
        // Lines in order, then toolbars along each line, from the laid-out compat window.
        std::sort(toolBars.begin(), toolBars.end(), [](const QToolBar* a, const QToolBar* b) {
            const bool horizontal = a->orientation() == Qt::Horizontal;
            const int lineA = horizontal ? a->y() : a->x();
            const int lineB = horizontal ? b->y() : b->x();
            return lineA != lineB ? lineA < lineB : (horizontal ? a->x() < b->x() : a->y() < b->y());
        });
        int lastArea = -1;
        int lastLine = 0;
        for (QToolBar* toolBar : std::as_const(toolBars)) {
            watch(toolBar);
            if (toolBar->isHidden()) {
                continue;
            }
            const int area = areaIndex(_window->toolBarArea(toolBar));
            const int line = toolBar->orientation() == Qt::Horizontal ? toolBar->y() : toolBar->x();
            if (lines[area].isEmpty() || lastArea != area || line != lastLine || _window->toolBarBreak(toolBar)) {
                // As one element: append(QVariantList) would add the (no) elements instead.
                lines[area].append(QVariant(QVariantList()));
            }
            lastArea = area;
            lastLine = line;

            QVariantList items;
            const QList<QAction*> actions = toolBar->actions();
            for (QAction* action : actions) {
                if (!action->isVisible()) {
                    continue;
                }
                QWidget* widget = toolBar->widgetForAction(action);
                auto* button = qobject_cast<QToolButton*>(widget);
                const bool spacer = widget && !button && !action->isSeparator();
                if (spacer && !(widget->sizePolicy().expandingDirections() & toolBar->orientation())) {
                    sonicWarning() << "toolbar widget not shown in the QML window:" << widget->metaObject()->className();
                    continue;
                }
                const bool hasMenu = button && (button->menu() || action->menu());
                items.append(QVariantMap{{QStringLiteral("action"), QVariant::fromValue<QObject*>(action)},
                    {QStringLiteral("iconName"), action->icon().name()},
                    {QStringLiteral("separator"), action->isSeparator()},
                    {QStringLiteral("spacer"), spacer},
                    // The button's own: KHamburgerMenu's is icon-only.
                    {QStringLiteral("buttonStyle"), int(button ? button->toolButtonStyle() : toolBar->toolButtonStyle())},
                    {QStringLiteral("popupMode"), hasMenu ? int(button->popupMode()) : -1}});
            }
            QVariantList toolBarsOfLine = lines[area].takeLast().toList();
            toolBarsOfLine.append(QVariantMap{{QStringLiteral("toolBar"), QVariant::fromValue<QObject*>(toolBar)},
                {QStringLiteral("buttonStyle"), int(toolBar->toolButtonStyle())},
                {QStringLiteral("iconSize"), toolBar->iconSize().width()},
                {QStringLiteral("items"), items}});
            lines[area].append(QVariant(toolBarsOfLine));
        }
    }
    const bool menuBarShown = menuBarVisible();
    bool changed = menus != _menus || menuBarShown != _menuBarShown;
    _menus = menus;
    _menuBarShown = menuBarShown;
    for (int area = 0; area < 4; ++area) {
        changed = changed || lines[area] != _lines[area];
        _lines[area] = lines[area];
    }
    if (changed) {
        sonicDebug() << "chrome:" << _menus.size() << "menus," << _lines[0].size() << "top toolbar lines";
        Q_EMIT this->changed();
    }
}

void ChromeAdapter::popupMenu(int index)
{
    const QVariantMap entry = _menus.value(index).toMap();
    if (auto* menu = qobject_cast<QMenu*>(entry.value(QStringLiteral("menu")).value<QObject*>())) {
        popup(menu, _menuBarItems.value(index), index);
    }
}

void ChromeAdapter::trigger(QObject* object)
{
    auto* action = qobject_cast<QAction*>(object);
    if (action && action->isEnabled()) {
        action->trigger();
    }
}

QToolBar* ChromeAdapter::toolBarOf(QAction* action) const
{
    if (!_window || !action) {
        return nullptr;
    }
    const QList<QToolBar*> toolBars = _window->findChildren<QToolBar*>(Qt::FindDirectChildrenOnly);
    for (QToolBar* toolBar : toolBars) {
        if (toolBar->actions().contains(action)) {
            return toolBar;
        }
    }
    return nullptr;
}

void ChromeAdapter::popupButtonMenu(QObject* object, QQuickItem* button)
{
    auto* action = qobject_cast<QAction*>(object);
    QToolBar* toolBar = toolBarOf(action);
    auto* widget = toolBar ? qobject_cast<QToolButton*>(toolBar->widgetForAction(action)) : nullptr;
    QMenu* menu = widget && widget->menu() ? widget->menu() : (action ? action->menu() : nullptr);
    if (menu) {
        popup(menu, button, -1);
    }
}

void ChromeAdapter::actionTriggered(QObject* action, QQuickItem* button)
{
    // KHamburgerMenu opens its menu under its own button only in an active window, which a
    // compat window never is, and at the pointer otherwise: under the QML button instead,
    // once the trigger has opened it (the toolbars may be rebuilt meanwhile, so the button
    // showing the action then answers buttonMenuRequested()).
    if (!action || !action->inherits("KHamburgerMenu") || !button || !button->window() || !button->window()->isActive()) {
        return;
    }
    QMetaObject::invokeMethod(
        this,
        [this, action = QPointer<QObject>(action)] {
            if (action && qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
                Q_EMIT buttonMenuRequested(action);
            }
        },
        Qt::QueuedConnection);
}

void ChromeAdapter::moveOpenMenu(QQuickItem* button)
{
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (menu && button && button->window()) {
        menu->hide();
        popup(menu, button, -1);
    }
}

void ChromeAdapter::toolBarContextMenu(QObject* toolBar, QQuickItem* item, QPointF position)
{
    auto* bar = qobject_cast<QToolBar*>(toolBar);
    if (!bar || !item) {
        return;
    }
    // WindowController keeps the compat window's native geometry on the QML window's, so
    // KToolBar's mapFromGlobal() finds the button under the pointer.
    const QPoint global = item->mapToGlobal(position).toPoint();
    QContextMenuEvent event(QContextMenuEvent::Mouse, bar->mapFromGlobal(global), global);
    QCoreApplication::sendEvent(bar, &event);
}

void ChromeAdapter::setExtent(QObject* bar, int extent)
{
    auto* widget = qobject_cast<QWidget*>(bar);
    if (!widget || extent <= 0) {
        return;
    }
    auto* toolBar = qobject_cast<QToolBar*>(widget);
    if (toolBar && toolBar->orientation() == Qt::Vertical) {
        if (widget->minimumWidth() != extent || widget->maximumWidth() != extent) {
            widget->setFixedWidth(extent);
        }
    } else if (widget->minimumHeight() != extent || widget->maximumHeight() != extent) {
        widget->setFixedHeight(extent);
    }
}

void ChromeAdapter::popup(QMenu* menu, QQuickItem* item, int menuIndex)
{
    if (!item || !item->window()) {
        return;
    }
    if (_openMenu && _openMenu != menu) {
        _openMenu->hide();
    }
    const QPoint position = item->mapToGlobal(QPointF(0, item->height())).toPoint();
    // A popup's window manager parent: the QML window, not the off-screen compat window.
    menu->winId();
    if (QWindow* handle = menu->windowHandle()) {
        handle->setTransientParent(item->window());
    }
    if (menuIndex >= 0) {
        _openMenu = menu;
        _openIndex = menuIndex;
        menu->installEventFilter(this);
        connect(
            menu,
            &QMenu::aboutToHide,
            this,
            [this, menu] {
                menu->removeEventFilter(this);
                if (_openMenu == menu) {
                    _openMenu = nullptr;
                    _openIndex = -1;
                    Q_EMIT openMenuChanged();
                }
            },
            Qt::SingleShotConnection);
        Q_EMIT openMenuChanged();
    }
    sonicDebug() << "popup" << menu->objectName();
    menu->popup(position);
}

bool ChromeAdapter::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _openMenu && _openIndex >= 0) {
        const int count = _menus.size();
        int next = -1;
        if (event->type() == QEvent::KeyPress && count > 1) {
            // A menu popped up outside its QMenuBar has nobody to hand Left/Right to.
            const int key = static_cast<QKeyEvent*>(event)->key();
            const bool rtl = QGuiApplication::layoutDirection() == Qt::RightToLeft;
            const QAction* active = _openMenu->activeAction();
            if (key == (rtl ? Qt::Key_Left : Qt::Key_Right) && !(active && active->menu())) {
                next = (_openIndex + 1) % count;
            } else if (key == (rtl ? Qt::Key_Right : Qt::Key_Left)) {
                next = (_openIndex + count - 1) % count;
            }
        } else if (event->type() == QEvent::MouseMove) {
            // QMenuBar's rule: while a menu is open, hovering another item opens its menu.
            const QPointF global = static_cast<QMouseEvent*>(event)->globalPosition();
            for (int i = 0; i < _menuBarItems.size(); ++i) {
                QQuickItem* item = _menuBarItems.at(i);
                if (i != _openIndex && item && item->isVisible() && item->contains(item->mapFromGlobal(global))) {
                    next = i;
                    break;
                }
            }
        }
        if (next >= 0) {
            _openMenu->hide();
            QMetaObject::invokeMethod(this, [this, next] { popupMenu(next); }, Qt::QueuedConnection);
            return event->type() == QEvent::KeyPress;
        }
        return false;
    }
    switch (event->type()) {
    case QEvent::ActionAdded:
    case QEvent::ActionRemoved:
    case QEvent::ActionChanged:
    case QEvent::ChildAdded:
    case QEvent::ChildRemoved:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::ShowToParent:
    case QEvent::HideToParent:
    case QEvent::Move:
    case QEvent::LayoutRequest:
        scheduleRefresh();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}
}

#include "moc_ChromeAdapter.cpp"
