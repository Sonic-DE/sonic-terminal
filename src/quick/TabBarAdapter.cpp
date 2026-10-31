/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "TabBarAdapter.h"

#include "CompatWindows.h"
#include "KonsoleSettings.h"
#include "widgets/DetachableTabBar.h"
#include "widgets/ViewContainer.h"

#include <QAbstractButton>
#include <QEvent>
#include <QMainWindow>
#include <QPainter>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QStyleOptionTab>
#include <QStyleOptionTabBarBase>
#include <QStyleOptionToolButton>
#include <QToolButton>
#include <QWheelEvent>

namespace Konsole {
namespace {
    QToolButton* scrollButton(const QTabBar* tabBar, const char* name)
    {
        return tabBar->findChild<QToolButton*>(QLatin1String(name), Qt::FindDirectChildrenOnly);
    }
}

TabBarAdapter::TabBarAdapter(QObject* parent)
    : WidgetMirror(parent)
{
    connect(CompatWindowsNotifier::instance(), &CompatWindowsNotifier::activeChanged, this, &TabBarAdapter::changed);
}

QObject* TabBarAdapter::window() const
{
    return _window;
}

void TabBarAdapter::setWindow(QObject* window)
{
    auto* widget = qobject_cast<QWidget*>(window);
    if (_window == widget) {
        return;
    }
    if (_container) {
        _container->disconnect(this);
        _container->removeEventFilter(this);
    }
    if (_tabBar) {
        _tabBar->disconnect(this);
        _tabBar->removeEventFilter(this);
    }
    _window = widget;
    auto* mainWindow = qobject_cast<QMainWindow*>(widget);
    _container = mainWindow ? qobject_cast<TabbedViewContainer*>(mainWindow->centralWidget()) : nullptr;
    _tabBar = _container ? qobject_cast<DetachableTabBar*>(_container->tabBar()) : nullptr;
    mirror(_tabBar, _window);
    if (_container) {
        // Settings replace the corner widgets (TabbedViewContainer::konsoleConfigChanged()).
        _container->installEventFilter(this);
    }
    if (_tabBar) {
        _tabBar->installEventFilter(this);
        for (QWidget* child : _tabBar->findChildren<QWidget*>()) {
            child->installEventFilter(this);
        }
        connect(_tabBar, &QTabBar::currentChanged, this, &TabBarAdapter::changed);
        connect(_tabBar, &QTabBar::tabMoved, this, &TabBarAdapter::changed);
        // The container's tab data changes reach the tab bar first.
        connect(_container, &TabbedViewContainer::tabsChanged, this, &TabBarAdapter::changed, Qt::QueuedConnection);
        connect(_container, &TabbedViewContainer::setColor, this, &TabBarAdapter::changed, Qt::QueuedConnection);
        connect(_container, &TabbedViewContainer::removeColor, this, &TabBarAdapter::changed, Qt::QueuedConnection);
        connect(_container, &TabbedViewContainer::setProgress, this, &TabBarAdapter::changed, Qt::QueuedConnection);
        connect(_container, &TabbedViewContainer::setActivityColor, this, &TabBarAdapter::changed, Qt::QueuedConnection);
    }
    Q_EMIT windowChanged();
    Q_EMIT cornersChanged();
    Q_EMIT changed();
}

QObject* TabBarAdapter::leftCorner() const
{
    return _container ? _container->cornerWidget(Qt::TopLeftCorner) : nullptr;
}

QObject* TabBarAdapter::rightCorner() const
{
    return _container ? _container->cornerWidget(Qt::TopRightCorner) : nullptr;
}

bool TabBarAdapter::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _container) {
        if (event->type() == QEvent::ChildAdded || event->type() == QEvent::ChildRemoved) {
            Q_EMIT cornersChanged();
        }
        return WidgetMirror::eventFilter(watched, event);
    }
    switch (event->type()) {
    case QEvent::ChildAdded:
        if (watched == _tabBar) {
            if (auto* child = qobject_cast<QWidget*>(static_cast<QChildEvent*>(event)->child())) {
                child->installEventFilter(this);
            }
        }
        Q_EMIT changed();
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
    case QEvent::LayoutRequest:
        Q_EMIT changed();
        break;
    default:
        break;
    }
    return WidgetMirror::eventFilter(watched, event);
}

