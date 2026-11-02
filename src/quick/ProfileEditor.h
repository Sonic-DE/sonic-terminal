/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"
#include "profile/Profile.h"

#include <QFont>
#include <QKeySequence>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQuickItem;
class QWidget;

namespace KNSWidgets {
class Dialog;
}
namespace KNSCore {
class Entry;
}

namespace Konsole {
class ColorScheme;
class FontDialog;

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
    Q_PROPERTY(QVariantList colorSchemes READ colorSchemes NOTIFY colorSchemesChanged)
    Q_PROPERTY(QString transparencyWarning READ transparencyWarning NOTIFY changed)
    Q_PROPERTY(QString colorSchemeMessage READ colorSchemeMessage NOTIFY colorSchemeMessageChanged)
    Q_PROPERTY(QFont previewFont READ previewFont NOTIFY changed)
    Q_PROPERTY(QString fontText READ fontText NOTIFY changed)
    Q_PROPERTY(QString emojiFontText READ emojiFontText NOTIFY changed)
    Q_PROPERTY(QVariantList keyBindings READ keyBindings NOTIFY keyBindingsChanged)

public:
    enum State {
        ExistingProfile,
        NewProfile,
    };
    Q_ENUM(State)

    ProfileEditor(const Profile::Ptr& profile, State state, QObject* parent = nullptr);

    // The QML Edit Profile dialog for profile, transient for opener's window.
    static QQuickWindow* open(const Profile::Ptr& profile, State state, QWidget* opener, Qt::WindowModality modality = Qt::ApplicationModal);

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
    // The colour schemes by description: {name, description, background, foreground}.
    QVariantList colorSchemes() const;
    // Why the chosen scheme's transparency won't show; empty if it will.
    QString transparencyWarning() const;
    // A Get New scheme that failed to load, for a few seconds.
    QString colorSchemeMessage() const;
    // The font as the preview shows it (the profile's, with its smoothing).
    QFont previewFont() const;
    QString fontText() const;
    QString emojiFontText() const;
    // The key bindings schemes by description: {name, description, path}.
    QVariantList keyBindings() const;

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

    // The Appearance page's colour scheme list: choosing one previews it.
    Q_INVOKABLE void selectColorScheme(const QString& name);
    Q_INVOKABLE bool canResetColorScheme(const QString& name) const;
    Q_INVOKABLE bool isColorSchemeDeletable(const QString& name) const;
    Q_INVOKABLE void removeColorScheme(const QString& name);
    Q_INVOKABLE void resetColorScheme(const QString& name);
    // The QML Colour Scheme Editor on name (the default scheme if empty), one at a time.
    Q_INVOKABLE void editColorScheme(const QString& name, bool isNewScheme, QQuickItem* from);
    // KNewStuff's dialog for colour schemes.
    Q_INVOKABLE void getNewColorSchemes(QQuickItem* from);
    // FontDialog, previewing the font until it is accepted or cancelled.
    Q_INVOKABLE void chooseFont(QQuickItem* from);
    Q_INVOKABLE void chooseEmojiFont(QQuickItem* from);

    // The Keyboard page's list.
    Q_INVOKABLE void selectKeyBindings(const QString& name);
    Q_INVOKABLE bool canResetKeyBindings(const QString& name) const;
    Q_INVOKABLE bool isKeyBindingsDeletable(const QString& name) const;
    Q_INVOKABLE void removeKeyBindings(const QString& name);
    Q_INVOKABLE void resetKeyBindings(const QString& name);
    // The QML Key Binding Editor on name (the default scheme if empty).
    Q_INVOKABLE void editKeyBindings(const QString& name, bool isNewTranslator, QQuickItem* from);

    // The Colour Scheme Editor's OK and Apply: stores the scheme and chooses it.
    void saveColorScheme(const ColorScheme& scheme, bool isNewScheme);

Q_SIGNALS:
    void changed();
    void messageChanged();
    void colorSchemesChanged();
    void colorSchemeMessageChanged();
    void keyBindingsChanged();
    // The list selects name (after an edit, a reset or a removal).
    void colorSchemeChosen(const QString& name);
    void keyBindingsChosen(const QString& name);
    // A refused name was set back to the profile's: the field shows it, selected.
    void nameReverted();

private:
    Profile::Property property(const QString& name) const;
    void updateTempProfileProperty(Profile::Property property, const QVariant& value);
    void updateButtonApply();
    void preview(Profile::Property property, const QVariant& value);
    void unpreview(Profile::Property property);
    void unpreviewAll();
    void showFontDialog(bool emoji, QQuickItem* from);
    void gotNewColorSchemes(const QList<KNSCore::Entry>& changedEntries);
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
    QString _colorSchemeMessage;
    QPointer<QQuickWindow> _colorSchemeEditor;
    QPointer<KNSWidgets::Dialog> _getNewDialog;
    QPointer<FontDialog> _fontDialog;
    QPointer<FontDialog> _emojiFontDialog;
};
}
