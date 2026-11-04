/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ProfileEditor.h"

#include "ColorSchemeEditorBackend.h"
#include "Enumeration.h"
#include "FontDialog.h"
#include "FrameworkDialogScope.h"
#include "KeyBindingEditorBackend.h"
#include "QmlDialogs.h"
#include "ShellCommand.h"
#include "WindowSystemInfo.h"
#include "colorscheme/ColorSchemeManager.h"
#include "containers/ContainerRegistry.h"
#include "keyboardtranslator/KeyboardTranslatorManager.h"
#include "profile/ProfileGroup.h"
#include "profile/ProfileManager.h"

#include <KCodecAction>
#include <KIconDialog>
#include <KLocalizedString>
#include <KNSCore/Entry>
#include <KNSWidgets/Dialog>
#include <Sonic/Logging/Log>

#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

#if defined(Q_OS_LINUX) | defined(Q_OS_FREEBSD) | defined(Q_OS_OPENBSD) | defined(Q_OS_SOLARIS) | defined(Q_OS_MACOS)
#include <unistd.h>
#endif

namespace Konsole {
namespace {
    QString groupProfileNames(const ProfileGroup::Ptr& group, int maxLength)
    {
        QString caption;
        const int count = group->profiles().count();
        for (int i = 0; i < count; i++) {
            caption += group->profiles()[i]->name();
            if (i < (count - 1)) {
                caption += QLatin1Char(',');
                // limit caption length to prevent very long window titles
                if (maxLength > 0 && caption.length() > maxLength) {
                    caption += QLatin1String("...");
                    break;
                }
            }
        }
        return caption;
    }

    bool isGroup(const Profile::Ptr& profile)
    {
        const ProfileGroup::Ptr group = profile->asGroup();
        return group && group->profiles().count() > 1;
    }

