/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// The replaced widget dialogs, each as its caller set it up, with the QML dialog replacing it
// and the properties that dialog is opened with (DialogContentTest, DialogLayoutTest).

#include "../CopyInputDialog.h"
#include "../HistorySizeDialog.h"
#include "../KeyBindingEditor.h"
#include "../MainWindow.h"
#include "../RenameTabDialog.h"
#include "../WindowSystemInfo.h"
#include "../colorscheme/ColorSchemeEditor.h"
#include "../colorscheme/ColorSchemeManager.h"
#include "../keyboardtranslator/KeyboardTranslatorManager.h"
#include "../profile/ProfileManager.h"
#include "../quick/ColorSchemeEditorBackend.h"
#include "../quick/CopyInputModel.h"
#include "../quick/KeyBindingEditorBackend.h"
#include "../quick/ProfileEditor.h"
#include "../quick/ProfileListModel.h"
#include "../settings/ConfigurationDialog.h"
#include "../settings/GeneralSettings.h"
#include "../settings/MemorySettings.h"
#include "../settings/ProfileSettings.h"
#include "../settings/TabBarSettings.h"
#include "../settings/TemporaryFilesSettings.h"
#include "../settings/ThumbnailsSettings.h"
#include "../widgets/EditProfileDialog.h"
#include "KonsoleSettings.h"
#include "ui_PartInfo.h"

#include <KConfigDialog>
#include <KEditToolBar>
#include <KLocalizedString>
#include <KToolBarEditor>
#include <KXMLGUIFactory>

#include <QApplication>
#include <QDir>
#include <QInputDialog>
#include <QPointer>
#include <QShowEvent>

#include <functional>

namespace DialogCases {
struct Case {
    const char* name;
    std::function<QWidget*()> widget;
    QString type;
    // The QML dialog's properties; models it needs are parented to the given object.
    std::function<QVariantMap(QObject*)> properties;
};

// The profile both Edit Profile dialogs show.
inline Konsole::Profile::Ptr editedProfile()
{
    static const Konsole::Profile::Ptr profile = [] {
        Konsole::Profile::Ptr created(new Konsole::Profile(Konsole::ProfileManager::instance()->defaultProfile()));
        created->setProperty(Konsole::Profile::Name, QStringLiteral("DialogCases"));
        return created;
    }();
    return profile;
}

// The window whose toolbars both Configure Toolbars dialogs edit, with a session's.
inline Konsole::MainWindow* toolBarWindow()
{
    static QPointer<Konsole::MainWindow> window;
    if (!window) {
        window = new Konsole::MainWindow();
        window->createSession(Konsole::ProfileManager::instance()->defaultProfile(), QDir::tempPath());
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, window, &QObject::deleteLater);
    }
    return window;
}

