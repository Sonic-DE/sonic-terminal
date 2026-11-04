/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QAbstractListModel>
#include <QKeySequence>
#include <QPointer>
#include <QSize>
#include <QtQml/qqmlregistration.h>

#include <optional>

class KActionCollection;
class QAction;
class QQuickItem;

namespace Konsole {
/**
 * Configure Keyboard Shortcuts (KShortcutsDialog without widgets) for a window's action
 * collections: KShortcutsEditor's tree as a flat list (top items, categories, actions with
 * their depth), the search line, edits applied to the actions at once and undone on Cancel,
 * KKeySequenceWidget's conflict checks, Defaults, Print and the shortcut schemes.
 */
class KONSOLEPRIVATE_EXPORT ShortcutsEditorModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by MainWindow::showShortcutsDialog()")
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QStringList schemes READ schemes NOTIFY schemesChanged)
    Q_PROPERTY(QString currentScheme READ currentScheme NOTIFY schemesChanged)

public:
    enum Kind {
        TopItem,
        CategoryItem,
        ActionItem,
    };
    Q_ENUM(Kind)
    // The shortcut columns, as KShortcutsEditor's LocalPrimary and LocalAlternate.
    enum Column {
        Primary = 1,
        Alternate = 2,
    };
    Q_ENUM(Column)
    enum Roles {
        KindRole = Qt::UserRole + 1,
        DepthRole,
        NameRole,
        IconNameRole,
        PrimaryRole,
        AlternateRole,
        DefaultPrimaryRole,
        DefaultAlternateRole,
        // QKeySequences, for the recorder.
        PrimarySequenceRole,
        AlternateSequenceRole,
        DefaultPrimarySequenceRole,
        DefaultAlternateSequenceRole,
        ShownRole,
        IdRole,
        // Shown, and no item above it collapsed.
        VisibleRole,
        HasChildrenRole,
        CollapsedRole,
    };

    ShortcutsEditorModel(const QList<KActionCollection*>& collections, QObject* parent = nullptr);
    ~ShortcutsEditorModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const;
    void setFilterText(const QString& text);
    QStringList schemes() const;
    QString currentScheme() const;
    QList<KActionCollection*> collections() const;

    // A recorded shortcut (empty for none) for a row's column, after the conflict checks
    // (message boxes transient for from's window); false if refused.
    Q_INVOKABLE bool setShortcut(int row, int column, const QKeySequence& sequence, QQuickItem* from);
    Q_INVOKABLE bool isModified() const;
    // A top or category item's branch, as the tree's arrow.
    Q_INVOKABLE void toggleCollapsed(int row);
    // The texts of a column (0 name, 1, 2) for its width (QHeaderView::ResizeToContents),
    // and each row's depth.
    Q_INVOKABLE QStringList columnTexts(int column) const;
    Q_INVOKABLE QList<int> depths() const;
    // Defaults: every action back to its defaults.
    Q_INVOKABLE void allDefault();
    // Cancel: every changed action back to what it had.
    Q_INVOKABLE void undo();
    // OK: keeps the changes and saves them (ShortcutStore).
    Q_INVOKABLE void save();
    Q_INVOKABLE void print(QQuickItem* from);

    Q_INVOKABLE void changeScheme(const QString& scheme, QQuickItem* from);
    Q_INVOKABLE void newScheme(QQuickItem* from);
    Q_INVOKABLE void deleteScheme(QQuickItem* from);
    Q_INVOKABLE void saveToScheme(QQuickItem* from);
    Q_INVOKABLE void exportScheme(QQuickItem* from);
    Q_INVOKABLE void importScheme(QQuickItem* from);

    // [KShortcutsDialog Settings] Dialog Size.
    Q_INVOKABLE static QSize dialogSize();
    Q_INVOKABLE static void saveDialogSize(const QSize& size);

Q_SIGNALS:
    void filterTextChanged();
    void schemesChanged();

private:
    struct Row {
        Kind kind = ActionItem;
        int depth = 0;
        QString name;
        QPointer<QAction> action;
        bool shown = true;
        bool collapsed = false;
        // The action's shortcuts before the first change, for undo().
        std::optional<QList<QKeySequence>> old;
    };

    void build();
    void filter();
    void setKeySequence(int row, int column, const QKeySequence& sequence);
    void rowChanged(int row);
    bool conflictWithLocalShortcuts(const QKeySequence& sequence, QQuickItem* from);
    bool conflictWithGlobalShortcuts(const QKeySequence& sequence, QQuickItem* from);
    void stealShortcut(const QKeySequence& sequence, QAction* action);
    void refreshSchemes();
    QStringList rowTexts(const Row& row) const;
    bool visible(int row) const;

    QList<QPointer<KActionCollection>> _collections;
    QList<Row> _rows;
    QString _filterText;
    QStringList _schemes;
    QString _currentScheme;
};
}
