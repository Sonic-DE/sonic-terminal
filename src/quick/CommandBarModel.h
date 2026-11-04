/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <KCommandBar>

#include <QAbstractListModel>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

class KXMLGUIFactory;
class QAction;

namespace Konsole {
/**
 * KCommandBar's rows (KCommandBarModel with its filter proxy, without widgets): the action
 * groups' enabled actions, "Clear History", the last used ones first, KFuzzyMatcher's
 * filter and order, the matched ranges to highlight, and what Return or a click does.
 */
class KONSOLEPRIVATE_EXPORT CommandBarModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by WindowController::openCommandBar()")
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(bool hasIcons READ hasIcons NOTIFY rowsChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText NOTIFY rowsChanged)

public:
    enum Roles {
        IconNameRole = Qt::UserRole + 1,
        // The group's length in the display text ("Group: Command"), drawn in a softer colour.
        GroupLengthRole,
        // [start, length, …] of the filter's matches in the display text.
        MatchesRole,
        ShortcutRole,
        ToolTipRole,
    };

    explicit CommandBarModel(QObject* parent = nullptr);
    ~CommandBarModel() override;

    // As KXmlGuiWindow's Find Action builds them from a window's clients and their children.
    static QList<KCommandBar::ActionGroup> windowGroups(KXMLGUIFactory* factory);
    // KCommandBar::setActions()
    void setActions(const QList<KCommandBar::ActionGroup>& groups);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const;
    void setFilterText(const QString& text);
    bool hasIcons() const;
    QString placeholderText() const;

    // Return or a click on a row: a menu's actions replace the rows (true: the bar stays);
    // another action is remembered and triggered once the bar has closed (false).
    Q_INVOKABLE bool activate(int row);
    // KCommandBarModel::splitShortcut(): keys at even indexes, separators at odd ones.
    Q_INVOKABLE static QStringList shortcutSegments(const QString& shortcut);

Q_SIGNALS:
    void filterTextChanged();
    void rowsChanged();

private:
    struct Item {
        QString groupName;
        QPointer<QAction> action;
        int score = -1;
        QString displayName() const;
    };

    void refresh(const QList<KCommandBar::ActionGroup>& groups);
    void filter();

    QList<Item> _all;
    // Indexes into _all, shown and sorted.
    QList<int> _shown;
    QString _filterText;
    QStringList _lastTriggered;
    QAction* _clearHistoryAction = nullptr;
};
}
