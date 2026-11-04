/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ShortcutsEditorModel.h"

#include "FrameworkDialogScope.h"
#include "ShortcutStore.h"
#include "config-konsole.h"

#include <Sonic/Logging/Log>

#include <KActionCategory>
#include <KActionCollection>
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>
#include <KStandardGuiItem>
#include <KXMLGUIClient>
#include <KXMLGUIFactory>

#if HAVE_DBUS
#include <KGlobalAccel>
#include <KGlobalShortcutInfo>
#endif

#include <QAction>
#include <QApplication>
#include <QCollator>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QInputDialog>
#include <QPrintDialog>
#include <QPrinter>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QTextTable>
#include <QWindow>

#include <functional>

using namespace Konsole;

namespace {
QKeySequence primarySequence(const QList<QKeySequence>& sequences)
{
    return sequences.isEmpty() ? QKeySequence() : sequences.at(0);
}

QKeySequence alternateSequence(const QList<QKeySequence>& sequences)
{
    return sequences.size() <= 1 ? QKeySequence() : sequences.at(1);
}

QList<QKeySequence> defaultShortcuts(const QAction* action)
{
    return action->property("defaultShortcuts").value<QList<QKeySequence>>();
}

// KKeySequenceWidget's shortcutsConflictWith().
bool shortcutsConflictWith(const QList<QKeySequence>& shortcuts, const QKeySequence& needle)
{
    if (needle.isEmpty()) {
        return false;
    }
    for (const QKeySequence& sequence : shortcuts) {
        if (sequence.isEmpty()) {
            continue;
        }
        if (sequence.matches(needle) != QKeySequence::NoMatch || needle.matches(sequence) != QKeySequence::NoMatch) {
            return true;
        }
    }
    return false;
}

void setTransientFor(QWidget* dialog, QQuickItem* from)
{
    if (from && from->window()) {
        dialog->winId();
        dialog->windowHandle()->setTransientParent(from->window());
    }
}

// A KMessageBox box transient for the QML dialog (a widget parent would make it transient
// for a hidden compat window); returns the clicked standard button.
int messageBox(QQuickItem* from,
    QMessageBox::Icon icon,
    const QString& objectName,
    const QString& title,
    const QString& text,
    QDialogButtonBox::StandardButtons buttons,
    const KGuiItem& yes = KGuiItem(),
    const KGuiItem& no = KGuiItem())
{
    FrameworkDialogScope scope(QStringLiteral("shortcuts message"));
    auto* dialog = new QDialog;
    dialog->setWindowTitle(title);
    dialog->setObjectName(objectName);
    dialog->setModal(true);
    auto* buttonBox = new QDialogButtonBox(dialog);
    buttonBox->setStandardButtons(buttons);
    if (buttons & QDialogButtonBox::Yes) {
        KGuiItem::assign(buttonBox->button(QDialogButtonBox::Yes), yes);
        KGuiItem::assign(buttonBox->button(QDialogButtonBox::No), no);
    }
    setTransientFor(dialog, from);
    return KMessageBox::createKMessageBox(dialog, buttonBox, icon, text, QStringList(), QString(), nullptr, KMessageBox::Notify);
}

// KMessageBox::warningContinueCancel() with a Continue item.
bool warningContinueCancel(QQuickItem* from, const QString& text, const QString& title, const KGuiItem& cont)
{
    const QString caption = title.isEmpty() ? QApplication::translate("KMessageBox", "Warning") : title;
    return messageBox(from, QMessageBox::Warning, QStringLiteral("warningYesNo"), caption, text, QDialogButtonBox::Yes | QDialogButtonBox::No, cont,
               KStandardGuiItem::cancel())
        == QDialogButtonBox::Yes;
}

void errorBox(QQuickItem* from, const QString& text, const QString& title = QString())
{
    const QString caption = title.isEmpty() ? QApplication::translate("KMessageBox", "Error") : title;
    messageBox(from, QMessageBox::Critical, QStringLiteral("error"), caption, text, QDialogButtonBox::Ok);
}

QString writableSchemeFile(const QString& component, const QString& scheme)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/%1/shortcuts/%2").arg(component, scheme);
}
}

ShortcutsEditorModel::ShortcutsEditorModel(const QList<KActionCollection*>& collections, QObject* parent)
    : QAbstractListModel(parent)
{
    for (KActionCollection* collection : collections) {
        // KShortcutsEditor::addCollection() skips empty ones.
        if (collection && !collection->isEmpty()) {
            _collections.append(collection);
        }
    }
    build();
    refreshSchemes();
}

