/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "keyboardtranslator/KeyboardTranslator.h"
#include "konsoleprivate_export.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QQuickItem;

namespace Konsole {
/**
 * The Key Binding Editor's edits (KeyBindingEditor without widgets) of a copy of a key
 * bindings scheme: its description and entries, one row per entry with its key combination
 * and output, and the test area. accept() saves the copy.
 */
class KONSOLEPRIVATE_EXPORT KeyBindingEditorBackend : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by ProfileEditor::editKeyBindings()")
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)

public:
    enum Roles {
        ConditionRole = Qt::UserRole + 1,
        ResultRole,
    };

    KeyBindingEditorBackend(const KeyboardTranslator* translator, const QString& currentProfileTranslator, bool isNewTranslator, QObject* parent = nullptr);
    ~KeyBindingEditorBackend() override;

    QString title() const;
    QString description() const;
    void setDescription(const QString& description);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // A row's key combination and output as edited (bindingTableItemChanged()).
    Q_INVOKABLE void setEntry(int row, const QString& condition, const QString& result);
    // An empty row at the end.
    Q_INVOKABLE void addEntry();
    Q_INVOKABLE void removeEntries(const QList<int>& rows);
    // The test area: a key press as {input, output}.
    Q_INVOKABLE QVariantMap testKey(int key, int modifiers, const QString& text) const;
    // OK: refuses an empty description with a message box transient for from's window,
    // else saves the scheme.
    Q_INVOKABLE bool accept(QQuickItem* from);

    const KeyboardTranslator* translator() const;

Q_SIGNALS:
    void descriptionChanged();
    // The scheme was saved under name; it is the profile's own when isProfileTranslator.
    void saved(const QString& name, bool isProfileTranslator);

private:
    struct Row {
        KeyboardTranslator::Entry entry;
        QString condition;
        QString result;
    };

    std::unique_ptr<KeyboardTranslator> _translator;
    QString _currentProfileTranslator;
    bool _isNewTranslator;
    QList<Row> _rows;
};
}
