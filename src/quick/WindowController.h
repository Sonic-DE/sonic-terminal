/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleapp_export.h"

#include <QObject>
#include <QPointer>

class QQuickWindow;
class QShortcut;
class QWindow;

namespace Konsole {
class MainWindow;
class TerminalDisplay;

/**
 * Pairs a MainWindow with the QML window that shows it. The MainWindow becomes the
 * compatibility window: shown off-screen (never mapped, never painted) with its KXmlGui
 * setup, layouts and the frozen API working; the QML window draws what its widgets lay
 * out. Size, position, visibility, title and icon follow each other; closing the QML window
 * closes the compat window (its queryClose() asks as before); a facade that gets focus
 * gives its view active focus (the facade focus bridge).
 */
class KONSOLEAPP_EXPORT WindowController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* compatWindow READ compatWindow CONSTANT)
    // As the widget window: translucent (alpha channel, no window background) whenever
    // MainWindow::updateUseTransparency() made the compat window so.
    Q_PROPERTY(bool translucent READ translucent CONSTANT)

public:
    // Before the window's first show().
    explicit WindowController(MainWindow* window);
    ~WindowController() override;

    QObject* compatWindow() const;
    QQuickWindow* qmlWindow() const;
    bool translucent() const;
    static WindowController* controllerOf(const QObject* compatWindow);

    // MainWindow's bodies that act on the visible window call these instead.
    void setBlur(bool blur);
    void setFrameless(bool frameless);
    void activateMenuBar();

    Q_INVOKABLE void requestClose();

Q_SIGNALS:
    void menuBarActivated();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void createQmlWindow();
    void followQmlGeometry();
    void followQmlWindowState();
    void setCompatGeometry(const QRect& geometry);
    void followCompatGeometry();
    void applyBlur();
    void connectFocusBridge();
    void bridgeFocus(QObject* object);
    void scheduleShortcuts();
    void rebuildShortcuts();

    QPointer<MainWindow> _window;
    QPointer<QQuickWindow> _qmlWindow;
    QPointer<QWindow> _bridgedWindow;
    // A facade that got focus before its view was in the scene.
    QPointer<TerminalDisplay> _pendingFocus;
    // The compat window's action shortcuts, on the QML window (its actions' own don't fire
    // while a QML window has focus).
    QList<QShortcut*> _shortcuts;
    bool _shortcutsScheduled = false;
    bool _syncing = false;
    bool _translucent = false;
    bool _blur = false;
};
}
