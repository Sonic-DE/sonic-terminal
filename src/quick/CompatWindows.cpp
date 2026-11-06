/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CompatWindows.h"

#include <QApplication>
#include <QHash>
#include <QPointer>
#include <QWidget>
#include <QWindow>

namespace Konsole {
CompatWindowsNotifier* CompatWindowsNotifier::instance()
{
    static CompatWindowsNotifier notifier;
    return &notifier;
}

namespace CompatWindows {
    namespace {
        // Compat window -> whether its QML window is active.
        QHash<const QObject*, bool>& windows()
        {
            static QHash<const QObject*, bool> hash;
            return hash;
        }

        // Compat window -> its QML window.
        QHash<const QObject*, QPointer<QWindow>>& shownWindows()
        {
            static QHash<const QObject*, QPointer<QWindow>> hash;
            return hash;
        }

        bool s_hostedProcess = false;
        bool s_fullTerminalHost = false;
    }

    void setHostedProcess()
    {
        s_hostedProcess = true;
    }

    bool hostedProcess()
    {
        return s_hostedProcess;
    }

    void setFullTerminalHost()
    {
        s_fullTerminalHost = true;
    }

    bool fullTerminalHost()
    {
        return s_fullTerminalHost;
    }

    void add(const QObject* window)
    {
        windows().insert(window, false);
    }

    void setShownWindow(const QObject* window, QWindow* shown)
    {
        if (contains(window)) {
            shownWindows().insert(window, shown);
        }
    }

    void remove(const QObject* window)
    {
        windows().remove(window);
        shownWindows().remove(window);
    }

    bool contains(const QObject* window)
    {
        return window && windows().contains(window);
    }

    void setActive(const QObject* window, bool active)
    {
        const auto it = windows().find(window);
        if (it != windows().end() && it.value() != active) {
            it.value() = active;
            Q_EMIT CompatWindowsNotifier::instance()->activeChanged(window);
        }
    }

    QPalette::ColorGroup colorGroup(const QWidget* widget)
    {
        // QWidget::palette()'s rule, with the QML window's activation for a compat window's.
        const auto it = windows().constFind(widget->window());
        if (it == windows().constEnd()) {
            return widget->palette().currentColorGroup();
        }
        if (!widget->isEnabled()) {
            return QPalette::Disabled;
        }
        return it.value() ? QPalette::Active : QPalette::Inactive;
    }

    QPalette palette(const QWidget* widget)
    {
        QPalette palette = widget->palette();
        palette.setCurrentColorGroup(colorGroup(widget));
        return palette;
    }

    QWidget* activeWindow()
    {
        if (QWidget* window = QApplication::activeWindow()) {
            return window;
        }
        for (auto it = windows().cbegin(); it != windows().cend(); ++it) {
            if (it.value()) {
                return const_cast<QWidget*>(qobject_cast<const QWidget*>(it.key()));
            }
        }
        return nullptr;
    }

    QWindow* shownWindow(const QWidget* window)
    {
        if (!window) {
            return nullptr;
        }
        if (QWindow* shown = shownWindows().value(window)) {
            return shown;
        }
        return window->windowHandle();
    }
}
}

#include "moc_CompatWindows.cpp"
