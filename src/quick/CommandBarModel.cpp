/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "CommandBarModel.h"

#include <Sonic/Logging/Log>

#include <KActionCollection>
#include <KConfigGroup>
#include <KFuzzyMatcher>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KToolBarPopupAction>
#include <KXMLGUIClient>
#include <KXMLGUIFactory>

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QTimer>

#include <functional>
#include <unordered_set>

using namespace Konsole;

namespace {
const QString LastUsedKey = QStringLiteral("CommandBarLastUsedActions");

void collect(KXMLGUIClient* client, std::vector<KActionCollection*>& collections)
{
    if (!client) {
        return;
    }
    if (KActionCollection* collection = client->actionCollection(); collection && !collection->isEmpty()) {
        collections.push_back(collection);
    }
    const auto children = client->childClients();
    for (KXMLGUIClient* child : children) {
        collect(child, collections);
    }
}

// KCommandBarModel's fillRows()
void fillRows(QList<QPair<QString, QAction*>>& rows, const QString& title, const QList<QAction*>& actions, std::unordered_set<QAction*>& unique)
{
    for (QAction* action : actions) {
        if (!action->isEnabled()) {
            continue;
        }
        if (QMenu* menu = action->menu()) {
            QList<QAction*> menuActions = menu->actions();
            if (menuActions.isEmpty()) {
                Q_EMIT menu->aboutToShow();
                menuActions = menu->actions();
            }
            fillRows(rows, menu->title(), menuActions, unique);
            continue;
        }
        if (action->text().isEmpty() && !action->isSeparator()) {
            sonicWarning() << "Action" << action << "in group" << title << "has no text";
            continue;
        }
        if (unique.insert(action).second) {
            rows.append({title, action});
        }
    }
}
}

QString CommandBarModel::Item::displayName() const
{
    const QString group = KLocalizedString::removeAcceleratorMarker(groupName);
    const QString command = KLocalizedString::removeAcceleratorMarker(action ? action->text() : QString());
    return group.isEmpty() ? command : group + QStringLiteral(": ") + command;
}

CommandBarModel::CommandBarModel(QObject* parent)
    : QAbstractListModel(parent)
{
    _clearHistoryAction = new QAction(QCoreApplication::translate("KCommandBarModel", "Clear History"), this);
    _clearHistoryAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear-history")));
    connect(_clearHistoryAction, &QAction::triggered, this, [this] {
        _lastTriggered.clear();
    });

    // KCommandBar's constructor moves the list to the state config.
    KConfigGroup old(KSharedConfig::openConfig(), QStringLiteral("General"));
    KConfigGroup state(KSharedConfig::openStateConfig(), QStringLiteral("General"));
    if (old.hasKey(LastUsedKey)) {
        state.writeEntry(LastUsedKey, old.readEntry(LastUsedKey, QStringList()));
        old.deleteEntry(LastUsedKey);
    }
}

CommandBarModel::~CommandBarModel()
{
    KConfigGroup state(KSharedConfig::openStateConfig(), QStringLiteral("General"));
    state.writeEntry(LastUsedKey, _lastTriggered);
}

