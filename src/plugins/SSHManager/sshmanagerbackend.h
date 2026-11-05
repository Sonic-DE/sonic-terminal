/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SSHMANAGERBACKEND_H
#define SSHMANAGERBACKEND_H

#include <QKeySequence>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

class QAbstractItemModel;
class SSHManagerFilterModel;
class SSHManagerModel;

namespace Konsole {
class MainWindow;
class SessionController;
}

// A window's SSH Manager panel (SSHManagerPanel.qml) without its widgets: the filtered model,
// the entries' edits and connecting. Indexes are the filter model's. An entry's fields in
// QML are {name, host, port, sshKey, username, profileName, useSshConfig, folder, imported}.
class SSHManagerBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* model READ model CONSTANT)
    Q_PROPERTY(QStringList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QStringList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(bool manageProfile READ manageProfile WRITE setManageProfile NOTIFY manageProfileChanged)
    Q_PROPERTY(QKeySequence shortcut READ shortcut WRITE setShortcut NOTIFY quickAccessShortcutChanged)

public:
    SSHManagerBackend(SSHManagerModel* model, Konsole::MainWindow* window, QObject* parent = nullptr);
    ~SSHManagerBackend() override;

    QAbstractItemModel* model() const;
    QStringList folders() const;
    QStringList profiles() const;
    bool manageProfile() const;
    void setManageProfile(bool manage);
    QKeySequence shortcut() const;
    void setShortcut(const QKeySequence& shortcut);

    void setCurrentController(Konsole::SessionController* controller);

    Q_INVOKABLE void setFilter(const QString& text);
    Q_INVOKABLE void setInvertFilter(bool invert);
    Q_INVOKABLE bool canEdit(const QModelIndex& index) const;
    // A row's fields; nothing for a folder.
    Q_INVOKABLE QVariantMap entry(const QModelIndex& index) const;
    // What a selected row allows: {canEdit, canDelete, deleteToolTip}.
    Q_INVOKABLE QVariantMap rowActions(const QModelIndex& index) const;
    // checkFields(): the error text, empty when the fields are fine.
    Q_INVOKABLE QString checkFields(const QVariantMap& fields) const;
    Q_INVOKABLE void addEntry(const QVariantMap& fields);
    Q_INVOKABLE void saveEdit(const QModelIndex& index, const QVariantMap& fields);
    Q_INVOKABLE void triggerDelete(const QModelIndex& index);
    Q_INVOKABLE void createMenu(const QModelIndex& index, const QPoint& globalPosition);
    // A double click.
    Q_INVOKABLE void connectTo(const QModelIndex& index);
    // A middle click: in a new tab.
    Q_INVOKABLE void connectInNewTab(const QModelIndex& index);
    // The SSH key button's file dialog; empty if cancelled.
    Q_INVOKABLE QString chooseSshKey();

Q_SIGNALS:
    void foldersChanged();
    void profilesChanged();
    void manageProfileChanged();
    void quickAccessShortcutChanged(const QKeySequence& shortcut);

private:
    SSHManagerModel* m_model;
    SSHManagerFilterModel* m_filterModel;
    Konsole::MainWindow* m_window;
    QPointer<Konsole::SessionController> m_controller;
    QKeySequence m_shortcut;
};

#endif
