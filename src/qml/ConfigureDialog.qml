/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.sonicde.terminal

// The main window's Configure dialog (ConfigurationDialog with MainWindow's pages):
// settings change on OK or Apply, Defaults resets the shown ones, OK also saves profiles.
PageDialog {
    id: root

    required property ProfileListModel profiles
    property bool hasDBus: true
    property bool memoryAvailable: true

    function showProfiles() {
        showPage(profilesPage);
    }

    // The settings the pages show (KConfigDialogManager's kcfg_ widgets).
    function settingNames(item) {
        let names = [];
        for (let i = 0; i < item.children.length; ++i) {
            const child = item.children[i];
            if (typeof child.setting === "string" && child.setting.length > 0 && !names.includes(child.setting)) {
                names.push(child.setting);
            }
            names = names.concat(settingNames(child).filter(name => !names.includes(name)));
        }
        return names;
    }

    title: i18nc("@title:window", "Configure")
    standardButtons: QQC2.DialogButtonBox.RestoreDefaults | QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Apply | QQC2.DialogButtonBox.Cancel

    onAccepted: {
        editor.apply();
        profiles.save();
    }
    onRejected: editor.reset()
    onApplied: editor.apply()
    onDefaultsRequested: editor.defaults()

    Component.onCompleted: {
        const pages = Array.prototype.filter.call(root.pages, page => page.available);
        editor.names = pages.reduce((names, page) => names.concat(settingNames(page)), []);
        standardButton(QQC2.DialogButtonBox.Apply).enabled = Qt.binding(() => editor.hasChanged);
        standardButton(QQC2.DialogButtonBox.RestoreDefaults).enabled = Qt.binding(() => !editor.isDefault);
    }

    SettingsEditor {
        id: editor
    }

    ConfigureGeneralPage {
        settings: editor
        hasDBus: root.hasDBus
    }
    ConfigureProfilesPage {
        id: profilesPage

        profiles: root.profiles
    }
    ConfigureMemoryPage {
        settings: editor
        available: root.memoryAvailable
    }
    ConfigureTabBarPage {
        settings: editor
    }
    ConfigureTemporaryFilesPage {
        settings: editor
    }
    ConfigureThumbnailsPage {
        settings: editor
    }
}
