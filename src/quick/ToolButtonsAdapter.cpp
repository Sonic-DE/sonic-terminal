/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ToolButtonsAdapter.h"

#include "CompatWindows.h"
#include "TabBarAdapter.h"

#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QPainter>
#include <QQuickWindow>
#include <QStyleOptionToolButton>
#include <QToolBar>
#include <QToolButton>

#include <Sonic/Logging/Log>

namespace Konsole {
namespace {
    // QToolButton::initStyleOption() and QToolButtonPrivate::styleButtonState(), with the
    // pressed and hovered state the QML side has (the hidden button has neither).
    QStyleOptionToolButton optionFor(const QToolButton* button, bool hovered, bool pressed, QPointF hoverPosition, bool active)
    {
        QStyleOptionToolButton option;
        option.initFrom(button);
        option.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken | QStyle::State_HasFocus | QStyle::State_Active);
        if (active) {
            option.state |= QStyle::State_Active;
        }
        option.palette = CompatWindows::palette(button);
        option.iconSize = button->iconSize();
        if (auto* toolBar = qobject_cast<QToolBar*>(button->parentWidget())) {
            option.iconSize = toolBar->iconSize();
        }
        option.text = button->text();
        option.icon = button->icon();
        option.arrowType = button->arrowType();
        const bool down = pressed && button->popupMode() != QToolButton::MenuButtonPopup;
        if (down) {
            option.state |= QStyle::State_Sunken;
        }
        if (button->isChecked()) {
            option.state |= QStyle::State_On;
        }
        if (button->autoRaise()) {
            option.state |= QStyle::State_AutoRaise;
        }
        if (!button->isChecked() && !down) {
            option.state |= QStyle::State_Raised;
        }
        option.subControls = QStyle::SC_ToolButton;
        option.activeSubControls = QStyle::SC_None;
        option.features = QStyleOptionToolButton::None;
        if (button->popupMode() == QToolButton::MenuButtonPopup) {
            option.subControls |= QStyle::SC_ToolButtonMenu;
            option.features |= QStyleOptionToolButton::MenuButtonPopup;
        }
        if (hovered && button->isEnabled()) {
            option.state |= QStyle::State_MouseOver;
            // QToolButtonPrivate::updateHoverControl(): the menu arrow or the button.
            const QRect arrow = button->style()->subControlRect(QStyle::CC_ToolButton, &option, QStyle::SC_ToolButtonMenu, button);
            option.activeSubControls = (button->popupMode() == QToolButton::MenuButtonPopup && arrow.contains(hoverPosition.toPoint()))
                ? QStyle::SC_ToolButtonMenu
                : QStyle::SC_ToolButton;
        }
        if (pressed && button->popupMode() == QToolButton::MenuButtonPopup) {
            option.state |= QStyle::State_Sunken;
            option.activeSubControls |= QStyle::SC_ToolButtonMenu;
        } else if (down) {
            option.activeSubControls |= QStyle::SC_ToolButton;
        }
        if (button->arrowType() != Qt::NoArrow) {
            option.features |= QStyleOptionToolButton::Arrow;
        }
        if (button->popupMode() == QToolButton::DelayedPopup) {
            option.features |= QStyleOptionToolButton::PopupDelay;
        }
        if (button->menu() || !button->actions().isEmpty() && button->defaultAction() && button->defaultAction()->menu()) {
            option.features |= QStyleOptionToolButton::HasMenu;
        }
        option.toolButtonStyle = button->toolButtonStyle();
        if (option.toolButtonStyle == Qt::ToolButtonFollowStyle) {
            option.toolButtonStyle = Qt::ToolButtonStyle(button->style()->styleHint(QStyle::SH_ToolButtonStyle, &option, button));
        }
        if (option.toolButtonStyle == Qt::ToolButtonTextBesideIcon && button->defaultAction() && button->defaultAction()->priority() < QAction::NormalPriority) {
            option.toolButtonStyle = Qt::ToolButtonIconOnly;
        }
        if (button->icon().isNull() && button->arrowType() == Qt::NoArrow) {
            if (!button->text().isEmpty()) {
                option.toolButtonStyle = Qt::ToolButtonTextOnly;
            } else if (option.toolButtonStyle != Qt::ToolButtonTextOnly) {
                option.toolButtonStyle = Qt::ToolButtonIconOnly;
            }
        }
        option.pos = button->pos();
        option.font = button->font();
        return option;
    }
}

ToolButtonsAdapter::ToolButtonsAdapter(QObject* parent)
    : QObject(parent)
{
    connect(CompatWindowsNotifier::instance(), &CompatWindowsNotifier::activeChanged, this, &ToolButtonsAdapter::refresh);
}

QObject* ToolButtonsAdapter::root() const
{
    return _root;
}

void ToolButtonsAdapter::setRoot(QObject* root)
{
    auto* widget = qobject_cast<QWidget*>(root);
    if (_root == widget) {
        return;
    }
    _root = widget;
    if (_root) {
        watch(_root);
    }
    Q_EMIT rootChanged();
    refresh();
}

