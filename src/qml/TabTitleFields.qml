/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.sonicde.terminal

// RenameTabWidget: the tab title formats with their Insert menus, and the tab colours.
GridLayout {
    id: root

    property alias tabTitle: tabTitleEdit.text
    property alias remoteTabTitle: remoteTabTitleEdit.text
    readonly property alias color: tabColorCombo.color
    readonly property alias activityColor: tabActivityColorCombo.color
    property alias tabTitleEdit: tabTitleEdit
    property alias remoteTabTitleEdit: remoteTabTitleEdit

    function setColor(color) {
        tabColorCombo.setColor(color);
    }

    function setActivityColor(color) {
        tabActivityColorCombo.setColor(color);
    }

    // The user changed them (RenameTabWidget's signals).
    signal tabTitleFormatEdited(string text)
    signal remoteTabTitleFormatEdited(string text)
    signal tabColorChanged(color color)
    signal tabActivityColorChanged(color color)

    function insert(edit, context, button) {
        const element = DialogHelpers.chooseTitleFormat(context, button);
        if (element.length > 0) {
            edit.insert(edit.cursorPosition, element);
        }
        edit.forceActiveFocus();
    }

    columns: 3

    MnemonicLabel {
        Layout.alignment: Qt.AlignRight
        label: i18n("&Tab title format:")
        buddy: tabTitleEdit
    }
    ClearableTextField {
        id: tabTitleEdit

        objectName: "tabTitleEdit"
        Layout.fillWidth: true
        QQC2.ToolTip.text: i18n("Normal tab title format")
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
        onTextChanged: root.tabTitleFormatEdited(text)
    }
    QQC2.Button {
        objectName: "tabTitleFormatButton"
        text: i18n("Insert")
        QQC2.ToolTip.text: i18nc("@info:tooltip", "Insert title format")
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
        // Session::LocalTabTitle
        onClicked: root.insert(tabTitleEdit, 0, this)
    }

    MnemonicLabel {
        Layout.alignment: Qt.AlignRight
        label: i18n("Remote tab title format:")
        buddy: remoteTabTitleEdit
    }
    ClearableTextField {
        id: remoteTabTitleEdit

        objectName: "remoteTabTitleEdit"
        Layout.fillWidth: true
        QQC2.ToolTip.text: i18n("Tab title format used when a remote command (e.g. connection to another computer via SSH) is being executed")
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
        onTextChanged: root.remoteTabTitleFormatEdited(text)
    }
    QQC2.Button {
        objectName: "remoteTabTitleFormatButton"
        text: i18n("Insert")
        QQC2.ToolTip.text: i18nc("@info:tooltip", "Insert remote title format")
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
        // Session::RemoteTabTitle
        onClicked: root.insert(remoteTabTitleEdit, 1, this)
    }

    MnemonicLabel {
        Layout.alignment: Qt.AlignRight
        label: i18n("Tab Co&lor:")
        buddy: tabColorCombo
    }
    ColorComboBox {
        id: tabColorCombo

        objectName: "tabColorCombo"
        Layout.fillWidth: true
        onColorActivated: color => root.tabColorChanged(color)
    }
    Item {}

    MnemonicLabel {
        Layout.alignment: Qt.AlignRight
        label: i18n("&Activity Tab Color:")
        buddy: tabActivityColorCombo
    }
    ColorComboBox {
        id: tabActivityColorCombo

        objectName: "tabActivityColorCombo"
        Layout.fillWidth: true
        onColorActivated: color => root.tabActivityColorChanged(color)
    }
    Item {}
}
