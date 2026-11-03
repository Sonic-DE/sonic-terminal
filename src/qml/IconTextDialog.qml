/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// Configure Toolbars' Change Text (IconTextEditDialog): an action's icon text, never empty,
// and whether it hides beside the icon. Modal, its height fixed.
DialogWindow {
    id: root

    property alias iconText: textField.text
    property alias textAlongsideIconHidden: hiddenCheck.checked

    title: i18ndc("kxmlgui6", "@title:window", "Change Text")
    modality: Qt.ApplicationModal
    standardButtons: QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Cancel
    Component.onCompleted: {
        // setFixedHeight(sizeHint().height())
        minimumHeight = height;
        maximumHeight = height;
        standardButton(QQC2.DialogButtonBox.Ok).enabled = Qt.binding(() => textField.text.trim().length > 0);
        textField.forceActiveFocus();
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2

        MnemonicLabel {
            label: i18nd("kxmlgui6", "Icon te&xt:")
            buddy: textField
        }
        ClearableTextField {
            id: textField

            objectName: "iconTextEdit"
            Layout.fillWidth: true
        }
        Item {}
        QQC2.CheckBox {
            id: hiddenCheck

            objectName: "hiddenCheck"
            text: i18ndc("kxmlgui6", "@option:check", "&Hide text when toolbar shows text alongside icons")
        }
    }
}
