/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"
#include "profile/Profile.h"

#include <QKeySequence>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQuickItem;
class QQuickWindow;
class QWidget;

namespace Konsole {
/**
 * The Edit Profile dialog's edits of a profile (EditProfileDialog without widgets): changes
 * collect in a temporary profile and reach the profile on Apply or OK; previewed ones show
 * at once and are undone on Cancel. Properties go by Profile property name.
 */
class KONSOLEPRIVATE_EXPORT ProfileEditor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created with the dialog by ProfileEditor::open()")
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(bool applyEnabled READ applyEnabled NOTIFY changed)
    Q_PROPERTY(bool setAsDefault READ setAsDefault WRITE setSetAsDefault NOTIFY changed)
    Q_PROPERTY(QString defaultProfileText READ defaultProfileText CONSTANT)
    Q_PROPERTY(bool nameEditable READ nameEditable CONSTANT)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QVariantList containers READ containers CONSTANT)
    Q_PROPERTY(bool containersEnabled READ containersEnabled CONSTANT)
    Q_PROPERTY(QString containersDisabledReason READ containersDisabledReason CONSTANT)
    Q_PROPERTY(QVariantList textEditors READ textEditors CONSTANT)
    Q_PROPERTY(QString homeDirectory READ homeDirectory CONSTANT)

public:
    enum State {
        ExistingProfile,
        NewProfile,
    };
    Q_ENUM(State)

    ProfileEditor(const Profile::Ptr& profile, State state, QObject* parent = nullptr);

    // The QML Edit Profile dialog for profile, transient for opener's window.
    static QQuickWindow* open(const Profile::Ptr& profile, State state, QWidget* opener);

    Profile::Ptr profile() const;

    QString title() const;
    bool applyEnabled() const;
    bool setAsDefault() const;
    void setSetAsDefault(bool isDefault);
    QString defaultProfileText() const;
    // False for a group of profiles: the name field shows their names.
    bool nameEditable() const;
    QString message() const;
    // "Always start in container": {text, iconName, key}, "None" first.
    QVariantList containers() const;
    bool containersEnabled() const;
    QString containersDisabledReason() const;
    // The text editor choices in Enum::TextEditorCmd order: {text, iconName, enabled}.
    QVariantList textEditors() const;
    QString homeDirectory() const;

    // The shown value: the edited one, else the profile's.
    Q_INVOKABLE QVariant value(const QString& name) const;
    // An edit; with preview, the profile shows it until Cancel.
    Q_INVOKABLE void setValue(const QString& name, const QVariant& value, bool preview = false);
    // A key sequence property, as QKeySequenceEdit shows it.
    Q_INVOKABLE QKeySequence keySequence(const QString& name) const;
    // The profile names shown for a group.
    Q_INVOKABLE QString groupNames() const;
    // The command with its arguments, as the Command field shows and edits it.
    Q_INVOKABLE QString commandLine() const;
    Q_INVOKABLE void setCommandLine(const QString& command);

    // Apply: stores the edits once the name is valid.
    Q_INVOKABLE bool apply();
    // OK: as Apply, then the previews stay; false keeps the dialog open.
    Q_INVOKABLE bool accept();
    // Cancel: the previews undone.
    Q_INVOKABLE void reject();

    // The dialogs the General, Mouse and Advanced pages open, transient for from's window.
    Q_INVOKABLE void chooseIcon(QQuickItem* from);
    Q_INVOKABLE void chooseInitialDirectory(QQuickItem* from);
    Q_INVOKABLE void editEnvironment(QQuickItem* from);
    Q_INVOKABLE void editCustomTextEditor(QQuickItem* from);
    // The encoding menu below its button.
    Q_INVOKABLE void chooseEncoding(QQuickItem* below);

Q_SIGNALS:
    void changed();
    void messageChanged();
    // A refused name was set back to the profile's: the field shows it, selected.
    void nameReverted();

private:
    Profile::Property property(const QString& name) const;
    void updateTempProfileProperty(Profile::Property property, const QVariant& value);
    void updateButtonApply();
    void preview(Profile::Property property, const QVariant& value);
    void unpreviewAll();
    void save();
    bool isProfileNameValid();
    void refuseName(const QString& message);
    void setMessage(const QString& message);

    Profile::Ptr _profile;
    Profile::Ptr _tempProfile;
    Profile::PropertyMap _previewedProperties;
    State _state;
    bool _isDefault = false;
    bool _setAsDefault = false;
    bool _applyEnabled = false;
    QString _message;
};
}
