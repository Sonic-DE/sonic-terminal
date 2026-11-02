/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// The Key Binding Editor (KeyBindingEditor): a scheme's description and entries, a filter
// and the test area. OK saves it, refused with a message box for an empty description;
// Cancel is the default button.
DialogWindow {
    id: root

    required property KeyBindingEditorBackend editor
    // The selected rows, as QTableWidget's row selection.
    property var selectedRows: []

    function select(row, add) {
        if (!add) {
            selectedRows = [row];
        } else if (selectedRows.includes(row)) {
            selectedRows = selectedRows.filter(selected => selected !== row);
        } else {
            selectedRows = selectedRows.concat([row]);
        }
    }

    title: editor.title
    standardButtons: QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Cancel
    defaultButton: QQC2.DialogButtonBox.Cancel
    acceptCheck: () => editor.accept(root.contentItem)

    GridLayout {
        Layout.fillWidth: true
        columns: 2

        QQC2.Label {
            text: i18n("Description:")
        }
        ClearableTextField {
            objectName: "descriptionEdit"
            Layout.fillWidth: true
            placeholderText: i18nc("@label:textbox", "Enter descriptive label")
            // Set once: a binding would be written back from its own change.
            Component.onCompleted: text = root.editor.description
            onTextChanged: root.editor.description = text
        }
        QQC2.Label {
            text: i18n("Filter:")
        }
        ClearableTextField {
            id: filterEdit

            objectName: "filterEdit"
            Layout.fillWidth: true
        }
    }

    // QTableWidget: Key Combination, Output; a cell is edited in place.
    QQC2.Frame {
        objectName: "keyBindingTable"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: Kirigami.Units.gridUnit * 12
        Layout.minimumWidth: Kirigami.Units.gridUnit * 30
        padding: 0
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false

        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2)
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true

                QQC2.Label {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    padding: Kirigami.Units.smallSpacing
                    font.bold: true
                    text: i18n("Key Combination")
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    padding: Kirigami.Units.smallSpacing
                    font.bold: true
                    text: i18n("Output")
                }
            }

            ListView {
                id: entries

                objectName: "keyBindingEntries"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.editor
                QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

                delegate: Rectangle {
                    id: entry

                    required property int index
                    required property string condition
                    required property string result
                    // QTableWidget::findItems(text, Qt::MatchContains): either cell, any case.
                    readonly property bool shown: [condition, result].some(text => text.toLowerCase().includes(filterEdit.text.toLowerCase()))
                    readonly property bool selected: root.selectedRows.includes(index)

                    function commit() {
                        root.editor.setEntry(index, conditionEdit.text, resultEdit.text);
                    }

                    width: ListView.view.width
                    height: shown ? row.implicitHeight : 0
                    visible: shown
                    color: selected ? Kirigami.Theme.highlightColor : "transparent"

                    TapHandler {
                        onTapped: (eventPoint, button) => root.select(entry.index, (eventPoint.event.modifiers & Qt.ControlModifier) !== 0)
                    }

                    RowLayout {
                        id: row

                        anchors.left: parent.left
                        anchors.right: parent.right

                        QQC2.TextField {
                            id: conditionEdit

                            objectName: "condition"
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            text: entry.condition
                            onActiveFocusChanged: if (activeFocus) root.select(entry.index, false)
                            onEditingFinished: entry.commit()
                        }
                        QQC2.TextField {
                            id: resultEdit

                            objectName: "result"
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            text: entry.result
                            onActiveFocusChanged: if (activeFocus) root.select(entry.index, false)
                            onEditingFinished: entry.commit()
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        QQC2.Button {
            objectName: "addEntryButton"
            text: i18n("Add")
            icon.name: "list-add"
            onClicked: {
                root.editor.addEntry();
                // make sure user can see new row
                entries.positionViewAtEnd();
            }
        }
        QQC2.Button {
            objectName: "removeEntryButton"
            text: i18n("Remove")
            icon.name: "list-remove"
            onClicked: {
                root.editor.removeEntries(root.selectedRows);
                root.selectedRows = [];
            }
        }
    }

    QQC2.GroupBox {
        Layout.fillWidth: true
        title: i18n("Test Area")

        GridLayout {
            anchors.fill: parent
            columns: 2

            QQC2.Label {
                Layout.alignment: Qt.AlignRight
                text: i18n("Input:")
            }
            QQC2.TextField {
                id: testAreaInputEdit

                objectName: "testAreaInputEdit"
                Layout.fillWidth: true
                // A key press shows the entry it matches, or its text.
                Keys.onPressed: event => {
                    const shown = root.editor.testKey(event.key, event.modifiers, event.text);
                    testAreaInputEdit.text = shown.input;
                    testAreaOutputEdit.text = shown.output;
                    event.accepted = true;
                }
            }
            QQC2.Label {
                Layout.alignment: Qt.AlignRight
                text: i18n("Output:")
            }
            QQC2.TextField {
                id: testAreaOutputEdit

                objectName: "testAreaOutputEdit"
                Layout.fillWidth: true
            }
        }
    }
}
