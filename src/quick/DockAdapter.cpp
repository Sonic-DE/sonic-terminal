/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DockAdapter.h"

#include <Sonic/Logging/Log>

#include <QDockWidget>
#include <QEvent>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QWindow>

namespace Konsole {
namespace {
    // Holds the content's place in the dock, with the content's size hints.
    class DockPlaceholder : public QWidget {
    public:
        DockPlaceholder(QSize hint, QSize minimum, QWidget* parent)
            : QWidget(parent)
            , _hint(hint)
            , _minimum(minimum)
        {
        }

        QSize sizeHint() const override
        {
            return _hint;
        }

        QSize minimumSizeHint() const override
        {
            return _minimum;
        }

    private:
        QSize _hint;
        QSize _minimum;
    };

    QSet<const QWidget*>& dockContents()
    {
        static QSet<const QWidget*> set;
        return set;
    }
}

bool DockAdapter::isDockContent(const QWidget* widget)
{
    return dockContents().contains(widget);
}

DockAdapter::DockAdapter(QObject* parent)
    : QObject(parent)
{
}

DockAdapter::~DockAdapter() = default;

QObject* DockAdapter::window() const
{
    return _window;
}

void DockAdapter::setWindow(QObject* window)
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
        // Plugins add their docks after the window exists.
        _window->installEventFilter(this);
    }
    refresh();
    Q_EMIT windowChanged();
}

QVariantList DockAdapter::docks() const
{
    return _docks;
}

void DockAdapter::closeDock(QObject* dock)
{
    if (auto* widget = qobject_cast<QDockWidget*>(dock)) {
        widget->close();
    }
}

void DockAdapter::syncContent(QDockWidget* dock)
{
    // Shown only while embedded (a child of the QML window), so it never maps as a top-level
    // of its own, and only while the dock is shown.
    QWidget* content = _contents.value(dock);
    if (!content || !content->windowHandle()) {
        return;
    }
    const bool visible = _window && dock->isVisibleTo(_window) && content->windowHandle()->parent();
    if (content->isVisible() != visible) {
        content->setVisible(visible);
    }
}

void DockAdapter::adopt(QDockWidget* dock)
{
    QWidget* content = dock->widget();
    if (!content) {
        return;
    }
    const QSize hint = content->sizeHint();
    const QSize minimum = content->minimumSizeHint();
    content->setParent(nullptr);
    // It kept the compat window's disabled updates.
    content->setUpdatesEnabled(true);
    dock->setWidget(new DockPlaceholder(hint, minimum, dock));
    content->setAttribute(Qt::WA_NativeWindow);
    content->winId();
    // WindowContainer embeds it by reparenting the native window.
    content->windowHandle()->installEventFilter(this);
    _contents.insert(dock, content);
    dockContents().insert(content);
    connect(content, &QObject::destroyed, content, [content] {
        dockContents().remove(content);
    });
    // The plugin deletes its dock, which no longer owns the content.
    connect(dock, &QObject::destroyed, content, &QObject::deleteLater);
    connect(dock, &QObject::destroyed, this, [this, dock] {
        _contents.remove(dock);
        scheduleRefresh();
    });
    dock->installEventFilter(this);
    dock->widget()->installEventFilter(this);
    sonicDebug() << "plugin dock embedded:" << dock->windowTitle();
}

void DockAdapter::scheduleRefresh()
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

void DockAdapter::refresh()
{
    QVariantList docks;
    if (_window) {
        const QList<QDockWidget*> widgets = _window->findChildren<QDockWidget*>(Qt::FindDirectChildrenOnly);
        for (QDockWidget* dock : widgets) {
            if (!_contents.contains(dock)) {
                adopt(dock);
            }
            QWidget* content = _contents.value(dock);
            QWidget* placeholder = dock->widget();
            if (!content || !placeholder) {
                continue;
            }
            syncContent(dock);
            const bool visible = dock->isVisibleTo(_window);
            const QRect dockRect(dock->mapTo(_window, QPoint(0, 0)), dock->size());
            const QRect contentRect(placeholder->mapTo(_window, QPoint(0, 0)), placeholder->size());
            const QRect titleRect(dockRect.topLeft(), QSize(dockRect.width(), contentRect.top() - dockRect.top()));
            // Without the mnemonic markers KAcceleratorManager adds ("&&" shows "&").
            QString title = dock->windowTitle();
            title.remove(QRegularExpression(QStringLiteral("&(?!&)")));
            docks.append(QVariantMap{{QStringLiteral("dock"), QVariant::fromValue<QObject*>(dock)},
                {QStringLiteral("title"), title},
                {QStringLiteral("contentWindow"), QVariant::fromValue<QObject*>(content->windowHandle())},
                {QStringLiteral("titleRect"), titleRect},
                {QStringLiteral("contentRect"), contentRect},
                {QStringLiteral("visible"), visible}});
        }
    }
    if (docks != _docks) {
        _docks = docks;
        Q_EMIT changed();
    }
}

bool DockAdapter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::ParentWindowChange) {
        for (auto it = _contents.cbegin(); it != _contents.cend(); ++it) {
            if (it.value() && it.value()->windowHandle() == watched) {
                syncContent(it.key());
            }
        }
        return QObject::eventFilter(watched, event);
    }
    switch (event->type()) {
    case QEvent::ChildAdded:
    case QEvent::ChildRemoved:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::ShowToParent:
    case QEvent::HideToParent:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::LayoutRequest:
    case QEvent::WindowTitleChange:
        scheduleRefresh();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}
}

#include "moc_DockAdapter.cpp"
