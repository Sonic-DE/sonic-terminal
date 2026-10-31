/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "SearchTabsAdapter.h"

#include "CompatWindows.h"
#include "KonsoleSettings.h"
#include "ViewManager.h"
#include "searchtabs/SearchTabsModel.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "widgets/ViewContainer.h"

#include <Sonic/Logging/Log>

#include <KFuzzyMatcher>
#include <KLocalizedString>

#include <QApplication>
#include <QEvent>
#include <QMainWindow>
#include <QStyle>
#include <QTabBar>

namespace Konsole {
SearchTabsAdapter::SearchTabsAdapter(QObject* parent)
    : QObject(parent)
    , _model(new SearchTabsModel(this))
{
    connect(CompatWindowsNotifier::instance(), &CompatWindowsNotifier::searchTabsRequested, this, [this](const QObject* window) {
        if (window == _window) {
            openSearch();
        }
    });
}

SearchTabsAdapter::~SearchTabsAdapter() = default;

QObject* SearchTabsAdapter::window() const
{
    return _window;
}

void SearchTabsAdapter::setWindow(QObject* window)
{
    auto* widget = qobject_cast<QWidget*>(window);
    if (widget == _window) {
        return;
    }
    if (_window) {
        _window->removeEventFilter(this);
    }
    _window = widget;
    if (_window) {
        // SearchTabs follows its window's size.
        _window->installEventFilter(this);
    }
    Q_EMIT windowChanged();
}

bool SearchTabsAdapter::isOpen() const
{
    return _open;
}

QString SearchTabsAdapter::text() const
{
    return _text;
}

void SearchTabsAdapter::setText(const QString& text)
{
    if (text != _text) {
        _text = text;
        filter();
    }
}

QVariantList SearchTabsAdapter::entries() const
{
    return _entries;
}

int SearchTabsAdapter::currentIndex() const
{
    return _currentIndex;
}

void SearchTabsAdapter::setCurrentIndex(int index)
{
    index = _entries.isEmpty() ? -1 : qBound(0, index, int(_entries.size()) - 1);
    if (index != _currentIndex) {
        _currentIndex = index;
        Q_EMIT currentIndexChanged();
    }
}

QRect SearchTabsAdapter::geometry() const
{
    return _geometry;
}

QString SearchTabsAdapter::placeholderText() const
{
    return i18nc("@label:textbox", "Search...");
}

QString SearchTabsAdapter::toolTip() const
{
    return i18nc("@info:tooltip", "Enter a tab name to search for here");
}

ViewManager* SearchTabsAdapter::viewManager() const
{
    auto* mainWindow = qobject_cast<QMainWindow*>(_window);
    auto* container = mainWindow ? qobject_cast<TabbedViewContainer*>(mainWindow->centralWidget()) : nullptr;
    return container ? container->connectedViewManager() : nullptr;
}

void SearchTabsAdapter::openSearch()
{
    ViewManager* manager = viewManager();
    if (!manager) {
        return;
    }
    _model->refresh(manager);
    _text.clear();
    updateGeometry();
    filter();
    if (!_open) {
        _open = true;
        Q_EMIT openChanged();
    }
    sonicDebug() << "tab search opened with" << _entries.size() << "tabs";
}

void SearchTabsAdapter::filter()
{
    // SearchTabsFilterProxyModel: tab order, fuzzy matched on the title.
    _entries.clear();
    for (int row = 0; row < _model->rowCount(); ++row) {
        const QString& name = _model->idxToName(row);
        if (_text.isEmpty() || KFuzzyMatcher::match(_text, name).matched) {
            const QModelIndex index = _model->index(row, 0);
            _entries.append(QVariantMap{{QStringLiteral("name"), name}, {QStringLiteral("view"), index.data(SearchTabsModel::View)}});
        }
    }
    Q_EMIT entriesChanged();
    // SearchTabs::reselectFirst(): the previous tab when nothing is typed yet.
    const ViewManager* manager = viewManager();
    const bool previous = manager && manager->viewProperties().size() > 1 && _entries.size() > 1 && _text.isEmpty();
    _currentIndex = -1;
    setCurrentIndex(previous ? 1 : 0);
}

void SearchTabsAdapter::activate()
{
    ViewManager* manager = viewManager();
    if (manager && _currentIndex >= 0 && _currentIndex < _entries.size()) {
        // The view identifier is off by one (SearchTabs::slotReturnPressed()).
        manager->setCurrentView(_entries.at(_currentIndex).toMap().value(QStringLiteral("view")).toInt() - 1);
    }
    close();
}

void SearchTabsAdapter::close()
{
    if (!_open) {
        return;
    }
    _open = false;
    Q_EMIT openChanged();
    // Back to the terminal (the facade focus bridge gives its view active focus).
    ViewManager* manager = viewManager();
    if (SessionController* controller = manager ? manager->activeViewController() : nullptr; controller && controller->view()) {
        controller->view()->setFocus(Qt::OtherFocusReason);
    }
}

void SearchTabsAdapter::updateGeometry()
{
    // SearchTabs::updateViewGeometry(), in the compat window, which has the QML window's
    // geometry.
    ViewManager* manager = viewManager();
    if (!_window || !manager || !manager->activeContainer()) {
        return;
    }
    const QRect boundingRect = _window->contentsRect();
    static constexpr int minWidth = 125;
    static constexpr int minHeight = 250;
    const int maxWidth = boundingRect.width();
    const int maxHeight = boundingRect.height();
    const QSize size{qMin(maxWidth, qMax(int(maxWidth / 4.8), minWidth)), qMin(maxHeight, qMax(maxHeight / 4, minHeight))};

    const int mainWindowHeight = _window->geometry().height();
    const int containerHeight = manager->activeContainer()->geometry().height();
    QTabBar* tabBar = manager->activeContainer()->tabBar();
    const bool tabBarVisible = tabBar->isVisible();
    const int tabBarHeight = tabBarVisible ? tabBar->geometry().height() : 0;
    int y;
    if (QTabWidget::TabPosition(KonsoleSettings::tabBarPosition()) == QTabWidget::South && tabBarVisible) {
        y = mainWindowHeight - tabBarHeight - size.height() - 6;
    } else {
        y = mainWindowHeight - containerHeight + tabBarHeight + 6;
    }
    const int scrollBarWidth = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    const int x = _window->geometry().width() - size.width() - scrollBarWidth - 6;
    const QRect geometry(QPoint(x, y), size);
    if (geometry != _geometry) {
        _geometry = geometry;
        Q_EMIT geometryChanged();
    }
}

bool SearchTabsAdapter::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _window && event->type() == QEvent::Resize && _open) {
        updateGeometry();
    }
    return QObject::eventFilter(watched, event);
}
}

#include "moc_SearchTabsAdapter.cpp"