    // A dialog of a framework kept as a widget, transient for the QML dialog it is opened from.
    void setTransientFor(QWidget* dialog, QQuickItem* from)
    {
        if (from && from->window()) {
            dialog->winId();
            dialog->windowHandle()->setTransientParent(from->window());
        }
    }
}

ProfileEditor::ProfileEditor(const Profile::Ptr& profile, State state, QObject* parent)
    : QObject(parent)
    , _profile(profile)
    , _tempProfile(new Profile{})
    , _state(state)
{
    Q_ASSERT(profile);
    _tempProfile->setHidden(true);
    _isDefault = profile == ProfileManager::instance()->defaultProfile();
    _setAsDefault = _isDefault;
    // A new profile can't be applied before an edit (EditProfileDialog::updateCaption()).
    if (state == ExistingProfile) {
        updateButtonApply();
    }
}

QQuickWindow* ProfileEditor::open(const Profile::Ptr& profile, State state, QWidget* opener, Qt::WindowModality modality)
{
    auto* editor = new ProfileEditor(profile, state);
    QQuickWindow* dialog = QmlDialogs::open(QStringLiteral("EditProfileDialog"), opener, {{QStringLiteral("editor"), QVariant::fromValue(editor)}}, modality);
    if (!dialog) {
        delete editor;
        return nullptr;
    }
    editor->setParent(dialog);
    return dialog;
}

Profile::Ptr ProfileEditor::profile() const
{
    return _profile;
}

QString ProfileEditor::title() const
{
    const int maxGroupCaptionLength = 25;
    if (isGroup(_profile)) {
        const ProfileGroup::Ptr group = _profile->asGroup();
        return i18np("Editing profile: %2", "Editing %1 profiles: %2", group->profiles().count(), groupProfileNames(group, maxGroupCaptionLength));
    }
    if (_state == NewProfile) {
        return i18n("Create New Profile");
    }
    return i18n("Edit Profile \"%1\"", value(QStringLiteral("Name")).toString());
}

bool ProfileEditor::applyEnabled() const
{
    return _applyEnabled;
}

bool ProfileEditor::setAsDefault() const
{
    return _setAsDefault;
}

void ProfileEditor::setSetAsDefault(bool isDefault)
{
    if (_setAsDefault == isDefault) {
        return;
    }
    _setAsDefault = isDefault;
    updateButtonApply();
}

QString ProfileEditor::defaultProfileText() const
{
    QString appName = QCoreApplication::applicationName();
    if (!appName.isEmpty() && appName != QLatin1String("konsole")) {
        appName[0] = appName.at(0).toUpper();
        return i18n("Default profile for new terminal sessions in %1", appName);
    }
    return i18n("Default profile");
}

bool ProfileEditor::nameEditable() const
{
    return !isGroup(_profile);
}

QString ProfileEditor::message() const
{
    return _message;
}

QVariantList ProfileEditor::containers() const
{
    QVariantList choices{QVariantMap{{QStringLiteral("text"), i18n("None")}, {QStringLiteral("iconName"), QString()}, {QStringLiteral("key"), QString()}}};
    const QList<ContainerInfo> found = ContainerRegistry::instance()->cachedContainers();
    for (const auto& container : found) {
        choices.append(QVariantMap{{QStringLiteral("text"), QStringLiteral("%1: %2").arg(container.detector->displayName(), container.displayName)},
            {QStringLiteral("iconName"), container.iconName},
            {QStringLiteral("key"), ContainerRegistry::keyFromContainerInfo(container)}});
    }
    return choices;
}

bool ProfileEditor::containersEnabled() const
{
    return ContainerRegistry::instance()->isEnabled();
}

QString ProfileEditor::containersDisabledReason() const
{
    return ContainerRegistry::instance()->disabledReason();
}

QVariantList ProfileEditor::textEditors() const
{
    static const Enum::TextEditorCmd editorsList[] = {Enum::Kate, Enum::KWrite, Enum::KDevelop, Enum::QtCreator, Enum::Gedit, Enum::gVim, Enum::CustomTextEditor, Enum::Default};

    QVariantList choices;
    for (auto editor : editorsList) {
        QString exec;
        QString displayName;
        QString iconName;
        switch (editor) {
        case Enum::Kate:
            exec = QStringLiteral("kate");
            displayName = QStringLiteral("Kate");
            iconName = exec;
            break;
        case Enum::KWrite:
            exec = QStringLiteral("kwrite");
            displayName = QStringLiteral("KWrite");
            iconName = exec;
            break;
        case Enum::KDevelop:
            exec = QStringLiteral("kdevelop");
            displayName = QStringLiteral("KDevelop");
            iconName = exec;
            break;
        case Enum::QtCreator:
            exec = QStringLiteral("qtcreator");
            displayName = QStringLiteral("Qt Creator");
            iconName = exec;
            break;
        case Enum::Gedit:
            exec = QStringLiteral("gedit");
            displayName = QStringLiteral("Gedit");
            break;
        case Enum::gVim:
            exec = QStringLiteral("gvim");
            displayName = QStringLiteral("gVim");
            iconName = exec;
            break;
        case Enum::CustomTextEditor:
            displayName = QStringLiteral("Custom");
            iconName = QStringLiteral("system-run");
            break;
        case Enum::Default:
            displayName = i18n("Default for file type");
            iconName = QStringLiteral("preferences-desktop-default-applications");
            break;
        default:
            break;
        }
        // For "CustomTextEditor" we don't check if the binary exists
        const bool isAvailable = editor == Enum::CustomTextEditor || editor == Enum::Default || !QStandardPaths::findExecutable(exec).isEmpty();
        choices.append(QVariantMap{{QStringLiteral("text"), displayName}, {QStringLiteral("iconName"), iconName}, {QStringLiteral("enabled"), isAvailable}});
    }
    return choices;
}

QString ProfileEditor::homeDirectory() const
{
    return QStandardPaths::standardLocations(QStandardPaths::HomeLocation).value(0);
}

Profile::Property ProfileEditor::property(const QString& name) const
{
    const Profile::Property found = Profile::lookupByName(name);
    // An unknown name gives the first enum value (Path), which no page edits.
    if (found == Profile::Path && name.compare(QLatin1String("Path"), Qt::CaseInsensitive) != 0) {
        sonicWarning() << "No profile property" << name;
    }
    return found;
}

QVariant ProfileEditor::value(const QString& name) const
{
    const Profile::Property p = property(name);
    return _tempProfile->isPropertySet(p) ? _tempProfile->property<QVariant>(p) : _profile->property<QVariant>(p);
}

void ProfileEditor::setValue(const QString& name, const QVariant& value, bool preview)
{
    const Profile::Property p = property(name);
    QVariant stored = value;
    // From QML: a JS array of strings, a key sequence.
    if (value.typeId() == QMetaType::QVariantList) {
        stored = value.toStringList();
    } else if (value.typeId() == QMetaType::QKeySequence) {
        stored = value.value<QKeySequence>().toString();
    }
    if (preview) {
        this->preview(p, stored);
    }
    updateTempProfileProperty(p, stored);
    if (p == Profile::Name) {
        updateTempProfileProperty(Profile::UntranslatedName, stored);
        // A new profile's name doesn't enable Apply (EditProfileDialog::updateCaption()).
        if (_state == NewProfile) {
            _applyEnabled = false;
            Q_EMIT changed();
        }
    }
}

QKeySequence ProfileEditor::keySequence(const QString& name) const
{
    return QKeySequence::fromString(value(name).toString());
}

QString ProfileEditor::groupNames() const
{
    const ProfileGroup::Ptr group = _profile->asGroup();
    return group ? groupProfileNames(group, -1) : _profile->name();
}

QString ProfileEditor::commandLine() const
{
    return ShellCommand(value(QStringLiteral("Command")).toString(), value(QStringLiteral("Arguments")).toStringList()).fullCommand();
}

void ProfileEditor::setCommandLine(const QString& command)
{
    ShellCommand shellCommand(command);

    updateTempProfileProperty(Profile::Command, shellCommand.command());
    updateTempProfileProperty(Profile::Arguments, shellCommand.arguments());
}

void ProfileEditor::updateTempProfileProperty(Profile::Property property, const QVariant& value)
{
    _tempProfile->setProperty(property, value);
    updateButtonApply();
}

void ProfileEditor::updateButtonApply()
{
    auto filterFunc = [this](const auto& keyValue) {
        const auto& [property, value] = keyValue;
        // for previewed property
        auto it = _previewedProperties.find(property);
        if (it != _previewedProperties.cend()) {
            if (value != it->second) {
                return true;
            }
            // for not-previewed property
            //
            // for the Profile::KeyBindings property, if it's set in the _tempProfile
            // then the user opened the edit key bindings dialog and clicked
            // OK, and could have add/removed a key bindings rule
        } else if (property == Profile::KeyBindings //
            || value != _profile->property<QVariant>(property)) {
            return true;
        }

        return false;
    };

    const Profile::PropertyMap& map = _tempProfile->properties();
    _applyEnabled = std::any_of(map.cbegin(), map.cend(), filterFunc) || _setAsDefault != _isDefault;
    Q_EMIT changed();
}

void ProfileEditor::preview(Profile::Property property, const QVariant& value)
{
    // skip previews for profile groups if the profiles in the group
    // have conflicting original values for the property
    if (isGroup(_profile) && _profile->property<QVariant>(property).isNull()) {
        return;
    }

    _previewedProperties.insert({property, _profile->property<QVariant>(property)});
    ProfileManager::instance()->changeProfile(_profile, {{property, value}}, false);
}

void ProfileEditor::unpreviewAll()
{
    if (!_previewedProperties.empty()) {
        ProfileManager::instance()->changeProfile(_profile, _previewedProperties, false);
    }
}

void ProfileEditor::save()
{
    const bool isNewProfile = _state == NewProfile;

    if (isNewProfile) {
        ProfileManager::instance()->addProfile(_profile);
        // The profile exists now, no longer treat it as initial creation
        _state = ExistingProfile;
    }

    const bool defaultChanged = _isDefault != _setAsDefault;

    if (_tempProfile->isEmpty() && !defaultChanged) {
        if (isNewProfile) {
            // New profile, we need to save it to disk, even if no settings
            // were changed and _tempProfile is empty
            ProfileManager::instance()->changeProfile(_profile, _profile->properties());
        }
        Q_EMIT changed();
        return;
    }

    ProfileManager::instance()->changeProfile(_profile, _tempProfile->properties());

    // ensure that these settings are not undone by a call
    // to unpreview()
    _previewedProperties.clear();

    if (defaultChanged) {
        Q_ASSERT(_profile != ProfileManager::instance()->builtinProfile());
        const Profile::Ptr newDefault = _setAsDefault ? _profile : ProfileManager::instance()->builtinProfile();
        ProfileManager::instance()->setDefaultProfile(newDefault);
        _isDefault = _setAsDefault;
    }

    _tempProfile->clear();
    _tempProfile->setHidden(true);
    _applyEnabled = false;
    Q_EMIT changed();
}

bool ProfileEditor::apply()
{
    if (!isProfileNameValid()) {
        return false;
    }
    save();
    return true;
}

bool ProfileEditor::accept()
{
    if (!apply()) {
        return false;
    }
    unpreviewAll();
    return true;
}

void ProfileEditor::reject()
{
    unpreviewAll();
}

void ProfileEditor::setMessage(const QString& message)
{
    _message = message;
    Q_EMIT messageChanged();
}

void ProfileEditor::refuseName(const QString& message)
{
    setMessage(message);
    // Revert the name in the dialog
    setValue(QStringLiteral("Name"), _profile->name());
    Q_EMIT nameReverted();
}

bool ProfileEditor::isProfileNameValid()
{
    // check whether the user has enough permissions to save the profile
    QFileInfo fileInfo(_profile->path());
    if (fileInfo.exists() && !fileInfo.isWritable() && (!_tempProfile->isPropertySet(Profile::Name) || _tempProfile->name() == _profile->name())) {
        setMessage(xi18nc("@info",
            "Insufficient permissions to save settings to: <filename>%1</filename>.<nl/>"
            "Either change the permissions of that file or set a different name to save "
            "the settings to a new profile.",
            _profile->path()));
        return false;
    }

    const QList<Profile::Ptr> existingProfiles = ProfileManager::instance()->allProfiles();
    QStringList otherExistingProfileNames;

    for (const Profile::Ptr& existingProfile : existingProfiles) {
        if (existingProfile->name() != _profile->name()) {
            otherExistingProfileNames.append(existingProfile->name());
        }
    }

    if ((_tempProfile->isPropertySet(Profile::Name) && _tempProfile->name().isEmpty()) || (_profile->name().isEmpty() && _tempProfile->name().isEmpty())) {
        refuseName(i18nc("@info", "Profile Name was empty; please set a name to be able to save settings."));
        return false;
    }

    if (!_tempProfile->name().isEmpty() && otherExistingProfileNames.contains(_tempProfile->name())) {
        refuseName(i18nc("@info", "A profile with the name \"%1\" already exists.", _tempProfile->name()));
        return false;
    }

    const QString profileStoragePath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + QStringLiteral("konsole"));
    int nameLenMax = 256;

#if defined(Q_OS_LINUX) | defined(Q_OS_OPENBSD) | defined(Q_OS_FREEBSD) | defined(Q_OS_MACOS) | defined(Q_OS_SOLARIS)
    const int maxPath = pathconf(profileStoragePath.toLocal8Bit().constData(), _PC_PATH_MAX);
    const int nameLenMaxPath = maxPath - _profile->path().lastIndexOf(QLatin1Char('/')) + 1;
    const int nameLenMaxName = pathconf(profileStoragePath.toLocal8Bit().constData(), _PC_NAME_MAX);
    nameLenMax = qMin(nameLenMaxPath, nameLenMaxName);
#elif defined(Q_OS_WIN)
    // If MAX_PATH not found, use default (260 - 1)
    const int maxPath = qMax(259, qEnvironmentVariable("MAX_PATH").toInt());
    nameLenMax = maxPath - _profile->path().lastIndexOf(QLatin1Char('/')) + 1;
#endif

