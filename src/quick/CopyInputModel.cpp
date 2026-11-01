/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CopyInputModel.h"

#include "CheckableSessionModel.h"
#include "session/Session.h"
#include "session/SessionManager.h"

namespace Konsole {
CopyInputModel::CopyInputModel(QObject* parent)
    : QSortFilterProxyModel(parent)
    , _sessions(new CheckableSessionModel(this))
{
    // Column 0 is the session number, column 1 the title with the check box.
    _sessions->setCheckColumn(1);
    _sessions->setSessions(SessionManager::instance()->sessions());
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterKeyColumn(-1);
    setSourceModel(_sessions);
}

void CopyInputModel::setMasterSession(Session* session)
{
    if (!_masterSession.isNull()) {
        _sessions->setCheckable(_masterSession, true);
    }
    _sessions->setCheckable(session, false);
    QSet<Session*> checked = _sessions->checkedSessions();
    checked.insert(session);
    _sessions->setCheckedSessions(checked);
    _masterSession = session;
}

void CopyInputModel::setChosenSessions(const QSet<Session*>& sessions)
{
    QSet<Session*> checked = sessions;
    if (!_masterSession.isNull()) {
        checked.insert(_masterSession);
    }
    _sessions->setCheckedSessions(checked);
}

QSet<Session*> CopyInputModel::chosenSessions() const
{
    return _sessions->checkedSessions();
}

QString CopyInputModel::filterText() const
{
    return _filterText;
}

void CopyInputModel::setFilterText(const QString& text)
{
    if (text == _filterText) {
        return;
    }
    _filterText = text;
    setFilterFixedString(text);
    Q_EMIT filterTextChanged();
}

void CopyInputModel::setChecked(int row, bool checked)
{
    const QModelIndex proxy = index(row, 0);
    if (!proxy.isValid()) {
        return;
    }
    if (setData(proxy.siblingAtColumn(_sessions->checkColumn()), static_cast<int>(checked ? Qt::Checked : Qt::Unchecked), Qt::CheckStateRole)) {
        // Views of QML read column 0.
        Q_EMIT dataChanged(proxy, proxy, {CheckedRole});
    }
}

void CopyInputModel::setSelectionChecked(const QList<int>& selectedRows, bool checked)
{
    if (selectedRows.size() > 1) {
        for (int row : selectedRows) {
            setChecked(row, checked);
        }
        return;
    }
    for (int row = 0; row < rowCount(); ++row) {
        setChecked(row, checked);
    }
}

QHash<int, QByteArray> CopyInputModel::roleNames() const
{
    return {{TitleRole, "title"}, {IconNameRole, "iconName"}, {CheckedRole, "checked"}, {CheckableRole, "checkable"}};
}

QVariant CopyInputModel::data(const QModelIndex& index, int role) const
{
    if (role < TitleRole) {
        return QSortFilterProxyModel::data(index, role);
    }
    const QModelIndex title = mapToSource(index).siblingAtColumn(_sessions->checkColumn());
    if (!title.isValid()) {
        return {};
    }
    switch (role) {
    case TitleRole:
        return title.data(Qt::DisplayRole);
    case IconNameRole:
        return static_cast<Session*>(title.internalPointer())->iconName();
    case CheckedRole:
        return title.data(Qt::CheckStateRole).toInt() == Qt::Checked;
    case CheckableRole:
        return bool(title.flags() & Qt::ItemIsEnabled);
    }
    return {};
}
}

#include "moc_CopyInputModel.cpp"
