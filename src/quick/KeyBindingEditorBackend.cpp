/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "KeyBindingEditorBackend.h"

#include "keyboardtranslator/KeyboardTranslatorManager.h"
#include "keyboardtranslator/KeyboardTranslatorReader.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>

namespace Konsole {
KeyBindingEditorBackend::KeyBindingEditorBackend(const KeyboardTranslator* translator,
    const QString& currentProfileTranslator,
    bool isNewTranslator,
    QObject* parent)
    : QAbstractListModel(parent)
    , _translator(std::make_unique<KeyboardTranslator>(*translator))
    , _currentProfileTranslator(currentProfileTranslator)
    , _isNewTranslator(isNewTranslator)
{
    if (isNewTranslator) {
        _translator->setDescription(i18n("New Key Binding List"));
    }
    const QList<KeyboardTranslator::Entry> entries = translator->entries();
    for (const KeyboardTranslator::Entry& entry : entries) {
        _rows.append({entry, entry.conditionToString(), entry.resultToString()});
    }
    // QTableWidget::sortItems(0): by key combination.
    std::stable_sort(_rows.begin(), _rows.end(), [](const Row& a, const Row& b) {
        return a.condition < b.condition;
    });
}

KeyBindingEditorBackend::~KeyBindingEditorBackend() = default;

QString KeyBindingEditorBackend::title() const
{
    return _isNewTranslator ? i18n("New Key Binding List") : i18n("Edit Key Binding List");
}

QString KeyBindingEditorBackend::description() const
{
    return _translator->description();
}

void KeyBindingEditorBackend::setDescription(const QString& description)
{
    if (_translator->description() != description) {
        _translator->setDescription(description);
        Q_EMIT descriptionChanged();
    }
}

int KeyBindingEditorBackend::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : _rows.size();
}

QVariant KeyBindingEditorBackend::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const Row& row = _rows.at(index.row());
    switch (role) {
    case ConditionRole:
        return row.condition;
    case ResultRole:
        return row.result;
    default:
        return {};
    }
}

QHash<int, QByteArray> KeyBindingEditorBackend::roleNames() const
{
    return {{ConditionRole, "condition"}, {ResultRole, "result"}};
}

void KeyBindingEditorBackend::setEntry(int row, const QString& condition, const QString& result)
{
    if (row < 0 || row >= _rows.size()) {
        return;
    }
    Row& edited = _rows[row];
    const KeyboardTranslator::Entry entry = KeyboardTranslatorReader::createEntry(condition, result);
    _translator->replaceEntry(edited.entry, entry);
    edited = {entry, condition, result};
    Q_EMIT dataChanged(index(row), index(row));
}

void KeyBindingEditorBackend::addEntry()
{
    beginInsertRows(QModelIndex(), _rows.size(), _rows.size());
    _rows.append({KeyboardTranslator::Entry(), QString(), QString()});
    endInsertRows();
}

void KeyBindingEditorBackend::removeEntries(const QList<int>& rows)
{
    QList<int> sorted = rows;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    for (int row : std::as_const(sorted)) {
        if (row < 0 || row >= _rows.size()) {
            continue;
        }
        _translator->removeEntry(_rows.at(row).entry);
        beginRemoveRows(QModelIndex(), row, row);
        _rows.removeAt(row);
        endRemoveRows();
    }
}

QVariantMap KeyBindingEditorBackend::testKey(int key, int modifiers, const QString& text) const
{
    // The state here is currently set to the state that a newly started
    // terminal in Konsole will be in ( which is also the same as the
    // state just after a reset ), this has 'ANSI' turned on and all other
    // states off.
    const KeyboardTranslator::States states = KeyboardTranslator::AnsiState;
    const auto keyboardModifiers = Qt::KeyboardModifiers(modifiers);
    const KeyboardTranslator::Entry entry = _translator->findEntry(key, keyboardModifiers, states);
    if (!entry.isNull()) {
        return {{QStringLiteral("input"), entry.conditionToString()}, {QStringLiteral("output"), entry.resultToString(true, keyboardModifiers)}};
    }
    return {{QStringLiteral("input"), text}, {QStringLiteral("output"), text}};
}

bool KeyBindingEditorBackend::accept(QQuickItem* from)
{
    auto newTranslator = std::make_unique<KeyboardTranslator>(*_translator);

    if (newTranslator->description().isEmpty()) {
        // KMessageBox::error()'s box, transient for the QML editor (errorWId() would make it
        // transient for a foreign window wrapper instead).
        auto* dialog = new QDialog;
        dialog->setWindowTitle(QApplication::translate("KMessageBox", "Error"));
        dialog->setObjectName(QStringLiteral("error"));
        dialog->setModal(true);
        auto* buttonBox = new QDialogButtonBox(dialog);
        buttonBox->setStandardButtons(QDialogButtonBox::Ok);
        if (from && from->window()) {
            dialog->winId();
            dialog->windowHandle()->setTransientParent(from->window());
        }
        KMessageBox::createKMessageBox(dialog,
            buttonBox,
            QMessageBox::Critical,
            i18n("A key bindings scheme cannot be saved with an empty description."),
            QStringList(),
            QString(),
            nullptr,
            KMessageBox::Notify);
        return false;
    }

    if (_isNewTranslator) {
        newTranslator->setName(newTranslator->description());
    }

    const QString name = newTranslator->name();
    KeyboardTranslatorManager::instance()->addTranslator(newTranslator.release());
    Q_EMIT saved(name, name == _currentProfileTranslator);
    return true;
}

const KeyboardTranslator* KeyBindingEditorBackend::translator() const
{
    return _translator.get();
}
}

#include "moc_KeyBindingEditorBackend.cpp"
