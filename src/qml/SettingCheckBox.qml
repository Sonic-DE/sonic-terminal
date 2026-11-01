/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.sonicde.terminal

// A kcfg_<setting> check box: a Bool setting of a SettingsEditor; the setting's tooltip
// when none is given, as KConfigDialogManager does.
QQC2.CheckBox {
    id: root

    required property SettingsEditor settings
    required property string setting
    property string toolTip: settings.toolTip(setting)

    function sync() {
        checked = settings.value(setting);
    }

    objectName: "kcfg_" + setting
    onToggled: settings.setValue(setting, checked)
    Component.onCompleted: sync()
    QQC2.ToolTip.text: toolTip
    QQC2.ToolTip.visible: hovered && toolTip.length > 0
    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval

    Connections {
        target: root.settings

        function onChanged() {
            root.sync();
        }
    }
}
