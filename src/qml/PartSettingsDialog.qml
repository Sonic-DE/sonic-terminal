/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.sonicde.terminal

// The KPart's settings dialog (a KConfigDialog with the Tabbed face): Profiles and Part
// Info. It shows no settings, so Apply and Defaults stay disabled, as KConfigDialog's.
TabbedPageDialog {
    id: root

    required property ProfileListModel profiles

    title: i18nc("@title:window", "Configure")
    standardButtons: QQC2.DialogButtonBox.RestoreDefaults | QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Apply | QQC2.DialogButtonBox.Cancel | QQC2.DialogButtonBox.Help

    Component.onCompleted: {
        standardButton(QQC2.DialogButtonBox.Apply).enabled = false;
        standardButton(QQC2.DialogButtonBox.RestoreDefaults).enabled = false;
    }
    onHelpRequested: DialogHelpers.showHelp()

    ConfigureProfilesPage {
        profiles: root.profiles
        title: i18nc("@title Preferences page name", "Profiles")
        iconName: "configure"
    }
    PartInfoPage {}
}