inline QList<Case> all()
{
    using namespace Konsole;
    const auto none = [](QObject*) {
        return QVariantMap();
    };
    const QString editorLabel = i18n(
        "The format is e.g. 'editorExec PATH:LINE:COLUMN'\n\n"
        "PATH    will be replaced by the path to the text file\n"
        "LINE    will be replaced by the line number\n"
        "COLUMN  (optional) will be replaced by the column number\n"
        "Note: you will need to replace 'PATH:LINE:COLUMN' by the actual\n"
        "syntax the editor you want to use supports; e.g.:\n"
        "gedit +LINE:COLUMN PATH\n\n"
        "If PATH or LINE aren't present in the command, this setting\n"
        "will be ignored and the file will be opened by the default text\n"
        "editor.");
    return {
        {"Rename Tab",
            [] {
                auto* dialog = new RenameTabDialog;
                dialog->setColor(QColor());
                dialog->setActivityColor(QColor());
                return dialog;
            },
            QStringLiteral("RenameTabDialog"),
            none},
        {"History Size",
            [] {
                auto* dialog = new HistorySizeDialog;
                dialog->setMode(Enum::FixedSizeHistory);
                return dialog;
            },
            QStringLiteral("HistorySizeDialog"),
            [](QObject*) {
                return QVariantMap{{QStringLiteral("mode"), Enum::FixedSizeHistory}};
            }},
        {"Copy Input",
            [] {
                return new CopyInputDialog;
            },
            QStringLiteral("CopyInputDialog"),
            [](QObject* parent) {
                return QVariantMap{{QStringLiteral("sessions"), QVariant::fromValue(new CopyInputModel(parent))}};
            }},
        // QInputDialog::getMultiLineText() (EditProfileDialog::showEnvironmentEditor()).
        {"Environment",
            [] {
                auto* dialog = new QInputDialog;
                dialog->setInputMode(QInputDialog::TextInput);
                dialog->setOptions(QInputDialog::UsePlainTextEditForTextInput);
                dialog->setWindowTitle(i18n("Edit Environment"));
                dialog->setLabelText(i18n("One environment variable per line"));
                return dialog;
            },
            QStringLiteral("TextInputDialog"),
            [](QObject*) {
                return QVariantMap{{QStringLiteral("title"), i18n("Edit Environment")},
                    {QStringLiteral("labelText"), i18n("One environment variable per line")},
                    {QStringLiteral("multiLine"), true}};
            }},
        {"Custom text editor",
            [editorLabel] {
                auto* dialog = new QInputDialog;
                dialog->setLabelText(editorLabel);
                dialog->setWindowTitle(i18n("Text Editor Custom Command"));
                return dialog;
            },
            QStringLiteral("TextInputDialog"),
            [editorLabel](QObject*) {
                return QVariantMap{{QStringLiteral("title"), i18n("Text Editor Custom Command")}, {QStringLiteral("labelText"), editorLabel}};
            }},
        // MainWindow::showSettingsDialog() before the QML dialog.
        {"Configure",
            [] {
                auto* dialog = new ConfigurationDialog(nullptr, KonsoleSettings::self());
                const auto addPage = [dialog](QWidget* page, const QString& name, const QString& icon) {
                    auto* item = new KPageWidgetItem(page, name);
                    item->setIcon(QIcon::fromTheme(icon));
                    dialog->addPage(item, true);
                };
                addPage(new GeneralSettings(dialog), i18nc("@title Preferences page name", "General"), QStringLiteral("utilities-terminal"));
                addPage(new ProfileSettings(dialog), i18nc("@title Preferences page name", "Profiles"), QStringLiteral("preferences-desktop-theme"));
                addPage(new MemorySettings(dialog), i18nc("@title Preferences page name", "Memory"), QStringLiteral("memory"));
                addPage(new TabBarSettings(dialog), i18nc("@title Preferences page name", "Tab Bar / Splitters"), QStringLiteral("preferences-tabs"));
                addPage(new TemporaryFilesSettings(dialog), i18nc("@title Preferences page name", "Temporary Files"), QStringLiteral("folder-temp"));
                addPage(new ThumbnailsSettings(dialog), i18nc("@title Preferences page name", "Thumbnails"), QStringLiteral("image-jpeg"));
                return dialog;
            },
            QStringLiteral("ConfigureDialog"),
            [](QObject* parent) {
                return QVariantMap{{QStringLiteral("profiles"), QVariant::fromValue(new ProfileListModel(parent))}};
            }},
        // Part::showManageProfilesDialog() before the QML dialog.
        {"KPart settings",
            [] {
                auto* dialog = new KConfigDialog(nullptr, QStringLiteral("DialogCases"), KonsoleSettings::self());
                dialog->setFaceType(KPageDialog::Tabbed);
                dialog->addPage(new ProfileSettings(dialog), i18nc("@title Preferences page name", "Profiles"), QStringLiteral("configure"));
                // PartInfoSettings is built into the part only; it is its .ui and nothing else.
                auto* partInfo = new QWidget(dialog);
                Ui::PartInfoSettings().setupUi(partInfo);
                dialog->addPage(partInfo, i18nc("@title Preferences page name", "Part Info"), QStringLiteral("dialog-information"));
                return dialog;
            },
            QStringLiteral("PartSettingsDialog"),
            [](QObject* parent) {
                return QVariantMap{{QStringLiteral("profiles"), QVariant::fromValue(new ProfileListModel(parent))}};
            }},
        // SessionController::editCurrentProfile() before the QML dialog.
        {"Edit Profile",
            [] {
                auto* dialog = new EditProfileDialog;
                dialog->setProfile(editedProfile());
                return dialog;
            },
            QStringLiteral("EditProfileDialog"),
            [](QObject* parent) {
                return QVariantMap{
                    {QStringLiteral("editor"), QVariant::fromValue(new ProfileEditor(editedProfile(), ProfileEditor::ExistingProfile, parent))}};
            }},
        // EditProfileDialog::showColorSchemeEditor() and showKeyBindingEditor().
        {"Color Scheme Editor",
            [] {
                auto* dialog = new ColorSchemeEditor(WindowSystemInfo::compositingActive());
                dialog->setup(ColorSchemeManager::instance()->defaultColorScheme(), false);
                return dialog;
            },
            QStringLiteral("ColorSchemeEditorDialog"),
            [](QObject* parent) {
                return QVariantMap{{QStringLiteral("editor"),
                    QVariant::fromValue(new ColorSchemeEditorBackend(ColorSchemeManager::instance()->defaultColorScheme(),
                        false,
                        WindowSystemInfo::compositingActive(),
                        parent))}};
            }},
        {"Key Binding Editor",
            [] {
                // Its size hint is a share of the Edit Profile dialog it's opened from.
                auto* parent = new EditProfileDialog;
                parent->setProfile(editedProfile());
                parent->adjustSize();
                auto* dialog = new KeyBindingEditor(parent);
                QObject::connect(dialog, &QObject::destroyed, parent, &QObject::deleteLater);
                dialog->setup(KeyboardTranslatorManager::instance()->defaultTranslator(), QString(), false);
                return dialog;
            },
            QStringLiteral("KeyBindingEditorDialog"),
            [](QObject* parent) {
                return QVariantMap{
                    {QStringLiteral("editor"),
                        QVariant::fromValue(new KeyBindingEditorBackend(KeyboardTranslatorManager::instance()->defaultTranslator(), QString(), false, parent))}};
            }},
        // KXmlGuiWindow::configureToolbars() before the QML dialog; it loads when shown.
        {"Configure Toolbars",
            [] {
                auto* dialog = new KEditToolBar(toolBarWindow()->guiFactory());
                QShowEvent show;
                QApplication::sendEvent(dialog, &show);
                return dialog;
            },
            QStringLiteral("ConfigureToolbarsDialog"),
            [](QObject* parent) {
                auto* editor = new KToolBarEditor(parent);
                editor->load(toolBarWindow()->guiFactory());
                return QVariantMap{{QStringLiteral("editor"), QVariant::fromValue(editor)}};
            }},
    };
}
}