ShortcutsEditorModel::~ShortcutsEditorModel() = default;

QList<KActionCollection*> ShortcutsEditorModel::collections() const
{
    QList<KActionCollection*> result;
    for (const QPointer<KActionCollection>& collection : _collections) {
        if (collection) {
            result.append(collection);
        }
    }
    return result;
}

void ShortcutsEditorModel::build()
{
    // KShortcutsEditor::addCollection(): a top item per display name, KActionCategory items,
    // configurable named actions; then sortItems(Name).
    struct Node {
        Kind kind;
        QString name;
        QPointer<QAction> action;
        QList<Node> children;
    };
    QList<Node> tops;
    auto findOrMake = [](QList<Node>& nodes, Kind kind, const QString& name) -> Node& {
        for (Node& node : nodes) {
            if (node.kind != ActionItem && node.name == name) {
                return node;
            }
        }
        nodes.append({kind, name, nullptr, {}});
        return nodes.last();
    };
    auto addAction = [](QList<Node>& nodes, QAction* action) {
        const QString id = action->objectName();
        if (id.isEmpty() || id.startsWith(QLatin1String("unnamed-")) || !KActionCollection::isShortcutsConfigurable(action)) {
            return;
        }
        QString name = i18ndc("kxmlgui6", "@item:intable Action name in shortcuts configuration", "%1", KLocalizedString::removeAcceleratorMarker(action->text()));
        if (name.isEmpty()) {
            name = id;
        }
        nodes.append({ActionItem, name, action, {}});
    };
    for (const QPointer<KActionCollection>& collection : std::as_const(_collections)) {
        Node& top = findOrMake(tops, TopItem, collection->componentDisplayName());
        QSet<QAction*> seen;
        const auto categories = collection->findChildren<KActionCategory*>();
        for (KActionCategory* category : categories) {
            Node& categoryNode = findOrMake(top.children, CategoryItem, category->text());
            const auto actions = category->actions();
            for (QAction* action : actions) {
                seen.insert(action);
                addAction(categoryNode.children, action);
            }
        }
        const auto actions = collection->actions();
        for (QAction* action : actions) {
            if (!seen.contains(action)) {
                addAction(top.children, action);
            }
        }
    }

    // KShortcutsEditorItem compares with a numeric, case-sensitive collator; other items by text.
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseSensitive);
    std::function<void(QList<Node>&)> sort = [&](QList<Node>& nodes) {
        std::stable_sort(nodes.begin(), nodes.end(), [&collator](const Node& a, const Node& b) {
            if (a.kind == ActionItem && b.kind == ActionItem) {
                return collator.compare(a.name, b.name) < 0;
            }
            return a.name < b.name;
        });
        for (Node& node : nodes) {
            sort(node.children);
        }
    };
    sort(tops);

    beginResetModel();
    _rows.clear();
    std::function<void(const QList<Node>&, int)> flatten = [&](const QList<Node>& nodes, int depth) {
        for (const Node& node : nodes) {
            Row row;
            row.kind = node.kind;
            row.depth = depth;
            row.name = node.name;
            row.action = node.action;
            _rows.append(row);
            flatten(node.children, depth + 1);
        }
    };
    flatten(tops, 0);
    filter();
    endResetModel();
}

int ShortcutsEditorModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : _rows.size();
}

QVariant ShortcutsEditorModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= _rows.size()) {
        return {};
    }
    const Row& row = _rows.at(index.row());
    const QList<QKeySequence> shortcuts = row.action ? row.action->shortcuts() : QList<QKeySequence>();
    const QList<QKeySequence> defaults = row.action ? defaultShortcuts(row.action) : QList<QKeySequence>();
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return row.name;
    case KindRole:
        return row.kind;
    case DepthRole:
        return row.depth;
    case IconNameRole:
        return row.action ? row.action->icon().name() : QString();
    case PrimaryRole:
        return primarySequence(shortcuts).toString(QKeySequence::NativeText);
    case AlternateRole:
        return alternateSequence(shortcuts).toString(QKeySequence::NativeText);
    case DefaultPrimaryRole:
        return primarySequence(defaults).toString(QKeySequence::NativeText);
    case DefaultAlternateRole:
        return alternateSequence(defaults).toString(QKeySequence::NativeText);
    case PrimarySequenceRole:
        return primarySequence(shortcuts);
    case AlternateSequenceRole:
        return alternateSequence(shortcuts);
    case DefaultPrimarySequenceRole:
        return primarySequence(defaults);
    case DefaultAlternateSequenceRole:
        return alternateSequence(defaults);
    case ShownRole:
        return row.shown;
    case IdRole:
        return row.action ? row.action->objectName() : QString();
    case VisibleRole:
        return visible(index.row());
    case HasChildrenRole:
        return index.row() + 1 < _rows.size() && _rows.at(index.row() + 1).depth > row.depth;
    case CollapsedRole:
        return row.collapsed;
    }
    return {};
}

