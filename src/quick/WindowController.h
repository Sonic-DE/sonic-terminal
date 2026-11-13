/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleapp_export.h"

#include <KCommandBar>

#include <QObject>
#include <QPointer>
#include <QShortcut>

class QQuickItem;
class QQuickWindow;
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
    // No window of its own: another program's item shows the compat window, which follows its
    // place on screen; shortcuts apply while it has the focus. With fullTerminal, the window's
    // shortcuts too, not only the session's.
    WindowController(MainWindow* window, QQuickItem* item, bool fullTerminal);
    ~WindowController() override;

    QObject* compatWindow() const;
    QQuickWindow* qmlWindow() const;
    // Where the terminal is on screen: the QML window, or the window holding the item.
    QWindow* shownWindow() const;
    bool translucent() const;
    static WindowController* controllerOf(const QObject* compatWindow);

    // MainWindow's bodies that act on the visible window call these instead.
    void setBlur(bool blur);
    bool blur() const;
    void setFrameless(bool frameless);
    void activateMenuBar();

    Q_INVOKABLE void requestClose();
    // The command bar over the QML window (KCommandBar there), with these actions: Find
    // Action's from the window's clients, or a plugin's.
    void openCommandBar(const QList<KCommandBar::ActionGroup>& groups);
    // The QML bar closed: its model goes.
    Q_INVOKABLE void commandBarClosed();

Q_SIGNALS:
    void menuBarActivated();
    void commandBarRequested(QObject* model);
    void blurChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setUpCompatWindow();
    void createQmlWindow();
    void watchActions();
    void rewireFindAction();
    void followItem();
    void followQmlGeometry();
    void followQmlWindowState();
    void setCompatGeometry(const QRect& geometry);
    void followCompatGeometry();
    void applyBlur();
    void exportMenuBar();
    void connectFocusBridge();
    void bridgeFocus(QObject* object);
    void scheduleShortcuts();
    void rebuildShortcuts();

    QPointer<MainWindow> _window;
    QPointer<QQuickWindow> _qmlWindow;
    QPointer<QQuickItem> _item;
    QPointer<QWindow> _itemWindow;
    QPointer<QWindow> _bridgedWindow;
    // A facade that got focus before its view was in the scene.
    QPointer<TerminalDisplay> _pendingFocus;
    // The compat window's action shortcuts, on the QML window (its actions' own don't fire
    // while a QML window has focus). A host's window can go first.
    QList<QPointer<QShortcut>> _shortcuts;
    QPointer<QObject> _commandBar;
    bool _fullTerminal = false;
    bool _shortcutsScheduled = false;
    bool _syncing = false;
    bool _translucent = false;
    bool _blur = false;
};
}
