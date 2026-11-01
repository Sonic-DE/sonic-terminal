/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.sonicde.terminal

// One value of an Int profile property (a button of the widget page's button group).
// Several properties share a layout, so exclusivity comes from the property.
QQC2.RadioButton {
    id: root

    required property ProfileEditor editor
    required property string profileProperty
    required property int value
    property string toolTip

    function sync() {
        // As Profile::property<int>(): some are stored as bools.
        checked = Number(editor.value(profileProperty)) === value;
    }

    autoExclusive: false
    onClicked: editor.setValue(profileProperty, value)
    Component.onCompleted: sync()
    QQC2.ToolTip.text: toolTip
    QQC2.ToolTip.visible: hovered && toolTip.length > 0
    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval

    Connections {
        target: root.editor

        function onChanged() {
            root.sync();
        }
    }
}
