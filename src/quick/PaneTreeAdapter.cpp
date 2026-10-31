/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "PaneTreeAdapter.h"

#include "CompatWindows.h"
#include "TerminalView.h"
#include "TerminalViewRegistry.h"
#include "terminalDisplay/TerminalDisplay.h"

#include <QEvent>
#include <QMainWindow>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTabBar>

namespace Konsole {
PaneTreeAdapter::PaneTreeAdapter(QObject* parent)
    : QAbstractListModel(parent)
{
    // Layout changes come in bursts (a split moves, resizes and shows several widgets).
    _refresh.setSingleShot(true);
    _refresh.setInterval(0);
    connect(&_refresh, &QTimer::timeout, this, &PaneTreeAdapter::refresh);
    connect(CompatWindowsNotifier::instance(), &CompatWindowsNotifier::activeChanged, &_refresh, qOverload<>(&QTimer::start));
}

QObject* PaneTreeAdapter::window() const
{
    return _window;
}

void PaneTreeAdapter::setWindow(QObject* window)
{
    auto* widget = qobject_cast<QWidget*>(window);
    if (_window == widget) {
        return;
    }
    _window = widget;
    if (_window) {
        watch(_window);
    }
    refresh();
    Q_EMIT windowChanged();
}

QWidget* PaneTreeAdapter::central() const
{
    auto* mainWindow = qobject_cast<QMainWindow*>(_window.data());
    return mainWindow ? mainWindow->centralWidget() : nullptr;
}

void PaneTreeAdapter::watch(QWidget* widget)
{
    widget->removeEventFilter(this);
    widget->installEventFilter(this);
    for (QWidget* child : widget->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
        watch(child);
    }
}

bool PaneTreeAdapter::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::ChildAdded:
        if (auto* child = qobject_cast<QWidget*>(static_cast<QChildEvent*>(event)->child())) {
            watch(child);
        }
        _refresh.start();
        break;
    case QEvent::ChildRemoved:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::ShowToParent:
    case QEvent::HideToParent:
    case QEvent::ParentChange:
        _refresh.start();
        break;
    default:
        break;
    }
    return QAbstractListModel::eventFilter(watched, event);
}

void PaneTreeAdapter::refresh()
{
    QWidget* centralWidget = central();
    const QRect centralGeometry = centralWidget ? QRect(centralWidget->mapTo(_window, QPoint(0, 0)), centralWidget->size()) : QRect();
    QList<Pane> current;
    QVariantList handles;
    if (centralWidget) {
        for (TerminalDisplay* display : centralWidget->findChildren<TerminalDisplay*>()) {
            TerminalView* view = TerminalViewRegistry::view(display);
            if (view && view->sceneMode()) {
                // TerminalHeaderBar::paintEvent() asks qApp->widgetAt(), which a never-mapped
                // compat window doesn't answer; its childAt() does.
                bool tabBarBase = false;
                if (const QWidget* header = display->headerBar(); header && header->isVisibleTo(_window)) {
                    const QWidget* above = _window->childAt(header->mapTo(_window, QPoint(10, -10)));
                    tabBarBase = above && !qobject_cast<const QTabBar*>(above) && !qobject_cast<const TerminalDisplay*>(above)
                        && !qobject_cast<const QSplitter*>(above) && !qobject_cast<const QSplitterHandle*>(above);
                }
                current.append({view, QRect(display->mapTo(centralWidget, QPoint(0, 0)), display->size()), display->isVisibleTo(_window), tabBarBase});
            }
        }
        for (const QSplitterHandle* handle : centralWidget->findChildren<QSplitterHandle*>()) {
            if (handle->isVisibleTo(_window)) {
                handles.append(QVariantMap{{QStringLiteral("geometry"), QRect(handle->mapTo(centralWidget, QPoint(0, 0)), handle->size())},
                    {QStringLiteral("orientation"), int(handle->orientation())},
                    {QStringLiteral("palette"), CompatWindows::palette(handle)}});
            }
        }
    }

    // Rows for views that went, then rows for new views; the others only change data.
    for (int row = _panes.size() - 1; row >= 0; --row) {
        const bool kept = std::any_of(current.cbegin(), current.cend(), [this, row](const Pane& pane) {
            return pane.view == _panes.at(row).view;
        });
        if (!kept) {
            beginRemoveRows({}, row, row);
            _panes.removeAt(row);
            endRemoveRows();
        }
    }
    for (const Pane& pane : std::as_const(current)) {
        const auto existing = std::find_if(_panes.begin(), _panes.end(), [&pane](const Pane& known) {
            return known.view == pane.view;
        });
        if (existing == _panes.end()) {
            beginInsertRows({}, _panes.size(), _panes.size());
            _panes.append(pane);
            endInsertRows();
        } else if (existing->geometry != pane.geometry || existing->shown != pane.shown || existing->headerTabBarBase != pane.headerTabBarBase) {
            *existing = pane;
            const QModelIndex changed = index(int(existing - _panes.begin()));
            Q_EMIT dataChanged(changed, changed, {GeometryRole, ShownRole, HeaderTabBarBaseRole});
        }
    }
    if (handles != _handles || centralGeometry != _centralGeometry) {
        _handles = handles;
        _centralGeometry = centralGeometry;
        Q_EMIT layoutChanged();
    }
}

QRect PaneTreeAdapter::centralGeometry() const
{
    return _centralGeometry;
}

QVariantList PaneTreeAdapter::handles() const
{
    return _handles;
}

int PaneTreeAdapter::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(_panes.size());
}

QVariant PaneTreeAdapter::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= _panes.size()) {
        return {};
    }
    const Pane& pane = _panes.at(index.row());
    switch (role) {
    case ViewRole:
        return QVariant::fromValue(pane.view.data());
    case GeometryRole:
        return pane.geometry;
    case ShownRole:
        return pane.shown;
    case HeaderTabBarBaseRole:
        return pane.headerTabBarBase;
    default:
        return {};
    }
}

QHash<int, QByteArray> PaneTreeAdapter::roleNames() const
{
    return {{ViewRole, "view"}, {GeometryRole, "geometry"}, {ShownRole, "shown"}, {HeaderTabBarBaseRole, "headerTabBarBase"}};
}
}

#include "moc_PaneTreeAdapter.cpp"