    nameLenMax -= QStringLiteral(".profile").size();

    // doesn't work on macOS and maybe other systems so make sure we have a sane value always
    // See https://mail.kde.org/pipermail/kwrite-devel/2024-April/007139.html
    if (nameLenMax < 0) {
        nameLenMax = 100;
    }

    if (_tempProfile->name().size() > nameLenMax) {
        refuseName(i18nc("@info", "Profile name exceeded maximum allowed length (%1).", nameLenMax));
        return false;
    }

    if (!_tempProfile->name().isEmpty()) {
        static const QString illegalCharacters(QStringLiteral("#%&{}/\\\"<>*?$!':@`+=|"));
        static const QString colorTagOpen(QStringLiteral("<font color=\"red\">"));
        static const QString colorTagClose(QStringLiteral("</font>"));
        const QString name(_tempProfile->name());
        bool hasIllegal = false;
        bool unclosedTag = false;
        QString highlightedName;

        for (int i = 0; i < name.size(); ++i) {
            const auto currChar = name.at(i);

            if (illegalCharacters.contains(currChar)) {
                if (!unclosedTag) {
                    highlightedName += colorTagOpen;
                }
                hasIllegal = true;
                unclosedTag = true;
            } else if (unclosedTag) {
                unclosedTag = false;
                highlightedName += colorTagClose;
            }

            highlightedName += currChar;

            if (i + 1 == name.size() && unclosedTag) {
                highlightedName += colorTagClose;
            }
        }

        if (hasIllegal) {
            refuseName(i18nc("@info", "The highlighted characters are invalid in profile names: %1.", highlightedName));
            return false;
        }
    }