bool ShortcutsEditorModel::visible(int row) const
{
    if (!_rows.at(row).shown) {
        return false;
    }
    int depth = _rows.at(row).depth;
    for (int above = row - 1; above >= 0 && depth > 0; --above) {
        if (_rows.at(above).depth < depth) {
            if (_rows.at(above).collapsed) {
                return false;
            }
            depth = _rows.at(above).depth;
        }
    }
    return true;
}

void ShortcutsEditorModel::toggleCollapsed(int row)
{
    if (row < 0 || row >= _rows.size() || _rows.at(row).kind == ActionItem) {
        return;
    }
    _rows[row].collapsed = !_rows.at(row).collapsed;
    Q_EMIT dataChanged(index(row), index(_rows.size() - 1), {VisibleRole, CollapsedRole});
}

QList<int> ShortcutsEditorModel::depths() const
{
    QList<int> result;
    for (const Row& row : _rows) {
        result.append(row.depth);
    }
    return result;
}

QStringList ShortcutsEditorModel::columnTexts(int column) const
{
    QStringList texts;
    for (const Row& row : _rows) {
        const QStringList rowColumns = rowTexts(row);
        texts.append(rowColumns.value(column));
    }
    return texts;
}

QHash<int, QByteArray> ShortcutsEditorModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {DepthRole, "depth"},
        {NameRole, "name"},
        {IconNameRole, "iconName"},
        {PrimaryRole, "primary"},
        {AlternateRole, "alternate"},
        {DefaultPrimaryRole, "defaultPrimary"},
        {DefaultAlternateRole, "defaultAlternate"},
        {PrimarySequenceRole, "primarySequence"},
        {AlternateSequenceRole, "alternateSequence"},
        {DefaultPrimarySequenceRole, "defaultPrimarySequence"},
        {DefaultAlternateSequenceRole, "defaultAlternateSequence"},
        {ShownRole, "shown"},
        {IdRole, "actionId"},
        {VisibleRole, "rowVisible"},
        {HasChildrenRole, "hasChildren"},
        {CollapsedRole, "collapsed"},
    };
}

QString ShortcutsEditorModel::filterText() const
{
    return _filterText;
}

void ShortcutsEditorModel::setFilterText(const QString& text)
{
    if (_filterText == text) {
        return;
    }
    _filterText = text;
    filter();
    if (!_rows.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(_rows.size() - 1), {ShownRole, VisibleRole});
    }
    Q_EMIT filterTextChanged();
}

QStringList ShortcutsEditorModel::rowTexts(const Row& row) const
{
    if (!row.action) {
        return {row.name};
    }
    const QList<QKeySequence> shortcuts = row.action->shortcuts();
    return {row.name, primarySequence(shortcuts).toString(QKeySequence::NativeText), alternateSequence(shortcuts).toString(QKeySequence::NativeText)};
}

void ShortcutsEditorModel::filter()
{
    // KTreeWidgetSearchLine: an item is shown when one of its visible columns contains the
    // text (any case) or one of its children is shown.
    std::function<bool(int&)> visit = [&](int& i) -> bool {
        Row& row = _rows[i];
        const int depth = row.depth;
        const int self = i;
        bool childShown = false;
        ++i;
        while (i < _rows.size() && _rows.at(i).depth > depth) {
            childShown = visit(i) || childShown;
        }
        const QStringList texts = rowTexts(_rows.at(self));
        const bool matches = _filterText.isEmpty() || std::any_of(texts.cbegin(), texts.cend(), [this](const QString& text) {
            return text.contains(_filterText, Qt::CaseInsensitive);
        });
        _rows[self].shown = matches || childShown;
        return _rows.at(self).shown;
    };
    int i = 0;
    while (i < _rows.size()) {
        visit(i);
    }
}

void ShortcutsEditorModel::rowChanged(int row)
{
    Q_EMIT dataChanged(index(row), index(row));
}