void ToolButtonsAdapter::watch(QWidget* widget)
{
    widget->removeEventFilter(this);
    widget->installEventFilter(this);
    for (QWidget* child : widget->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
        watch(child);
    }
}

bool ToolButtonsAdapter::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::ChildAdded:
        if (auto* child = qobject_cast<QWidget*>(static_cast<QChildEvent*>(event)->child())) {
            watch(child);
        }
        refresh();
        break;
    case QEvent::ChildRemoved:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::ShowToParent:
    case QEvent::HideToParent:
    case QEvent::EnabledChange:
    case QEvent::PaletteChange:
    case QEvent::FontChange:
    case QEvent::StyleChange:
    case QEvent::ToolTipChange:
    case QEvent::ActionChanged:
    case QEvent::ParentChange:
        refresh();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

QVariantList ToolButtonsAdapter::buttons() const
{
    return _list;
}

void ToolButtonsAdapter::refresh()
{
    _buttons.clear();
    QVariantList list;
    if (!_root) {
        _list = list;
        Q_EMIT changed();
        return;
    }
    QList<QToolButton*> found = _root->findChildren<QToolButton*>();
    if (auto* rootButton = qobject_cast<QToolButton*>(_root.data())) {
        found.prepend(rootButton);
    }
    QWidget* window = _root->window();
    for (QToolButton* button : std::as_const(found)) {
        if (!button->isVisibleTo(window)) {
            continue;
        }
        _buttons.append(button);
        QStyleOptionToolButton option = optionFor(button, false, false, {}, false);
        option.rect = button->rect();
        const QRect arrow = button->popupMode() == QToolButton::MenuButtonPopup
            ? button->style()->subControlRect(QStyle::CC_ToolButton, &option, QStyle::SC_ToolButtonMenu, button)
            : QRect();
        list.append(QVariantMap{{QStringLiteral("geometry"), QRect(button->mapTo(window, QPoint(0, 0)), button->size())},
            {QStringLiteral("toolTip"), button->toolTip()},
            {QStringLiteral("enabled"), button->isEnabled()},
            {QStringLiteral("menuArrowGeometry"), arrow},
            {QStringLiteral("iconName"), button->icon().name()},
            {QStringLiteral("text"), button->toolButtonStyle() == Qt::ToolButtonIconOnly ? QString() : button->text()},
            {QStringLiteral("hasMenu"), button->menu() || (button->defaultAction() && button->defaultAction()->menu())}});
    }
    _list = list;
    Q_EMIT changed();
}

int ToolButtonsAdapter::toolTipDelay() const
{
    return QApplication::style()->styleHint(QStyle::SH_ToolTip_WakeUpDelay);
}

QToolButton* ToolButtonsAdapter::button(int index) const
{
    return index >= 0 && index < _buttons.size() ? _buttons.at(index).data() : nullptr;
}

void ToolButtonsAdapter::popupMenu(QToolButton* button)
{
    QMenu* menu = button->menu();
    if (!menu && button->defaultAction()) {
        menu = button->defaultAction()->menu();
    }
    if (!menu) {
        return;
    }
    // QToolButton::showMenu() positions it below the button; the compat window shares the
    // QML window's geometry, so the hidden button's screen position is the QML button's.
    Q_EMIT button->triggered(menu->menuAction());
    sonicDebug() << "menu popup" << menu->objectName();
    menu->popup(button->mapToGlobal(QPoint(0, button->height())));
}

void ToolButtonsAdapter::press(int index, QPointF position)
{
    QToolButton* toolButton = button(index);
    _menuOpenedOnPress = false;
    if (!toolButton || !toolButton->isEnabled()) {
        return;
    }
    const bool hasMenu = toolButton->menu() || (toolButton->defaultAction() && toolButton->defaultAction()->menu());
    QStyleOptionToolButton option = optionFor(toolButton, false, false, {}, false);
    option.rect = toolButton->rect();
    const QRect arrow = toolButton->style()->subControlRect(QStyle::CC_ToolButton, &option, QStyle::SC_ToolButtonMenu, toolButton);
    // QToolButton::mousePressEvent().
    if (hasMenu
        && (toolButton->popupMode() == QToolButton::InstantPopup
            || (toolButton->popupMode() == QToolButton::MenuButtonPopup && arrow.contains(position.toPoint())))) {
        _menuOpenedOnPress = true;
        popupMenu(toolButton);
    }
}

void ToolButtonsAdapter::holdPress(int index)
{
    // QToolButton's DelayedPopup: holding the button opens the menu.
    QToolButton* toolButton = button(index);
    if (toolButton && toolButton->popupMode() == QToolButton::DelayedPopup && !_menuOpenedOnPress) {
        _menuOpenedOnPress = true;
        popupMenu(toolButton);
    }
}

void ToolButtonsAdapter::release(int index, QPointF position, bool inside)
{
    Q_UNUSED(position)
    QToolButton* toolButton = button(index);
    if (toolButton && toolButton->isEnabled() && inside && !_menuOpenedOnPress) {
        toolButton->click();
    }
    _menuOpenedOnPress = false;
}
}

#include "moc_ToolButtonsAdapter.cpp"