    // Valid name
    return true;
}

void ProfileEditor::chooseIcon(QQuickItem* from)
{
    FrameworkDialogScope scope(QStringLiteral("profile icon"));
    KIconDialog dialog;
    dialog.setup(KIconLoader::Desktop, KIconLoader::Application, false, 0, false);
    setTransientFor(&dialog, from);
    const QString icon = dialog.openDialog();
    if (!icon.isEmpty()) {
        setValue(QStringLiteral("Icon"), icon);
    }
}

void ProfileEditor::chooseInitialDirectory(QQuickItem* from)
{
    // QFileDialog::getExistingDirectoryUrl(), transient for the QML dialog.
    FrameworkDialogScope scope(QStringLiteral("initial directory"));
    QFileDialog dialog(nullptr, i18n("Select Initial Directory"));
    dialog.setDirectoryUrl(QUrl::fromUserInput(value(QStringLiteral("Directory")).toString()));
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly);
    setTransientFor(&dialog, from);
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedUrls().isEmpty()) {
        setValue(QStringLiteral("Directory"), dialog.selectedUrls().constFirst().path());
    }
}

void ProfileEditor::editEnvironment(QQuickItem* from)
{
    QQuickWindow* dialog = QmlDialogs::openFrom(QStringLiteral("TextInputDialog"),
        from,
        {{QStringLiteral("title"), i18n("Edit Environment")},
            {QStringLiteral("labelText"), i18n("One environment variable per line")},
            {QStringLiteral("multiLine"), true},
            {QStringLiteral("textValue"), value(QStringLiteral("Environment")).toStringList().join(QStringLiteral("\n"))}});
    if (!dialog) {
        return;
    }
    QmlDialogs::onAccepted(dialog, this, [this, dialog]() {
        const QString text = dialog->property("textValue").toString();
        // the user could have removed all entries so we store an empty list
        updateTempProfileProperty(Profile::Environment, text.isEmpty() ? QStringList() : text.split(QLatin1Char('\n')));
    });
}

