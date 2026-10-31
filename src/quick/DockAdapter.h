/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QDockWidget;
class QWidget;

namespace Konsole {
/**
 * Old widget plugins' QDockWidgets in a compat window, for the QML window. Each dock stays
 * in the compat window (its area, size, visibility and the plugin's toggles keep working
 * there and the layout keeps its room); its content widget becomes a native window that
 * the QML window embeds (WindowContainer) where the dock lays it out, under a QML title.
 */
class KONSOLEPRIVATE_EXPORT DockAdapter : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject* window READ window WRITE setWindow NOTIFY windowChanged)
    // In the window: {dock, title, contentWindow (QWindow), titleRect, contentRect, visible}.
    Q_PROPERTY(QVariantList docks READ docks NOTIFY changed)

public:
    explicit DockAdapter(QObject* parent = nullptr);
    ~DockAdapter() override;

    QObject* window() const;
    void setWindow(QObject* window);
    QVariantList docks() const;

    // The QML title's close button: the real dock closes (the plugin's toggle follows).
    Q_INVOKABLE void closeDock(QObject* dock);

Q_SIGNALS:
    void windowChanged();
    void changed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void scheduleRefresh();
    void refresh();
    void adopt(QDockWidget* dock);
    void syncContent(QDockWidget* dock);

    QPointer<QWidget> _window;
    // Dock → its content, taken out as a native window.
    QHash<QDockWidget*, QPointer<QWidget>> _contents;
    QVariantList _docks;
    bool _refreshScheduled = false;
};
}
