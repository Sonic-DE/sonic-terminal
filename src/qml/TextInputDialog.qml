/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.kde.kirigami as Kirigami

// QInputDialog's text input: a label above a line edit, or a plain text edit when
// multiLine (getMultiLineText()).
DialogWindow {
    id: root

    property alias labelText: label.text
    property bool multiLine: false
    property string textValue

    standardButtons: QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Cancel
    // Connected before any caller's connection, so callers read the edited text.
    onAccepted: textValue = multiLine ? area.text : field.text
    Component.onCompleted: {
        field.text = textValue;
        area.text = textValue;
    }

    QQC2.Label {
        id: label

        Layout.fillWidth: true
        textFormat: Text.PlainText
    }

    QQC2.TextField {
        id: field

        objectName: "textValueEdit"
        Layout.fillWidth: true
        Layout.minimumWidth: Kirigami.Units.gridUnit * 15
        visible: !root.multiLine
        focus: !root.multiLine
        selectByMouse: true
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: Kirigami.Units.gridUnit * 15
        Layout.preferredHeight: Kirigami.Units.gridUnit * 8
        visible: root.multiLine

        QQC2.TextArea {
            id: area

            objectName: "textValueArea"
            focus: root.multiLine
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.NoWrap
        }
    }
}