void ProfileEditor::editCustomTextEditor(QQuickItem* from)
{
    QQuickWindow* dialog = QmlDialogs::openFrom(QStringLiteral("TextInputDialog"),
        from,
        {{QStringLiteral("title"), i18n("Text Editor Custom Command")},
            {QStringLiteral("labelText"),
                i18n("The format is e.g. 'editorExec PATH:LINE:COLUMN'\n\n"
                     "PATH    will be replaced by the path to the text file\n"
                     "LINE    will be replaced by the line number\n"
                     "COLUMN  (optional) will be replaced by the column number\n"
                     "Note: you will need to replace 'PATH:LINE:COLUMN' by the actual\n"
                     "syntax the editor you want to use supports; e.g.:\n"
                     "gedit +LINE:COLUMN PATH\n\n"
                     "If PATH or LINE aren't present in the command, this setting\n"
                     "will be ignored and the file will be opened by the default text\n"
                     "editor.")},
            {QStringLiteral("textValue"), value(QStringLiteral("TextEditorCmdCustom"))}},
        Qt::NonModal);
    if (!dialog) {
        return;
    }
    QmlDialogs::onAccepted(dialog, this, [this, dialog]() {
        updateTempProfileProperty(Profile::TextEditorCmdCustom, dialog->property("textValue").toString());
    });
}