void ShortcutsEditorModel::setKeySequence(int row, int column, const QKeySequence& sequence)
{
    // KShortcutsEditorItem::setKeySequence(): the action changes at once, its old value kept.
    Row& entry = _rows[row];
    QList<QKeySequence> shortcuts = entry.action->shortcuts();
    if (!entry.old) {
        entry.old = shortcuts;
    }
    if (column == Alternate) {
        if (shortcuts.isEmpty()) {
            shortcuts << QKeySequence();
        }
        if (shortcuts.size() <= 1) {
            shortcuts << sequence;
        } else {
            shortcuts[1] = sequence;
        }
    } else if (shortcuts.isEmpty()) {
        shortcuts << sequence;
    } else {
        shortcuts[0] = sequence;
    }
    entry.action->setShortcuts(shortcuts);
    if (*entry.old == entry.action->shortcuts()) {
        entry.old.reset();
    }
    rowChanged(row);
}

bool ShortcutsEditorModel::setShortcut(int row, int column, const QKeySequence& keySequence, QQuickItem* from)
{
    if (row < 0 || row >= _rows.size() || !_rows.at(row).action || (column != Primary && column != Alternate)) {
        return false;
    }
    const QList<QKeySequence> shortcuts = _rows.at(row).action->shortcuts();
    if ((column == Primary ? primarySequence(shortcuts) : alternateSequence(shortcuts)) == keySequence) {
        return true;
    }
    // KKeySequenceWidget::isKeySequenceAvailable() for a local column: local, then global.
    if (!keySequence.isEmpty() && (conflictWithLocalShortcuts(keySequence, from) || conflictWithGlobalShortcuts(keySequence, from))) {
        return false;
    }
    setKeySequence(row, column, keySequence);
    sonicDebug() << "shortcut edited:" << _rows.at(row).action->objectName() << column << keySequence;
    return true;
}

bool ShortcutsEditorModel::conflictWithLocalShortcuts(const QKeySequence& sequence, QQuickItem* from)
{
    QList<QAction*> conflicting;
    for (const QPointer<KActionCollection>& collection : std::as_const(_collections)) {
        if (!collection) {
            continue;
        }
        const auto actions = collection->actions();
        for (QAction* action : actions) {
            if (!shortcutsConflictWith(action->shortcuts(), sequence)) {
                continue;
            }
            if (!KActionCollection::isShortcutsConfigurable(action)) {
                // wontStealShortcut()
                errorBox(from,
                    i18nd("kxmlgui6",
                        "<qt>The '%1' key combination is already used by the <b>%2</b> action.<br>"
                        "Please select a different one.</qt>",
                        sequence.toString(QKeySequence::NativeText),
                        KLocalizedString::removeAcceleratorMarker(action->text())),
                    i18ndc("kxmlgui6", "@title:window", "Shortcut conflict"));
                return true;
            }
            conflicting.append(action);
        }
    }
    if (conflicting.isEmpty()) {
        return false;
    }
    // promptStealLocalShortcut()
    QString list;
    for (const QAction* action : std::as_const(conflicting)) {
        list += i18nd("kxmlgui6",
            "Shortcut '%1' for action '%2'\n",
            action->shortcut().toString(QKeySequence::NativeText),
            KLocalizedString::removeAcceleratorMarker(action->text()));
    }
    const QString message = i18ndcp("kxmlgui6",
        "%1 is the number of ambiguous shortcut clashes (hidden)",
        "The \"%2\" shortcut is ambiguous with the following shortcut.\n"
        "Do you want to assign an empty shortcut to this action?\n"
        "%3",
        "The \"%2\" shortcut is ambiguous with the following shortcuts.\n"
        "Do you want to assign an empty shortcut to these actions?\n"
        "%3",
        conflicting.size(),
        sequence.toString(QKeySequence::NativeText),
        list);
    const QString title = i18ndcp("kxmlgui6", "%1 is the number of conflicts", "Shortcut Conflict", "Shortcut Conflicts", conflicting.size());
    if (!warningContinueCancel(from, message, title, KGuiItem(i18ndc("kxmlgui6", "@action:button", "Reassign")))) {
        return true;
    }
    for (QAction* action : std::as_const(conflicting)) {
        stealShortcut(sequence, action);
    }
    return false;
}

