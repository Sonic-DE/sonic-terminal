/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ProfileListModel.h"

#include "ProfileEditor.h"
#include "profile/ProfileManager.h"
#include "profile/ProfileModel.h"

#include <QFont>
#include <QWidget>

namespace Konsole {
ProfileListModel::ProfileListModel(QObject* parent)
    : QIdentityProxyModel(parent)
{
    setSourceModel(ProfileModel::instance());
    // ProfileModel::setDefault() reports row 0 only, though two rows' names change; a
    // tree view repaints them all anyway, delegates don't.
    connect(sourceModel(), &QAbstractItemModel::dataChanged, this, [this] {
        if (rowCount() > 0) {
            Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0));
        }
    });
}

void ProfileListModel::setOpener(QWidget* opener)
{
    _opener = opener;
}

QHash<int, QByteArray> ProfileListModel::roleNames() const
{
    QHash<int, QByteArray> roles = QIdentityProxyModel::roleNames();
    roles.insert({{NameRole, "name"},
        {IconNameRole, "iconName"},
        // Not "toolTip": the delegates' model data has a final member of that name.
        {ToolTipRole, "profileToolTip"},
        {BoldRole, "bold"},
        {ShortcutRole, "shortcut"},
        {ShortcutToolTipRole, "shortcutToolTip"},
        {EditableRole, "editable"},
        {DeletableRole, "deletable"},
        {DefaultRole, "isDefault"},
        {ShortcutTextRole, "shortcutText"}});
    return roles;
}

QVariant ProfileListModel::data(const QModelIndex& index, int role) const
{
    if (role < NameRole || !index.isValid()) {
        return QIdentityProxyModel::data(index, role);
    }
    const auto source = [this, &index](int column, int sourceRole) {
        return sourceModel()->index(index.row(), column).data(sourceRole);
    };
    const auto profile = source(ProfileModel::PROFILE, ProfileModel::ProfilePtrRole).value<Profile::Ptr>();
    switch (role) {
    case NameRole:
        return source(ProfileModel::NAME, Qt::DisplayRole);
    case IconNameRole:
        return profile ? profile->icon() : QString();
    case ToolTipRole:
        return source(ProfileModel::NAME, Qt::ToolTipRole);
    case BoldRole:
        return source(ProfileModel::NAME, Qt::FontRole).value<QFont>().bold();
    case ShortcutRole:
        return QKeySequence::fromString(source(ProfileModel::SHORTCUT, Qt::DisplayRole).toString());
    case ShortcutToolTipRole:
        return source(ProfileModel::SHORTCUT, Qt::ToolTipRole);
    case ShortcutTextRole:
        return source(ProfileModel::SHORTCUT, Qt::DisplayRole);
    case EditableRole:
        return profile && profile->isEditable();
    case DeletableRole:
        return profile && profile->isDeletable();
    case DefaultRole:
        return profile && profile == ProfileManager::instance()->defaultProfile();
    }
    return QVariant();
}

int ProfileListModel::rowOf(const QString& text) const
{
    const QModelIndexList found = match(index(0, 0), Qt::DisplayRole, text, 1, Qt::MatchStartsWith);
    return found.isEmpty() ? -1 : found.first().row();
}

namespace {
    Profile::Ptr profileAt(const QAbstractItemModel* model, int row)
    {
        return model->index(row, ProfileModel::PROFILE).data(ProfileModel::ProfilePtrRole).value<Profile::Ptr>();
    }
}

void ProfileListModel::createProfile(int row)
{
    auto newProfile = Profile::Ptr(new Profile(ProfileManager::instance()->builtinProfile()));
    if (const auto profile = profileAt(sourceModel(), row)) {
        newProfile->clone(profile, true);
    }
    const QString uniqueName = ProfileManager::instance()->generateUniqueName();
    newProfile->setProperty(Profile::Name, uniqueName);
    newProfile->setProperty(Profile::UntranslatedName, uniqueName);

    if (QQuickWindow* dialog = ProfileEditor::open(newProfile, ProfileEditor::NewProfile, _opener)) {
        QMetaObject::invokeMethod(dialog, "selectProfileName");
    }
}

void ProfileListModel::editProfile(int row)
{
    const auto profile = profileAt(sourceModel(), row);
    if (!(profile && profile->isEditable())) {
        return;
    }
    ProfileEditor::open(profile, ProfileEditor::ExistingProfile, _opener);
}

void ProfileListModel::deleteProfile(int row)
{
    const auto profile = profileAt(sourceModel(), row);
    if (profile && profile != ProfileManager::instance()->defaultProfile()) {
        ProfileManager::instance()->deleteProfile(profile);
    }
}

void ProfileListModel::setDefault(int row)
{
    if (const auto profile = profileAt(sourceModel(), row)) {
        ProfileManager::instance()->setDefaultProfile(profile);
    }
}

void ProfileListModel::setShortcut(int row, const QKeySequence& shortcut)
{
    sourceModel()->setData(sourceModel()->index(row, ProfileModel::SHORTCUT), shortcut.toString(), Qt::DisplayRole);
}

namespace {
    QKeySequence sequenceOf(const QList<int>& chords)
    {
        // QKeySequence holds four chords at most, as QKeySequenceEdit records them; an unused
        // QKeyCombination would be an empty chord.
        const auto chord = [&chords](int i) {
            return QKeyCombination::fromCombined(chords.at(i));
        };
        switch (chords.size()) {
        case 0:
            return {};
        case 1:
            return QKeySequence(chord(0));
        case 2:
            return QKeySequence(chord(0), chord(1));
        case 3:
            return QKeySequence(chord(0), chord(1), chord(2));
        default:
            return QKeySequence(chord(0), chord(1), chord(2), chord(3));
        }
    }
}

void ProfileListModel::setShortcutChords(int row, const QList<int>& chords)
{
    setShortcut(row, sequenceOf(chords));
}

QString ProfileListModel::chordsText(const QList<int>& chords) const
{
    return sequenceOf(chords).toString(QKeySequence::NativeText);
}

void ProfileListModel::save()
{
    ProfileManager::instance()->saveSettings();
}
}

#include "moc_ProfileListModel.cpp"