void ProfileEditor::chooseEncoding(QQuickItem* below)
{
    KCodecAction codecAction(nullptr);
    codecAction.setCurrentCodec(value(QStringLiteral("DefaultEncoding")).toString());
    connect(&codecAction, &KCodecAction::codecNameTriggered, this, [this](const QByteArray& codecName) {
        setValue(QStringLiteral("DefaultEncoding"), QString::fromLocal8Bit(codecName));
    });
    connect(&codecAction, &KCodecAction::defaultItemTriggered, this, [this] {
        setValue(QStringLiteral("DefaultEncoding"), QString::fromLocal8Bit(QStringConverter::nameForEncoding(QStringConverter::System)));
    });
    const QPoint position = below ? below->mapToGlobal(QPointF(0, below->height())).toPoint() : QCursor::pos();
    codecAction.menu()->exec(position);
}

void ProfileEditor::unpreview(Profile::Property property)
{
    auto node = _previewedProperties.extract(property);
    if (!node) {
        return;
    }

    Profile::PropertyMap map;
    map.insert(std::move(node));
    ProfileManager::instance()->changeProfile(_profile, map, false);
}

QVariantList ProfileEditor::colorSchemes() const
{
    QList<std::shared_ptr<const ColorScheme>> schemes = ColorSchemeManager::instance()->allColorSchemes();
    std::sort(schemes.begin(), schemes.end(), [](const auto& a, const auto& b) {
        return a->description() < b->description();
    });
    QVariantList choices;
    for (const auto& scheme : std::as_const(schemes)) {
        QColor background = scheme->backgroundColor();
        background.setAlphaF(scheme->opacity());
        choices.append(QVariantMap{{QStringLiteral("name"), scheme->name()},
            {QStringLiteral("description"), scheme->description()},
            {QStringLiteral("background"), background},
            {QStringLiteral("foreground"), scheme->foregroundColor()}});
    }
    return choices;
}

QString ProfileEditor::transparencyWarning() const
{
    const auto scheme = ColorSchemeManager::instance()->findColorScheme(value(QStringLiteral("ColorScheme")).toString());
    if (!scheme || scheme->opacity() >= 1.0) {
        return {};
    }
    if (!WindowSystemInfo::compositingActive()) {
        return i18n(
            "This color scheme uses a transparent background"
            " which does not appear to be supported on your"
            " desktop");
    }
    if (!WindowSystemInfo::HAVE_TRANSPARENCY) {
        return i18n(
            "Konsole was started before desktop effects were enabled."
            " You need to restart Konsole to see transparent background.");
    }
    return {};
}

QString ProfileEditor::colorSchemeMessage() const
{
    return _colorSchemeMessage;
}

QFont ProfileEditor::previewFont() const
{
    QFont font = _profile->font();
    font.setStyleStrategy(_profile->antiAliasFonts() ? QFont::PreferAntialias : QFont::NoAntialias);
    return font;
}

QString ProfileEditor::fontText() const
{
    const QFont font = _profile->font();
    return QStringLiteral("%1 %2pt").arg(font.family()).arg(font.pointSize());
}

QString ProfileEditor::emojiFontText() const
{
    const QFont font = _profile->emojiFont();
    return QStringLiteral("%1 %2pt").arg(font.family()).arg(font.pointSize());
}

void ProfileEditor::selectColorScheme(const QString& name)
{
    updateTempProfileProperty(Profile::ColorScheme, name);
    preview(Profile::ColorScheme, name);
    Q_EMIT changed();
}

bool ProfileEditor::canResetColorScheme(const QString& name) const
{
    return ColorSchemeManager::instance()->canResetColorScheme(name);
}

bool ProfileEditor::isColorSchemeDeletable(const QString& name) const
{
    return ColorSchemeManager::instance()->isColorSchemeDeletable(name);
}

void ProfileEditor::removeColorScheme(const QString& name)
{
    if (ColorSchemeManager::instance()->deleteColorScheme(name)) {
        Q_EMIT colorSchemesChanged();
        Q_EMIT colorSchemeChosen(QString());
    }
}

void ProfileEditor::resetColorScheme(const QString& name)
{
    ColorSchemeManager::instance()->deleteColorScheme(name);
    Q_EMIT colorSchemesChanged();
    // The list selects the profile's scheme again, as a choice.
    const QString current = _profile->colorScheme();
    selectColorScheme(current);
    Q_EMIT colorSchemeChosen(current);
}

