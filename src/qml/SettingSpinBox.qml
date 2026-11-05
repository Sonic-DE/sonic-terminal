/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.sonicde.terminal

// A kcfg_<setting> spin box: an Int setting of a SettingsEditor, with QSpinBox's suffix.
QQC2.SpinBox {
    id: root

    required property SettingsEditor settings
    required property string setting
    property string suffix
    property string toolTip: settings.toolTip(setting)

    function sync() {
        value = settings.value(setting);
    }

    objectName: "kcfg_" + setting
    editable: true
    textFromValue: (value, locale) => Number(value).toLocaleString(locale, "f", 0) + suffix
    valueFromText: (text, locale) => Number.fromLocaleString(locale, text.replace(suffix, "").trim())
    onValueModified: settings.setValue(setting, value)
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