QList<QWidget*> TabBarAdapter::buttons() const
{
    QList<QWidget*> buttons;
    if (!_tabBar) {
        return buttons;
    }
    const auto side = QTabBar::ButtonPosition(_tabBar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, _tabBar));
    for (int i = 0; i < _tabBar->count(); ++i) {
        QWidget* button = _tabBar->tabButton(i, side);
        if (button && button->isVisibleTo(_tabBar)) {
            buttons.append(button);
        }
    }
    for (const char* name : {"ScrollLeftButton", "ScrollRightButton"}) {
        QToolButton* button = scrollButton(_tabBar, name);
        if (button && button->isVisibleTo(_tabBar)) {
            buttons.append(button);
        }
    }
    return buttons;
}

QVariantList TabBarAdapter::tabs() const
{
    QVariantList list;
    if (!_tabBar) {
        return list;
    }
    for (int i = 0; i < _tabBar->count(); ++i) {
        QStyleOptionTab option;
        _tabBar->initStyleOption(&option, i);
        const QColor textColor = _tabBar->tabTextColor(i);
        const auto data = _tabBar->tabData(i).value<DetachableTabData>();
        // DetachableTabBar::paintEvent()'s colour/progress bar.
        QRect strip = _tabBar->tabRect(i);
        strip.setTop(_tabBar->fontMetrics().height() + 6);
        strip.setHeight(4);
        strip.setLeft(strip.left() + 6);
        strip.setWidth(strip.width() - 6);
        // Where the style puts the text (beside the icon and buttons), in the tab.
        const QRect textRect = _tabBar->style()->subElementRect(QStyle::SE_TabBarTabText, &option, _tabBar).translated(-option.rect.topLeft());
        // The text without mnemonic markers ("&&" shows "&").
        QString label = option.text;
        label.remove(QRegularExpression(QStringLiteral("&(?!&)")));
        list.append(QVariantMap{{QStringLiteral("rect"), _tabBar->tabRect(i)},
            {QStringLiteral("stripRect"), strip},
            {QStringLiteral("text"), option.text},
            {QStringLiteral("label"), label},
            {QStringLiteral("textRect"), textRect},
            {QStringLiteral("iconSize"), option.iconSize},
            {QStringLiteral("iconName"), _tabBar->tabIcon(i).name()},
            {QStringLiteral("toolTip"), _tabBar->tabToolTip(i)},
            {QStringLiteral("textColor"), textColor},
            {QStringLiteral("color"), data.color.isValid() && data.color.alpha() > 0 ? data.color : QColor()},
            {QStringLiteral("progress"), data.progress.value_or(-1)},
            {QStringLiteral("visible"), _tabBar->isTabVisible(i)}});
    }
    return list;
}

int TabBarAdapter::currentIndex() const
{
    return _tabBar ? _tabBar->currentIndex() : -1;
}

bool TabBarAdapter::atBottom() const
{
    return _container && _container->tabPosition() == QTabWidget::South;
}

QVariantList TabBarAdapter::closeButtons() const
{
    QVariantList list;
    if (!_tabBar) {
        return list;
    }
    const auto side = QTabBar::ButtonPosition(_tabBar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, _tabBar));
    for (int i = 0; i < _tabBar->count(); ++i) {
        QWidget* button = _tabBar->tabButton(i, side);
        if (button && button->isVisibleTo(_tabBar)) {
            list.append(QVariantMap{{QStringLiteral("geometry"), button->geometry()},
                {QStringLiteral("index"), i},
                {QStringLiteral("toolTip"), button->toolTip()}});
        }
    }
    return list;
}

QVariantList TabBarAdapter::scrollButtons() const
{
    QVariantList list;
    if (!_tabBar) {
        return list;
    }
    for (const char* name : {"ScrollLeftButton", "ScrollRightButton"}) {
        QToolButton* button = scrollButton(_tabBar, name);
        if (button && button->isVisibleTo(_tabBar)) {
            list.append(QVariantMap{{QStringLiteral("geometry"), button->geometry()}, {QStringLiteral("enabled"), button->isEnabled()}});
        }
    }
    return list;
}

int TabBarAdapter::tabAt(QPointF position) const
{
    return _tabBar ? _tabBar->tabAt(position.toPoint()) : -1;
}