bool ShortcutsEditorModel::conflictWithGlobalShortcuts(const QKeySequence& sequence, QQuickItem* from)
{
#if HAVE_DBUS
    // As KKeySequenceWidget (which tests the whole sequence for each of its keys).
    QList<QPair<QKeySequence, QList<KGlobalShortcutInfo>>> clashing;
    for (int i = 0; i < sequence.count(); ++i) {
        if (!KGlobalAccel::isGlobalShortcutAvailable(sequence, QString())) {
            clashing.append({sequence, KGlobalAccel::globalShortcutsByKey(QKeySequence(sequence[i]))});
        }
    }
    if (clashing.isEmpty()) {
        return false;
    }
    QString list;
    for (const auto& [key, infos] : std::as_const(clashing)) {
        for (const KGlobalShortcutInfo& info : infos) {
            list += i18nd("kxmlgui6", "Shortcut '%1' in Application '%2' for action '%3'\n", key.toString(), info.componentFriendlyName(), info.friendlyName());
        }
    }
    QString message = i18ndcp("kxmlgui6",
        "%1 is the number of conflicts (hidden), %2 is the key sequence of the shortcut that is problematic",
        "The shortcut '%2' conflicts with the following key combination:\n",
        "The shortcut '%2' conflicts with the following key combinations:\n",
        clashing.size(),
        sequence.toString());
    message += list;
    const QString title = i18ndcp("kxmlgui6",
        "%1 is the number of shortcuts with which there is a conflict",
        "Conflict with Registered Global Shortcut",
        "Conflict with Registered Global Shortcuts",
        clashing.size());
    if (!warningContinueCancel(from, message, title, KGuiItem(i18ndc("kxmlgui6", "@action:button", "Reassign")))) {
        return true;
    }
    KGlobalAccel::stealShortcutSystemwide(sequence);
    return false;
#else
    Q_UNUSED(sequence)
    Q_UNUSED(from)
    return false;
#endif
}

void ShortcutsEditorModel::stealShortcut(const QKeySequence& sequence, QAction* action)
{
    // KShortcutsEditorDelegate::stealShortcut(): the listed action loses the matching ones.
    for (int row = 0; row < _rows.size(); ++row) {
        if (_rows.at(row).action != action) {
            continue;
        }
        const QList<QKeySequence> shortcuts = action->shortcuts();
        const QKeySequence primary = primarySequence(shortcuts);
        const QKeySequence alternate = alternateSequence(shortcuts);
        if (primary.matches(sequence) != QKeySequence::NoMatch || sequence.matches(primary) != QKeySequence::NoMatch) {
            setKeySequence(row, Primary, QKeySequence());
        }
        if (alternate.matches(sequence) != QKeySequence::NoMatch || sequence.matches(alternate) != QKeySequence::NoMatch) {
            setKeySequence(row, Alternate, QKeySequence());
        }
        return;
    }
}

bool ShortcutsEditorModel::isModified() const
{
    return std::any_of(_rows.cbegin(), _rows.cend(), [](const Row& row) {
        return row.old.has_value();
    });
}

void ShortcutsEditorModel::allDefault()
{
    for (int row = 0; row < _rows.size(); ++row) {
        QAction* action = _rows.at(row).action;
        if (!action) {
            continue;
        }
        const QList<QKeySequence> defaults = defaultShortcuts(action);
        if (action->shortcuts() != defaults) {
            setKeySequence(row, Primary, primarySequence(defaults));
            setKeySequence(row, Alternate, alternateSequence(defaults));
        }
    }
}

void ShortcutsEditorModel::undo()
{
    for (int row = 0; row < _rows.size(); ++row) {
        Row& entry = _rows[row];
        if (entry.old && entry.action) {
            entry.action->setShortcuts(*entry.old);
            rowChanged(row);
        }
        entry.old.reset();
    }
}

void ShortcutsEditorModel::save()
{
    for (Row& row : _rows) {
        row.old.reset();
    }
    ShortcutStore::instance()->save(collections());
}

