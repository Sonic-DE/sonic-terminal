/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QIdentityProxyModel>
#include <QKeySequence>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

class QWidget;

namespace Konsole {
/**
 * The Profiles settings page's list (ProfileSettings): ProfileModel's rows with its
 * columns as roles, and the page's New, Edit, Remove, Set as Default and shortcut actions.
 */
class KONSOLEPRIVATE_EXPORT ProfileListModel : public QIdentityProxyModel {
    Q_OBJECT
    QML_ELEMENT

public:
    enum Roles {
        NameRole = Qt::UserRole + 100,
        IconNameRole,
        ToolTipRole,
        BoldRole,
        ShortcutRole,
        ShortcutToolTipRole,
        EditableRole,
        DeletableRole,
        DefaultRole,
        // The Shortcut column's text, as the profile tree view shows it.
        ShortcutTextRole,
    };

    explicit ProfileListModel(QObject* parent = nullptr);

    // The widget the Edit Profile dialogs are opened for (the window's compat widget).
    void setOpener(QWidget* opener);

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex& index, int role) const override;

    // The first row whose name starts with text; -1 if none.
    Q_INVOKABLE int rowOf(const QString& text) const;
    // A copy of the row's profile, else of the built-in one, in a new Edit Profile dialog.
    Q_INVOKABLE void createProfile(int row);
    Q_INVOKABLE void editProfile(int row);
    Q_INVOKABLE void deleteProfile(int row);
    Q_INVOKABLE void setDefault(int row);
    Q_INVOKABLE void setShortcut(int row, const QKeySequence& shortcut);
    // The Shortcut column's editor (ShortcutItemDelegate's FilteredKeySequenceEdit): the
    // chords pressed, as key | modifiers values, as a sequence and as its text.
    Q_INVOKABLE void setShortcutChords(int row, const QList<int>& chords);
    Q_INVOKABLE QString chordsText(const QList<int>& chords) const;
    // OK in the Configure dialog.
    Q_INVOKABLE void save();

private:
    QPointer<QWidget> _opener;
};
}