QList<KCommandBar::ActionGroup> CommandBarModel::windowGroups(KXMLGUIFactory* factory)
{
    // KXmlGuiWindow's getActionCollections() and actionCollectionToActionGroup().
    std::vector<KActionCollection*> collections;
    const auto clients = factory ? factory->clients() : QList<KXMLGUIClient*>();
    for (KXMLGUIClient* client : clients) {
        collect(client, collections);
    }
    // As there: one empty group per collection ahead of the real ones.
    QList<KCommandBar::ActionGroup> groups(collections.size());
    std::function<void(QList<QAction*>, KCommandBar::ActionGroup&)> addSubGroupActions = [&](QList<QAction*> actions, KCommandBar::ActionGroup& group) {
        // Menus first.
        std::partition(actions.begin(), actions.end(), [](const QAction* action) {
            return action->menu();
        });
        for (QAction* action : std::as_const(actions)) {
            if (!action || action->isSeparator()) {
                continue;
            }
            QMenu* menu = action->menu();
            if (!menu) {
                if (auto* popupAction = qobject_cast<KToolBarPopupAction*>(action)) {
                    menu = popupAction->popupMenu();
                }
            }
            if (menu) {
                if (menu->actions().isEmpty()) {
                    Q_EMIT menu->aboutToShow();
                }
                const QString actionName = KLocalizedString::removeAcceleratorMarker(action->text());
                KCommandBar::ActionGroup menuGroup;
                menuGroup.name = group.name.isEmpty() ? actionName : group.name + u" > " + actionName;
                addSubGroupActions(menu->actions(), menuGroup);
                if (!menuGroup.actions.isEmpty()) {
                    groups.append(menuGroup);
                    continue;
                }
            } else if (!action->text().isEmpty()) {
                group.actions.append(action);
            }
        }
    };
    for (KActionCollection* collection : collections) {
        KCommandBar::ActionGroup group;
        group.name = collection->componentDisplayName() != QGuiApplication::applicationDisplayName() ? collection->componentDisplayName() : QString();
        addSubGroupActions(collection->actions(), group);
        groups.append(group);
    }
    return groups;
}

void CommandBarModel::setActions(const QList<KCommandBar::ActionGroup>& groups)
{
    // KCommandBarPrivate::setLastUsedActions(), then refresh().
    _lastTriggered = KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("General")).readEntry(LastUsedKey, QStringList());
    while (_lastTriggered.size() > 6) {
        _lastTriggered.removeLast();
    }
    refresh(groups);
}

void CommandBarModel::refresh(const QList<KCommandBar::ActionGroup>& groups)
{
    QList<QPair<QString, QAction*>> rows;
    std::unordered_set<QAction*> unique;
    for (const KCommandBar::ActionGroup& group : groups) {
        fillRows(rows, group.name, group.actions, unique);
    }
    rows.append({QCoreApplication::translate("KCommandBarModel", "Command Bar"), _clearHistoryAction});

    _all.clear();
    for (const auto& [group, action] : std::as_const(rows)) {
        _all.append({group, action, -1});
    }
    // The most recently used scores highest.
    int score = 0;
    for (auto it = _lastTriggered.crbegin(); it != _lastTriggered.crend(); ++it) {
        auto found = std::find_if(_all.begin(), _all.end(), [&](const Item& item) {
            return item.action && item.action->text() == *it;
        });
        if (found != _all.end()) {
            found->score = score++;
        }
    }
    filter();
}

void CommandBarModel::filter()
{
    // CommandBarFilterModel: KFuzzyMatcher's matches with their score, the best first, then
    // by text (its lessThan() under the tree's descending sort).
    beginResetModel();
    _shown.clear();
    for (int i = 0; i < _all.size(); ++i) {
        Item& item = _all[i];
        if (!item.action) {
            continue;
        }
        if (!_filterText.isEmpty()) {
            const KFuzzyMatcher::Result result = KFuzzyMatcher::match(_filterText, item.displayName());
            item.score = result.score;
            if (!result.matched) {
                continue;
            }
        }
        _shown.append(i);
    }
    std::stable_sort(_shown.begin(), _shown.end(), [this](int left, int right) {
        if (_all.at(left).score != _all.at(right).score) {
            return _all.at(left).score > _all.at(right).score;
        }
        return _all.at(left).displayName().localeAwareCompare(_all.at(right).displayName()) < 0;
    });
    endResetModel();
    Q_EMIT rowsChanged();
}

int CommandBarModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : _shown.size();
}

