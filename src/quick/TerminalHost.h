/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleapp_export.h"

#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

class QAction;

namespace Sonic::Ipc {
class Server;
}

namespace Konsole {
class MainWindow;
class Session;
class SessionController;

/**
 * A terminal for another program's QML (Ecco, the panel widget), with what those used from
 * the KPart. Off-screen behind it is a compat window with one session, as behind
 * sonic-terminal's own window; it needs a QApplication in the host and shows nothing
 * without one.
 */
class KONSOLEAPP_EXPORT TerminalHost : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject* compatWindow READ compatWindow NOTIFY compatWindowChanged)
    // The window's controller, for the command bar.
    Q_PROPERTY(QObject* controller READ controller NOTIFY compatWindowChanged)
    // Tabs, splits, toolbars and the terminal's menus, not one session as the KPart had. Set
    // when the item is created.
    Q_PROPERTY(bool fullTerminal MEMBER _fullTerminal)
    // The name of the profile to start with, or the default one. Set when the item is created.
    Q_PROPERTY(QString profile MEMBER _profile)
    Q_PROPERTY(bool toolBarsAtBottom READ toolBarsAtBottom WRITE setToolBarsAtBottom NOTIFY toolBarsAtBottomChanged)
    // A started sonic-terminal opens a tab here instead of a window (PanelOpen). The first host
    // to ask serves.
    Q_PROPERTY(bool servesNewTerminals READ servesNewTerminals WRITE setServesNewTerminals NOTIFY servesNewTerminalsChanged)
    // The color scheme's blur setting: the window holding this item is the host's to blur.
    Q_PROPERTY(bool blur READ blur NOTIFY blurChanged)

public:
    explicit TerminalHost(QQuickItem* parent = nullptr);
    ~TerminalHost() override;

    QObject* compatWindow() const;
    QObject* controller() const;
    bool toolBarsAtBottom() const;
    void setToolBarsAtBottom(bool atBottom);
    bool servesNewTerminals() const;
    void setServesNewTerminals(bool serves);
    bool blur() const;

    // A MainWindow being built now belongs to a host: it uses Konsole's XML and leaves the
    // host's window settings and quitting alone.
    static bool creatingWindow();

    // TerminalInterface
    Q_INVOKABLE void showShellInDir(const QString& dir);
    Q_INVOKABLE void sendInput(const QString& text);
    Q_INVOKABLE QString currentWorkingDirectory() const;
    Q_INVOKABLE int foregroundProcessId() const;
    Q_INVOKABLE QString foregroundProcessName() const;

    Q_INVOKABLE void setContextMenuAdditionalActions(const QList<QAction*>& actions);
    // The session's KActionCollection, for the host's Configure Shortcuts dialog.
    Q_INVOKABLE QObject* sessionActionCollection() const;
    // After that dialog: keeps its changes (ShortcutStore).
    Q_INVOKABLE void saveShortcuts();
    // A menu or dialog of the terminal is open: the window holding this item lost the focus to it.
    Q_INVOKABLE bool ownWindowOpen();
    // The active session's Edit Profile dialog.
    Q_INVOKABLE void configureTerminal();

Q_SIGNALS:
    void compatWindowChanged();
    void toolBarsAtBottomChanged();
    void servesNewTerminalsChanged();
    void blurChanged();
    // The last of them closed. keepInput: no other program has taken the focus since.
    void ownWindowsClosed(bool keepInput);
    // A started sonic-terminal opened a tab here: the host shows it and gives it the keyboard.
    void newTerminalOpened();
    // Any button, anywhere in it. A panel's window takes no keyboard input until it asks for it,
    // and the terminal's focus doesn't change on the press: it had the scene's from the start.
    void pressedInside();
    void currentDirectoryChanged(const QString& dir);
    // The session ended; nothing of the terminal is left.
    void finished();

protected:
    void componentComplete() override;
    bool childMouseEventFilter(QQuickItem* item, QEvent* event) override;

private:
    Session* activeSession() const;
    void activeViewChanged(SessionController* controller);
    void setUpFullTerminal();
    void placeToolBars();
    void updateServer();
    void checkOwnWindows();
    void openNewTerminal(const QString& directory);

    QPointer<MainWindow> _window;
    QPointer<SessionController> _controller;
    QPointer<Sonic::Ipc::Server> _server;
    QString _profile;
    bool _fullTerminal = false;
    bool _toolBarsAtBottom = false;
    bool _servesNewTerminals = false;
    bool _ownWindowOpen = false;
    bool _ownDialogOpen = false;
};
}
