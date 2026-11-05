/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef QUICKCOMMANDSBACKEND_H
#define QUICKCOMMANDSBACKEND_H

#include <QKeySequence>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

class FilterModel;
class QAbstractItemModel;
class QuickCommandData;
class QuickCommandsModel;

namespace Konsole {
class MainWindow;
class SessionController;
}

// A window's Quick Commands panel (QuickCommandsPanel.qml) without its widgets: the filtered
// model, the commands' edits and running them. Indexes are the filter model's.
class QuickCommandsBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* model READ model CONSTANT)
    Q_PROPERTY(QStringList groups READ groups NOTIFY groupsChanged)
    Q_PROPERTY(QString warnings READ warnings WRITE setWarnings NOTIFY warningsChanged)
    Q_PROPERTY(QString warningsTitle READ warningsTitle NOTIFY warningsChanged)
    Q_PROPERTY(QKeySequence shortcut READ shortcut WRITE setShortcut NOTIFY quickAccessShortcutChanged)

public:
    QuickCommandsBackend(QuickCommandsModel* model, Konsole::MainWindow* window, QObject* parent = nullptr);
    ~QuickCommandsBackend() override;

    QAbstractItemModel* model() const;
    QStringList groups() const;
    QString warnings() const;
    void setWarnings(const QString& warnings);
    QString warningsTitle() const;
    QKeySequence shortcut() const;
    void setShortcut(const QKeySequence& shortcut);

    void setCurrentController(Konsole::SessionController* controller);

    Q_INVOKABLE void setFilter(const QString& text);
    Q_INVOKABLE void setInvertFilter(bool invert);
    // A clicked row: {name, tooltip, command, group}; empty values for a group, nothing to
    // change for a row without data.
    Q_INVOKABLE QVariantMap selected(const QModelIndex& index) const;
    // false refuses (a message box said why) and the form stays.
    Q_INVOKABLE bool saveCommand(const QString& name, const QString& tooltip, const QString& command, const QString& group);
    Q_INVOKABLE bool updateCommand(const QModelIndex& index, const QString& name, const QString& tooltip, const QString& command, const QString& group);
    // Double click: runs the row's command, unless there are warnings.
    Q_INVOKABLE void invokeCommand(const QModelIndex& index);
    // Run: the text area's command, asking first when there are warnings.
    Q_INVOKABLE void runCommand(const QString& command);
    Q_INVOKABLE void runShellCheck(const QString& command);
    Q_INVOKABLE void triggerDelete(const QModelIndex& index);
    Q_INVOKABLE void createMenu(const QModelIndex& index, const QPoint& globalPosition);

Q_SIGNALS:
    void groupsChanged();
    void warningsChanged();
    void quickAccessShortcutChanged(const QKeySequence& shortcut);
    void editRequested();
    void renameRequested(const QModelIndex& index);

private:
    bool valid(const QString& name, const QString& command);
    void send(const QString& command);

    QuickCommandsModel* m_model;
    FilterModel* m_filterModel;
    Konsole::MainWindow* m_window;
    QPointer<Konsole::SessionController> m_controller;
    QString m_warnings;
    QString m_warningsTitle;
    QKeySequence m_shortcut;
    bool m_hasShellCheck = false;
};

#endif
