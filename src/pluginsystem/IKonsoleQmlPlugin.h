/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class QAction;

namespace Konsole {
class MainWindow;

// A plugin's side panel in a window, drawn by QML.
struct QmlPanel {
    QString title;
    // Saved with the window's layout, as a dock's object name.
    QString objectName;
    // Its root item takes these as initial properties; the application's engine loads it.
    QUrl source;
    QVariantMap properties;
    // Checkable: shows and hides the panel, and follows it. May be null.
    QAction* toggleAction = nullptr;
};

/**
 * Implemented next to IKonsolePlugin (Q_INTERFACES) by a plugin with QML panels: nothing of
 * it is a widget. Its createWidgetsForMainWindow() stays empty; activeViewChanged() and
 * menuBarActions() work as for any plugin.
 */
class IKonsoleQmlPlugin {
public:
    virtual ~IKonsoleQmlPlugin() = default;

    virtual QList<QmlPanel> panels(MainWindow* window) = 0;
};
}

Q_DECLARE_INTERFACE(Konsole::IKonsoleQmlPlugin, "org.sonicde.terminal.IKonsoleQmlPlugin/1.0")