void TabBarAdapter::press(QPointF position, int button)
{
    if (!_tabBar) {
        return;
    }
    // QTabBar::mousePressEvent(): left presses select (SH_TabBar_SelectMouseType).
    const int index = _tabBar->tabAt(position.toPoint());
    Q_EMIT _tabBar->tabBarClicked(index);
    if (button != Qt::LeftButton || index < 0) {
        return;
    }
    QStyleOptionTabBarBase option;
    option.initFrom(_tabBar);
    option.documentMode = _tabBar->documentMode();
    if (_tabBar->style()->styleHint(QStyle::SH_TabBar_SelectMouseType, &option, _tabBar) == QEvent::MouseButtonPress) {
        _tabBar->setCurrentIndex(index);
    }
}

void TabBarAdapter::release(QPointF position, int button, int pressedIndex)
{
    if (!_tabBar) {
        return;
    }
    const int index = _tabBar->tabAt(position.toPoint());
    if (button == Qt::MiddleButton) {
        // DetachableTabBar::mouseReleaseEvent() (QTabBar's own middle-click close is blocked).
        if (KonsoleSettings::closeTabOnMiddleMouseButton() && index != -1) {
            Q_EMIT _tabBar->closeTab(index);
        }
        if (index == -1) {
            Q_EMIT _tabBar->newTabRequest();
        }
        return;
    }
    if (button != Qt::LeftButton) {
        return;
    }
    QStyleOptionTabBarBase option;
    option.initFrom(_tabBar);
    option.documentMode = _tabBar->documentMode();
    if (_tabBar->style()->styleHint(QStyle::SH_TabBar_SelectMouseType, &option, _tabBar) == QEvent::MouseButtonRelease) {
        _tabBar->setCurrentIndex(index == pressedIndex ? pressedIndex : -1);
    }
}

void TabBarAdapter::doubleClick(QPointF position, int button)
{
    // DetachableTabBar::mouseDoubleClickEvent() takes the left button only.
    if (!_tabBar || button != Qt::LeftButton) {
        return;
    }
    const QPoint point = position.toPoint();
    for (const char* name : {"ScrollLeftButton", "ScrollRightButton"}) {
        const QToolButton* scroll = scrollButton(_tabBar, name);
        if (scroll && !scroll->isHidden() && scroll->geometry().contains(point)) {
            return;
        }
    }
    Q_EMIT _tabBar->tabBarDoubleClicked(_tabBar->tabAt(point));
    press(position, button);
}

void TabBarAdapter::wheel(QPointF angleDelta)
{
    // QTabBar::wheelEvent() for wheels with angle steps: the next or previous enabled tab.
    if (!_tabBar || !_tabBar->style()->styleHint(QStyle::SH_TabBar_AllowWheelScrolling, nullptr, _tabBar)) {
        return;
    }
    _accumulatedAngleDelta += angleDelta.toPoint();
    const int xSteps = _accumulatedAngleDelta.x() / QWheelEvent::DefaultDeltasPerStep;
    const int ySteps = _accumulatedAngleDelta.y() / QWheelEvent::DefaultDeltasPerStep;
    int offset = 0;
    if (xSteps > 0 || ySteps > 0) {
        offset = -1;
        _accumulatedAngleDelta = QPoint();
    } else if (xSteps < 0 || ySteps < 0) {
        offset = 1;
        _accumulatedAngleDelta = QPoint();
    }
    for (int index = _tabBar->currentIndex() + offset; offset != 0 && index >= 0 && index < _tabBar->count(); index += offset) {
        if (_tabBar->isTabEnabled(index) && _tabBar->isTabVisible(index)) {
            _tabBar->setCurrentIndex(index);
            break;
        }
    }
}

void TabBarAdapter::contextMenu(QPointF position)
{
    // The container opens its menu (Qt::CustomContextMenu) at the tab bar's global position,
    // which is the QML tab bar's: the compat window has the QML window's geometry.
    if (_tabBar && _tabBar->contextMenuPolicy() == Qt::CustomContextMenu) {
        Q_EMIT _tabBar->customContextMenuRequested(position.toPoint());
    }
}

void TabBarAdapter::clickButton(int index)
{
    const QList<QWidget*> all = buttons();
    if (index >= 0 && index < all.size()) {
        if (auto* button = qobject_cast<QAbstractButton*>(all.at(index)); button && button->isEnabled()) {
            button->click();
        }
    }
}

}

#include "moc_TabBarAdapter.cpp"