void ShortcutsEditorModel::print(QQuickItem* from)
{
    // KShortcutsEditorPrivate::printShortcuts()
    QTextDocument doc;
    doc.setDefaultFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));
    QTextCursor cursor(&doc);
    cursor.beginEditBlock();
    QTextCharFormat headerFormat;
    headerFormat.setProperty(QTextFormat::FontSizeAdjustment, 3);
    headerFormat.setFontWeight(QFont::Bold);
    cursor.insertText(i18ndc("kxmlgui6", "header for an applications shortcut list", "Shortcuts for %1", QGuiApplication::applicationDisplayName()), headerFormat);
    QTextCharFormat componentFormat;
    componentFormat.setProperty(QTextFormat::FontSizeAdjustment, 2);
    componentFormat.setFontWeight(QFont::Bold);
    QTextBlockFormat componentBlockFormat = cursor.blockFormat();
    componentBlockFormat.setTopMargin(16);
    componentBlockFormat.setBottomMargin(16);
    QTextTableFormat tableformat;
    tableformat.setHeaderRowCount(1);
    tableformat.setCellPadding(4.0);
    tableformat.setCellSpacing(0);
    tableformat.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
    tableformat.setBorder(0.5);
    const QPair<QString, int> columns[] = {{i18nd("kxmlgui6", "Main:"), Primary}, {i18nd("kxmlgui6", "Alternate:"), Alternate}};

    for (int top = 0; top < _rows.size(); ++top) {
        if (_rows.at(top).kind != TopItem) {
            continue;
        }
        cursor.insertBlock(componentBlockFormat, componentFormat);
        cursor.insertText(_rows.at(top).name);
        QTextTable* table = cursor.insertTable(1, 3);
        table->setFormat(tableformat);
        int currow = 0;
        QTextTableCell cell = table->cellAt(currow, 0);
        QTextCharFormat format = cell.format();
        format.setFontWeight(QFont::Bold);
        cell.setFormat(format);
        cell.firstCursorPosition().insertText(i18nd("kxmlgui6", "Action Name"));
        cell = table->cellAt(currow, 1);
        cell.setFormat(format);
        cell.firstCursorPosition().insertText(i18nd("kxmlgui6", "Shortcuts"));
        cell = table->cellAt(currow, 2);
        cell.setFormat(format);
        cell.firstCursorPosition().insertText(i18nd("kxmlgui6", "Description"));
        currow++;
        for (int row = top + 1; row < _rows.size() && _rows.at(row).depth > 0; ++row) {
            QAction* action = _rows.at(row).action;
            if (!action) {
                continue;
            }
            table->insertRows(table->rows(), 1);
            table->cellAt(currow, 0).firstCursorPosition().insertText(_rows.at(row).name);
            QTextTable* shortcutTable = nullptr;
            for (const auto& [title, column] : columns) {
                const QList<QKeySequence> shortcuts = action->shortcuts();
                const QString key = (column == Primary ? primarySequence(shortcuts) : alternateSequence(shortcuts)).toString();
                if (key.isEmpty()) {
                    continue;
                }
                if (!shortcutTable) {
                    shortcutTable = table->cellAt(currow, 1).firstCursorPosition().insertTable(1, 2);
                    QTextTableFormat shortcutTableFormat = tableformat;
                    shortcutTableFormat.setCellSpacing(0.0);
                    shortcutTableFormat.setHeaderRowCount(0);
                    shortcutTableFormat.setBorder(0.0);
                    shortcutTable->setFormat(shortcutTableFormat);
                } else {
                    shortcutTable->insertRows(shortcutTable->rows(), 1);
                }
                shortcutTable->cellAt(shortcutTable->rows() - 1, 0).firstCursorPosition().insertText(title);
                shortcutTable->cellAt(shortcutTable->rows() - 1, 1).firstCursorPosition().insertText(key);
            }
            cell = table->cellAt(currow, 2);
            format = cell.format();
            format.setProperty(QTextFormat::FontSizeAdjustment, -1);
            cell.setFormat(format);
            cell.firstCursorPosition().insertHtml(action->whatsThis());
            currow++;
        }
        cursor.movePosition(QTextCursor::End);
    }
    cursor.endEditBlock();

    FrameworkDialogScope scope(QStringLiteral("print shortcuts"));
    QPrinter printer;
    QPrintDialog dialog(&printer);
    setTransientFor(&dialog, from);
    if (dialog.exec() == QDialog::Accepted) {
        doc.print(&printer);
    }
}

void ShortcutsEditorModel::refreshSchemes()
{
    // KShortcutSchemesEditor::refreshSchemes()
    QStringList schemes{QStringLiteral("Default")};
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
        QCoreApplication::applicationName() + QLatin1String("/shortcuts"),
        QStandardPaths::LocateDirectory);
    for (const QString& dir : dirs) {
        schemes += QDir(dir).entryList(QDir::Files | QDir::NoDotAndDotDot);
    }
    _schemes = schemes;
    const QString current = KConfigGroup(KSharedConfig::openConfig(), QStringLiteral("Shortcut Schemes")).readEntry("Current Scheme", "Default");
    _currentScheme = _schemes.contains(current) ? current : _schemes.constFirst();
    Q_EMIT schemesChanged();
}

QStringList ShortcutsEditorModel::schemes() const
{
    return _schemes;
}

QString ShortcutsEditorModel::currentScheme() const
{
    return _currentScheme;
}

