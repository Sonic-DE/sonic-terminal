/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// The Edit Profile dialog's Keyboard page (EditProfileKeyboardPage.ui): the key bindings
// schemes, the chosen one the profile's.
DialogPage {
    id: root

    required property ProfileEditor editor
    // The list's scheme, as its selection.
    property string selectedBindings: editor.value("KeyBindings")
    // What its buttons may do, again whenever the schemes change.
    readonly property bool bindingsResettable: editor.keyBindings && editor.canResetKeyBindings(selectedBindings)
    readonly property bool bindingsDeletable: editor.keyBindings && editor.isKeyBindingsDeletable(selectedBindings)

    title: i18n("Keyboard")
    header: i18n("Key bindings")
    iconName: "input-keyboard"

    Connections {
        target: root.editor

        function onKeyBindingsChosen(name) {
            root.selectedBindings = name;
        }
    }

    QQC2.Label {
        objectName: "note"
        Layout.fillWidth: true
        // Wrapped: its one-line width mustn't widen the dialog.
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        wrapMode: Text.WordWrap
        text: i18nc("@info", "Key bindings control how combinations of keystrokes in the terminal window are converted into the stream of characters that is then sent to the current terminal program. For more information on how to customize the key bindings check the Konsole Handbook.")
    }

    RowLayout {
        objectName: "keyBindingsGroup"
        Layout.fillWidth: true
        Layout.fillHeight: true

        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: Kirigami.Units.gridUnit * 20
            Layout.preferredHeight: Kirigami.Units.gridUnit * 16
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false

            background: Rectangle {
                color: Kirigami.Theme.backgroundColor
                border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2)
            }

            ListView {
                id: keyBindingList

                objectName: "keyBindingList"
                clip: true
                model: root.editor.keyBindings
                currentIndex: model.findIndex(bindings => bindings.name === root.selectedBindings)
                keyNavigationEnabled: true
                // The current scheme is scrolled into view, as by QAbstractItemView.
                Component.onCompleted: positionViewAtIndex(currentIndex, ListView.Contain)
                onCurrentIndexChanged: {
                    positionViewAtIndex(currentIndex, ListView.Contain);
                    // Keys move the selection, as the widget's list.
                    const name = currentIndex >= 0 ? model[currentIndex].name : "";
                    if (activeFocus && name !== root.selectedBindings) {
                        root.selectedBindings = name;
                        root.editor.selectKeyBindings(name);
                    }
                }

                delegate: QQC2.ItemDelegate {
                    id: bindingsRow

                    required property var modelData
                    required property int index
                    readonly property bool selected: ListView.isCurrentItem
                    // As the widget style draws a view's selection: in the inactive colours
                    // while the view hasn't the focus.
                    readonly property var colors: keyBindingList.activeFocus ? palette.active : palette.inactive

                    width: ListView.view.width
                    text: modelData.description
                    icon.name: "preferences-desktop-keyboard"
                    palette.buttonText: selected ? colors.highlightedText : colors.text

                    background: Rectangle {
                        readonly property color highlight: bindingsRow.palette.active.highlight

                        color: bindingsRow.selected ? bindingsRow.colors.highlight
                            : bindingsRow.hovered ? Qt.rgba(highlight.r, highlight.g, highlight.b, 0.2) : "transparent"
                    }
                    onClicked: {
                        keyBindingList.forceActiveFocus();
                        root.selectedBindings = modelData.name;
                        root.editor.selectKeyBindings(modelData.name);
                    }
                    QQC2.ToolTip.text: modelData.path
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                }
            }
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            // As wide as its widest button, not more.
            Layout.fillWidth: false

            QQC2.Button {
                objectName: "newKeyBindingsButton"
                Layout.fillWidth: true
                text: i18n("New...")
                icon.name: "list-add"
                onClicked: root.editor.editKeyBindings(root.selectedBindings, true, this)
                QQC2.ToolTip.text: i18n("Create a new key bindings scheme based upon the selected bindings")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            }
            QQC2.Button {
                objectName: "editKeyBindingsButton"
                Layout.fillWidth: true
                text: i18n("Edit...")
                icon.name: "document-edit"
                enabled: keyBindingList.currentIndex >= 0
                onClicked: root.editor.editKeyBindings(root.selectedBindings, false, this)
                QQC2.ToolTip.text: i18n("Edit the selected key bindings scheme")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            }
            QQC2.Button {
                objectName: "removeKeyBindingsButton"
                Layout.fillWidth: true
                text: i18n("Remove")
                icon.name: "edit-delete"
                // A scheme that can be reset can't be deleted.
                enabled: keyBindingList.currentIndex >= 0 && root.bindingsDeletable && !root.bindingsResettable
                onClicked: root.editor.removeKeyBindings(root.selectedBindings)
                QQC2.ToolTip.text: i18n("Delete the selected key bindings scheme")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            }
            QQC2.Button {
                objectName: "resetKeyBindingsButton"
                Layout.fillWidth: true
                text: i18n("Defaults")
                icon.name: "edit-undo"
                enabled: keyBindingList.currentIndex >= 0 && root.bindingsResettable
                onClicked: root.editor.resetKeyBindings(root.selectedBindings)
                QQC2.ToolTip.text: i18n("Reset the selected key bindings scheme to its default values")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            }
        }
    }
}