void ProfileEditor::editColorScheme(const QString& name, bool isNewScheme, QQuickItem* from)
{
    const auto scheme = name.isEmpty() ? ColorSchemeManager::instance()->defaultColorScheme() : ColorSchemeManager::instance()->findColorScheme(name);
    if (!scheme) {
        return;
    }
    // One editor at a time.
    if (_colorSchemeEditor) {
        _colorSchemeEditor->close();
    }
    auto* backend = new ColorSchemeEditorBackend(scheme, isNewScheme, WindowSystemInfo::compositingActive());
    connect(backend, &ColorSchemeEditorBackend::saveRequested, this, &ProfileEditor::saveColorScheme);
    QQuickWindow* dialog = QmlDialogs::openFrom(QStringLiteral("ColorSchemeEditorDialog"), from, {{QStringLiteral("editor"), QVariant::fromValue(backend)}}, Qt::NonModal);
    if (!dialog) {
        delete backend;
        return;
    }
    backend->setParent(dialog);
    _colorSchemeEditor = dialog;
}

void ProfileEditor::saveColorScheme(const ColorScheme& scheme, bool isNewScheme)
{
    auto newScheme = std::make_shared<ColorScheme>(scheme);

    // if this is a new color scheme, pick a name based on the description
    if (isNewScheme) {
        newScheme->setName(newScheme->description());
    }

    ColorSchemeManager::instance()->addColorScheme(newScheme);
    Q_EMIT colorSchemesChanged();

    // select the edited or the new colorScheme after saving the changes
    selectColorScheme(newScheme->name());
    Q_EMIT colorSchemeChosen(newScheme->name());
}

void ProfileEditor::getNewColorSchemes(QQuickItem* from)
{
    FrameworkDialogScope scope(QStringLiteral("get new color schemes"));
    if (!_getNewDialog) {
        // KNSWidgets::Button's dialog: titled after the button, reported when it closes.
        _getNewDialog = new KNSWidgets::Dialog(QStringLiteral(":/konsole/konsole.knsrc"));
        _getNewDialog->setWindowTitle(i18n("Get New...").remove(QLatin1Char('&')));
        connect(this, &QObject::destroyed, _getNewDialog, &QObject::deleteLater);
        connect(_getNewDialog, &KNSWidgets::Dialog::finished, this, [this]() {
            gotNewColorSchemes(_getNewDialog->changedEntries());
        });
    }
    setTransientFor(_getNewDialog, from);
    _getNewDialog->open();
}

void ProfileEditor::gotNewColorSchemes(const QList<KNSCore::Entry>& changedEntries)
{
    int failures = 0;
    for (auto& entry : std::as_const(changedEntries)) {
        switch (entry.status()) {
        case KNSCore::Entry::Installed: {
            const auto installedFiles = entry.installedFiles();
            for (const QString& file : installedFiles) {
                if (ColorSchemeManager::instance()->loadColorScheme(file)) {
                    continue;
                }
                sonicWarning() << "Failed to load file" << file;
                ++failures;
            }
            if (failures == entry.installedFiles().size()) {
                _colorSchemeMessage = xi18nc("@info", "Scheme <resource>%1</resource> failed to load.", entry.name());
                Q_EMIT colorSchemeMessageChanged();
                QTimer::singleShot(8000, this, [this]() {
                    _colorSchemeMessage.clear();
                    Q_EMIT colorSchemeMessageChanged();
                });
            }
            break;
        }
        case KNSCore::Entry::Deleted: {
            const auto uninstalledFiles = entry.uninstalledFiles();
            for (const auto& file : uninstalledFiles) {
                if (ColorSchemeManager::instance()->unloadColorScheme(file)) {
                    continue;
                }
                sonicWarning() << "Failed to unload file" << file;
                // If unloading fails we do not care. If the scheme failed here
                // it either wasn't loaded or was invalid to begin with.
            }
            break;
        }
        case KNSCore::Entry::Invalid:
        case KNSCore::Entry::Installing:
        case KNSCore::Entry::Downloadable:
        case KNSCore::Entry::Updateable:
        case KNSCore::Entry::Updating:
            // Not interesting.
            break;
        }
    }
    Q_EMIT colorSchemesChanged();
    // The list shows the profile's scheme again; it isn't chosen anew.
    Q_EMIT colorSchemeChosen(_profile->colorScheme());
}

void ProfileEditor::chooseFont(QQuickItem* from)
{
    showFontDialog(false, from);
}

void ProfileEditor::chooseEmojiFont(QQuickItem* from)
{
    showFontDialog(true, from);
}

