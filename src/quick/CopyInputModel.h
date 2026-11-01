/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QPointer>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QtQml/qqmlregistration.h>

namespace Konsole {
class CheckableSessionModel;
class Session;

/**
 * The Copy Input dialog's session list (CopyInputDialog's models and rules): every
 * session with a check box, the master session checked and disabled, filtered by text.
 */
class KONSOLEPRIVATE_EXPORT CopyInputModel : public QSortFilterProxyModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("created by SessionController")
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        IconNameRole,
        CheckedRole,
        CheckableRole,
    };

    explicit CopyInputModel(QObject* parent = nullptr);

    void setMasterSession(Session* session);
    void setChosenSessions(const QSet<Session*>& sessions);
    QSet<Session*> chosenSessions() const;

    QString filterText() const;
    void setFilterText(const QString& text);

    Q_INVOKABLE void setChecked(int row, bool checked);
    // Select All / Deselect All: the selected rows when more than one is, else every row.
    Q_INVOKABLE void setSelectionChecked(const QList<int>& selectedRows, bool checked);

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex& index, int role) const override;

Q_SIGNALS:
    void filterTextChanged();

private:
    CheckableSessionModel* _sessions;
    QPointer<Session> _masterSession;
    QString _filterText;
};
}