void ShortcutsEditorModel::changeScheme(const QString& scheme, QQuickItem* from)
{
    // KShortcutsDialogPrivate::changeShortcutScheme()
    if (isModified()
        && messageBox(from,
               QMessageBox::Information,
               QStringLiteral("questionYesNo"),
               QApplication::translate("KMessageBox", "Question"),
               i18nd("kxmlgui6", "The current shortcut scheme is modified. Save before switching to the new one?"),
               QDialogButtonBox::Yes | QDialogButtonBox::No,
               KStandardGuiItem::save(),
               KStandardGuiItem::discard())
            == QDialogButtonBox::Yes) {
        save();
    } else {
        undo();
    }
    const QList<KActionCollection*> all = collections();
    for (KActionCollection* collection : all) {
        if (auto* client = const_cast<KXMLGUIClient*>(collection->parentGUIClient())) {
            client->setXMLGUIBuildDocument(QDomDocument());
        }
    }
    if (!all.isEmpty() && all.constFirst()->parentGUIClient() && all.constFirst()->parentGUIClient()->factory()) {
        all.constFirst()->parentGUIClient()->factory()->changeShortcutScheme(scheme);
    }
    // The user's shortcuts stay over the scheme's, as the .rc's did.
    ShortcutStore::instance()->reapply();
    _currentScheme = scheme;
    Q_EMIT schemesChanged();
    build();
    sonicInfo() << "shortcut scheme:" << scheme;
}

void ShortcutsEditorModel::newScheme(QQuickItem* from)
{
    bool ok = false;
    QString name;
    {
        FrameworkDialogScope scope(QStringLiteral("new shortcut scheme"));
        QInputDialog dialog;
        dialog.setWindowTitle(i18ndc("kxmlgui6", "@title:window", "Name for New Scheme"));
        dialog.setLabelText(i18nd("kxmlgui6", "Name for new scheme:"));
        dialog.setTextValue(i18nd("kxmlgui6", "New Scheme"));
        setTransientFor(&dialog, from);
        ok = dialog.exec() == QDialog::Accepted;
        name = dialog.textValue();
    }
    if (!ok) {
        return;
    }
    if (_schemes.contains(name)) {
        errorBox(from, i18nd("kxmlgui6", "A scheme with this name already exists."));
        return;
    }
    const QString fileName = writableSchemeFile(QCoreApplication::applicationName(), name);
    QDir().mkpath(QFileInfo(fileName).absolutePath());
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
        sonicWarning() << "Couldn't write to" << fileName;
        return;
    }
    QDomDocument doc;
    QDomElement docElem = doc.createElement(QStringLiteral("gui"));
    doc.appendChild(docElem);
    docElem.appendChild(doc.createElement(QStringLiteral("ActionProperties")));
    QTextStream out(&file);
    out << doc.toString(4);
    file.close();
    _schemes.append(name);
    changeScheme(name, from);
}

void ShortcutsEditorModel::deleteScheme(QQuickItem* from)
{
    if (messageBox(from,
            QMessageBox::Information,
            QStringLiteral("questionYesNo"),
            QApplication::translate("KMessageBox", "Question"),
            i18nd("kxmlgui6",
                "Do you really want to delete the scheme %1?\n"
                "Note that this will not remove any system wide shortcut schemes.",
                _currentScheme),
            QDialogButtonBox::Yes | QDialogButtonBox::No,
            KStandardGuiItem::del(),
            KStandardGuiItem::cancel())
        != QDialogButtonBox::Yes) {
        return;
    }
    QFile::remove(writableSchemeFile(QCoreApplication::applicationName(), _currentScheme));
    const QList<KActionCollection*> all = collections();
    for (KActionCollection* collection : all) {
        if (const KXMLGUIClient* client = collection->parentGUIClient()) {
            QFile::remove(writableSchemeFile(client->componentName(), _currentScheme));
        }
    }
    // QComboBox::removeItem() of the current item makes the one after it current.
    const int removed = _schemes.indexOf(_currentScheme);
    _schemes.removeAt(removed);
    changeScheme(_schemes.value(std::min<int>(removed, _schemes.size() - 1)), from);
}