void ProfileEditor::showFontDialog(bool emoji, QQuickItem* from)
{
    QPointer<FontDialog>& dialog = emoji ? _emojiFontDialog : _fontDialog;
    const Profile::Property property = emoji ? Profile::EmojiFont : Profile::Font;
    FrameworkDialogScope scope(emoji ? QStringLiteral("emoji font") : QStringLiteral("font"));
    if (!dialog) {
        dialog = emoji ? new FontDialog(nullptr, true, _profile->emojiFont()) : new FontDialog(nullptr);
        dialog->setModal(true);
        connect(this, &QObject::destroyed, dialog, &QObject::deleteLater);
        connect(dialog, &FontDialog::fontChanged, this, [this, property](const QFont& font) {
            preview(property, font);
            Q_EMIT changed();
        });
        connect(dialog, &FontDialog::accepted, this, [this, property, dialog = dialog.data()]() {
            const QFont font = dialog->font();
            preview(property, font);
            updateTempProfileProperty(property, font);
        });
        connect(dialog, &FontDialog::rejected, this, [this, property]() {
            unpreview(property);
            Q_EMIT changed();
        });
    }
    if (!emoji) {
        dialog->setFont(_profile->font());
    }
    setTransientFor(dialog, from);
    dialog->show();
}

QVariantList ProfileEditor::keyBindings() const
{
    auto* manager = KeyboardTranslatorManager::instance();
    QVariantList choices;
    const QStringList names = manager->allTranslators();
    for (const QString& name : names) {
        const KeyboardTranslator* translator = manager->findTranslator(name);
        if (translator == nullptr) {
            continue;
        }
        choices.append(QVariantMap{{QStringLiteral("name"), translator->name()},
            {QStringLiteral("description"), translator->description()},
            {QStringLiteral("path"), manager->findTranslatorPath(name)}});
    }
    std::sort(choices.begin(), choices.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value(QStringLiteral("description")).toString() < b.toMap().value(QStringLiteral("description")).toString();
    });
    return choices;
}

void ProfileEditor::selectKeyBindings(const QString& name)
{
    updateTempProfileProperty(Profile::KeyBindings, name);
}

bool ProfileEditor::canResetKeyBindings(const QString& name) const
{
    return KeyboardTranslatorManager::instance()->isTranslatorResettable(name);
}

bool ProfileEditor::isKeyBindingsDeletable(const QString& name) const
{
    return KeyboardTranslatorManager::instance()->isTranslatorDeletable(name);
}

void ProfileEditor::removeKeyBindings(const QString& name)
{
    if (KeyboardTranslatorManager::instance()->deleteTranslator(name)) {
        Q_EMIT keyBindingsChanged();
        Q_EMIT keyBindingsChosen(QString());
    }
}

void ProfileEditor::resetKeyBindings(const QString& name)
{
    auto* manager = KeyboardTranslatorManager::instance();
    manager->deleteTranslator(name);
    // find and load the translator
    manager->findTranslator(name);
    Q_EMIT keyBindingsChanged();
    selectKeyBindings(name);
    Q_EMIT keyBindingsChosen(name);
}

void ProfileEditor::editKeyBindings(const QString& name, bool isNewTranslator, QQuickItem* from)
{
    auto* manager = KeyboardTranslatorManager::instance();
    const KeyboardTranslator* translator = name.isEmpty() ? manager->defaultTranslator() : manager->findTranslator(name);
    if (translator == nullptr) {
        return;
    }
    auto* backend = new KeyBindingEditorBackend(translator, _profile->keyBindings(), isNewTranslator);
    connect(backend, &KeyBindingEditorBackend::saved, this, [this](const QString& saved, bool isProfileTranslator) {
        Q_EMIT keyBindingsChanged();
        // The list selects the saved scheme, which chooses it.
        selectKeyBindings(saved);
        if (isProfileTranslator) {
            updateTempProfileProperty(Profile::KeyBindings, saved);
        }
        Q_EMIT keyBindingsChosen(saved);
    });
    QQuickWindow* dialog = QmlDialogs::openFrom(QStringLiteral("KeyBindingEditorDialog"), from, {{QStringLiteral("editor"), QVariant::fromValue(backend)}});
    if (!dialog) {
        delete backend;
        return;
    }
    backend->setParent(dialog);
}
}

#include "moc_ProfileEditor.cpp"
