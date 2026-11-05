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

public:
    explicit TerminalHost(QQuickItem* parent = nullptr);
    ~TerminalHost() override;

    QObject* compatWindow() const;

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

Q_SIGNALS:
    void compatWindowChanged();
    void currentDirectoryChanged(const QString& dir);
    // The session ended; nothing of the terminal is left.
    void finished();

protected:
    void componentComplete() override;

private:
    Session* activeSession() const;
    void activeViewChanged(SessionController* controller);

    QPointer<MainWindow> _window;
    QPointer<SessionController> _controller;
};
}
