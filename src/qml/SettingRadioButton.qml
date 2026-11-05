/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.sonicde.terminal

// One choice of an Enum setting (a radio button of a kcfg_<setting> button group, named
// after its choice). Several settings share a layout, so exclusivity comes from the setting.
QQC2.RadioButton {
    id: root

    required property SettingsEditor settings
    required property string setting
    required property string choice

    function sync() {
        checked = settings.choice(setting) === choice;
    }

    objectName: choice
    autoExclusive: false
    onClicked: {
        settings.setChoice(setting, choice);
        sync();
    }
    Component.onCompleted: sync()

    Connections {
        target: root.settings

        function onChanged() {
            root.sync();
        }
    }
}
