/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.sonicde.terminal

// A check box editing a Bool profile property in a ProfileEditor.
QQC2.CheckBox {
    required property ProfileEditor editor
    required property string profileProperty
    property string toolTip

    Component.onCompleted: checked = editor.value(profileProperty)
    onToggled: editor.setValue(profileProperty, checked)
    QQC2.ToolTip.text: toolTip
    QQC2.ToolTip.visible: hovered && toolTip.length > 0
    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
}