void ShortcutsEditorModel::saveToScheme(QQuickItem* from)
{
    // KShortcutSchemesHelper::saveShortcutScheme(): a file per client component.
    QMultiMap<QString, KActionCollection*> byComponent;
    const QList<KActionCollection*> all = collections();
    for (KActionCollection* collection : all) {
        if (const KXMLGUIClient* client = collection->parentGUIClient()) {
            byComponent.insert(client->componentName(), collection);
        }
    }
    bool saved = true;
    const auto components = byComponent.uniqueKeys();
    for (const QString& component : components) {
        QDomDocument doc;
        QDomElement docElem = doc.createElement(QStringLiteral("gui"));
        docElem.setAttribute(QStringLiteral("version"), QStringLiteral("1"));
        docElem.setAttribute(QStringLiteral("name"), component);
        doc.appendChild(docElem);
        QDomElement elem = doc.createElement(QStringLiteral("ActionProperties"));
        docElem.appendChild(elem);
        const auto componentCollections = byComponent.values(component);
        for (KActionCollection* collection : componentCollections) {
            const auto actions = collection->actions();
            for (QAction* action : actions) {
                const QString shortcut = QKeySequence::listToString(action->shortcuts());
                if (!shortcut.isEmpty()) {
                    QDomElement actionElem = doc.createElement(QStringLiteral("Action"));
                    actionElem.setAttribute(QStringLiteral("name"), action->objectName());
                    actionElem.setAttribute(QStringLiteral("shortcut"), shortcut);
                    elem.appendChild(actionElem);
                }
            }
        }
        const QString fileName = writableSchemeFile(component, _currentScheme);
        if (elem.childNodes().isEmpty()) {
            QFile::remove(fileName);
            continue;
        }
        QDir().mkpath(QFileInfo(fileName).absolutePath());
        QFile file(fileName);
        if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
            saved = false;
            break;
        }
        QTextStream out(&file);
        out << doc.toString(2);
    }
    if (saved) {
        messageBox(from,
            QMessageBox::Information,
            QStringLiteral("information"),
            QApplication::translate("KMessageBox", "Information"),
            i18nd("kxmlgui6", "Shortcut scheme successfully saved."),
            QDialogButtonBox::Ok);
    } else {
        errorBox(from, i18nd("kxmlgui6", "Error saving the shortcut scheme."));
    }
}

void ShortcutsEditorModel::exportScheme(QQuickItem* from)
{
    QString path;
    {
        FrameworkDialogScope scope(QStringLiteral("export shortcuts"));
        QFileDialog dialog(nullptr, i18ndc("kxmlgui6", "@title:window", "Export Shortcuts"), QDir::currentPath(), i18nd("kxmlgui6", "Shortcuts (*.shortcuts)"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        setTransientFor(&dialog, from);
        if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
            path = dialog.selectedFiles().constFirst();
        }
    }
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path.append(QStringLiteral(".shortcuts"));
    }
    // KShortcutsEditor::exportConfiguration() for local actions.
    KConfig config(path);
    KConfigGroup group(&config, QStringLiteral("Shortcuts"));
    const QList<KActionCollection*> all = collections();
    for (KActionCollection* collection : all) {
        collection->writeSettings(&group, true);
    }
}

void ShortcutsEditorModel::importScheme(QQuickItem* from)
{
    QString path;
    {
        FrameworkDialogScope scope(QStringLiteral("import shortcuts"));
        QFileDialog dialog(nullptr, i18ndc("kxmlgui6", "@title:window", "Import Shortcuts"), QDir::currentPath(), i18nd("kxmlgui6", "Shortcuts (*.shortcuts)"));
        dialog.setFileMode(QFileDialog::ExistingFile);
        setTransientFor(&dialog, from);
        if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
            path = dialog.selectedFiles().constFirst();
        }
    }
    if (path.isEmpty()) {
        return;
    }
    // KShortcutsEditorPrivate::importConfiguration() for local actions.
    KConfig config(path);
    const KConfigGroup group(&config, QStringLiteral("Shortcuts"));
    for (int row = 0; row < _rows.size(); ++row) {
        QAction* action = _rows.at(row).action;
        if (!action || !group.hasKey(action->objectName())) {
            continue;
        }
        const QList<QKeySequence> shortcuts = QKeySequence::listFromString(group.readEntry(action->objectName(), QString()));
        setKeySequence(row, Primary, primarySequence(shortcuts));
        setKeySequence(row, Alternate, alternateSequence(shortcuts));
    }
}

QSize ShortcutsEditorModel::dialogSize()
{
    return KConfigGroup(KSharedConfig::openConfig(), QStringLiteral("KShortcutsDialog Settings")).readEntry("Dialog Size", QSize(600, 480));
}

void ShortcutsEditorModel::saveDialogSize(const QSize& size)
{
    KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("KShortcutsDialog Settings"));
    group.writeEntry("Dialog Size", size, KConfigGroup::Persistent | KConfigGroup::Global);
}

#include "moc_ShortcutsEditorModel.cpp"