QVariant CommandBarModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= _shown.size()) {
        return {};
    }
    const Item& item = _all.at(_shown.at(index.row()));
    if (!item.action) {
        return {};
    }
    const QString display = item.displayName();
    switch (role) {
    case Qt::DisplayRole:
        return display;
    case IconNameRole:
        return item.action->icon().name();
    case GroupLengthRole: {
        const int colon = display.indexOf(QLatin1Char(':'));
        return colon > 0 ? colon : 0;
    }
    case MatchesRole: {
        // CommandBarStyleDelegate: matches in the command, after "Group: ".
        const int colon = display.indexOf(QLatin1Char(':'));
        const int start = colon > 0 ? colon + 2 : 0;
        QVariantList matches;
        const auto ranges = KFuzzyMatcher::matchedRanges(_filterText, QStringView(display).mid(start));
        for (const KFuzzyMatcher::Range& range : ranges) {
            matches << range.start + start << range.length;
        }
        return matches;
    }
    case ShortcutRole:
        return item.action->shortcut().toString();
    case ToolTipRole: {
        QString toolTip = display;
        if (!item.action->shortcut().isEmpty()) {
            toolTip += QLatin1Char('\n') + item.action->shortcut().toString();
        }
        return toolTip;
    }
    }
    return {};
}

QHash<int, QByteArray> CommandBarModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {IconNameRole, "iconName"},
        {GroupLengthRole, "groupLength"},
        {MatchesRole, "matches"},
        {ShortcutRole, "shortcut"},
        {ToolTipRole, "toolTip"},
    };
}

QString CommandBarModel::filterText() const
{
    return _filterText;
}

void CommandBarModel::setFilterText(const QString& text)
{
    if (_filterText == text) {
        return;
    }
    _filterText = text;
    filter();
    Q_EMIT filterTextChanged();
}

bool CommandBarModel::hasIcons() const
{
    return std::any_of(_shown.cbegin(), _shown.cend(), [this](int i) {
        return _all.at(i).action && !_all.at(i).action->icon().isNull();
    });
}

QString CommandBarModel::placeholderText() const
{
    if (!_shown.isEmpty()) {
        return QString();
    }
    return _all.isEmpty() ? i18nd("kconfigwidgets6", "No commands to display") : i18nd("kconfigwidgets6", "No commands matching the filter");
}

bool CommandBarModel::activate(int row)
{
    if (row < 0 || row >= _shown.size()) {
        return false;
    }
    QAction* action = _all.at(_shown.at(row)).action;
    if (!action) {
        return false;
    }
    if (QMenu* menu = action->menu()) {
        // KCommandBarPrivate::slotReturnPressed(): only a menu filled on demand gives rows.
        KCommandBar::ActionGroup group;
        if (menu->actions().isEmpty()) {
            Q_EMIT menu->aboutToShow();
            group.actions = menu->actions();
        }
        group.name = KLocalizedString::removeAcceleratorMarker(action->text());
        refresh({group});
        setFilterText(QString());
        return true;
    }
    // KCommandBarModel::actionTriggered()
    if (_lastTriggered.size() == 6) {
        _lastTriggered.removeLast();
    }
    _lastTriggered.prepend(action->text());
    sonicDebug() << "command bar triggers" << action->objectName() << action->text();
    // After the bar has closed, as KCommandBar hides before triggering.
    QTimer::singleShot(0, action, &QAction::trigger);
    return false;
}

QStringList CommandBarModel::shortcutSegments(const QString& shortcut)
{
    // ShortcutStyleDelegate::splitShortcut(): "Ctrl+A, Ctrl+K" → Ctrl, +, A, ", ", Ctrl, +, K.
    QStringList segments;
    const int length = shortcut.length();
    int start = 0;
    for (int i = 0; i < length; ++i) {
        const QChar c = shortcut.at(i);
        if (c == QLatin1Char('+')) {
            if (i > start) {
                segments << shortcut.mid(start, i - start);
            }
            segments << shortcut.at(i);
            start = i + 1;
        } else if (c == QLatin1Char(',')) {
            if (i > start) {
                segments << shortcut.mid(start, i - start);
                start = i;
            }
            const int j = i + 1;
            if (j < length && shortcut.at(j) == QLatin1Char(' ')) {
                segments << shortcut.mid(start, j - start + 1);
                i = j;
            } else {
                segments << shortcut.at(i);
            }
            start = i + 1;
        }
    }
    if (start < length) {
        segments << shortcut.mid(start);
    }
    return segments;
}

#include "moc_CommandBarModel.cpp"
