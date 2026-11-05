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
    // KeyBindingEditor::sizeHint(): a share of the window it's opened from. QmlDialogs sets
    // the transient parent after creating the dialog, before centring it on that window.
    onTransientParentChanged: {
        if (transientParent) {
            width = Math.round(transientParent.width * 0.9);
            height = Math.round(transientParent.height * 0.95);
        }
    }

    // A QFormLayout: the style right-aligns its labels.
    GridLayout {
        Layout.fillWidth: true
        columns: 2

        QQC2.Label {
            Layout.alignment: Qt.AlignRight
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
            Layout.alignment: Qt.AlignRight
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
        // QTableWidget's size hint, which sizes the editor when it has no parent.
        Layout.preferredWidth: 256
        Layout.preferredHeight: 192
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

            // QHeaderView: centred titles, the columns stretched equally.
            RowLayout {
                Layout.fillWidth: true
                spacing: 0

                Repeater {
                    model: [i18n("Key Combination"), i18n("Output")]
                    delegate: QQC2.Label {
                        required property string modelData

                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        padding: Kirigami.Units.smallSpacing
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData
                    }
                }
            }
            Kirigami.Separator {
                Layout.fillWidth: true
            }

            // Plain cells with grid lines, a row selected as a whole (SelectRows). A cell is
            // edited in place on a double click, or when a key is typed while it is current
            // (QTableView's DoubleClicked|AnyKeyPressed edit triggers).
            ListView {
                id: entries

                // The cell keys go to and the one being edited, -1 when none.
                property int currentColumn: 0
                property int editingRow: -1
                property int editingColumn: -1
                property string initialText

                function edit(row, column, text) {
                    initialText = text;
                    editingColumn = column;
                    editingRow = row;
                }

                objectName: "keyBindingEntries"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                focus: true
                model: root.editor
                QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

                Keys.onPressed: event => {
                    if (editingRow < 0 && currentIndex >= 0 && event.text.length > 0 && event.text.trim().length > 0
                        && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))) {
                        edit(currentIndex, currentColumn, event.text);
                        event.accepted = true;
                    }
                }

                delegate: Rectangle {
                    id: entry

                    required property int index
                    required property string condition
                    required property string result
                    // QTableWidget::findItems(text, Qt::MatchContains): either cell, any case.
                    readonly property bool shown: [condition, result].some(text => text.toLowerCase().includes(filterEdit.text.toLowerCase()))
                    readonly property bool selected: root.selectedRows.includes(index)
                    readonly property color gridColor: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15)

                    function commit(column, text) {
                        root.editor.setEntry(index, column === 0 ? text : condition, column === 1 ? text : result);
                        entries.editingRow = -1;
                        entries.forceActiveFocus();
                    }

                    width: ListView.view.width
                    height: shown ? Math.ceil(cellMetrics.height) + 2 * Kirigami.Units.smallSpacing + 1 : 0
                    visible: shown
                    color: selected ? Kirigami.Theme.highlightColor : "transparent"

                    FontMetrics {
                        id: cellMetrics
                    }

                    Row {
                        anchors.fill: parent

                        Repeater {
                            model: 2
                            delegate: Item {
                                id: cell

                                required property int index
                                readonly property string text: index === 0 ? entry.condition : entry.result
                                readonly property bool editing: entries.editingRow === entry.index && entries.editingColumn === index

                                width: entry.width / 2
                                height: entry.height

                                QQC2.Label {
                                    anchors.fill: parent
                                    anchors.bottomMargin: 1
                                    leftPadding: Kirigami.Units.smallSpacing
                                    rightPadding: Kirigami.Units.smallSpacing
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    visible: !cell.editing
                                    color: entry.selected ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                    text: cell.text
                                }
                                // The grid: a line under the row and between the columns.
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 1
                                    color: entry.gridColor
                                }
                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.right: parent.right
                                    width: 1
                                    color: entry.gridColor
                                    visible: cell.index === 0
                                }
                                TapHandler {
                                    id: cellTap

                                    onTapped: {
                                        root.select(entry.index, (cellTap.point.modifiers & Qt.ControlModifier) !== 0);
                                        entries.currentIndex = entry.index;
                                        entries.currentColumn = cell.index;
                                        entries.forceActiveFocus();
                                    }
                                    onDoubleTapped: entries.edit(entry.index, cell.index, cell.text)
                                }
                                Loader {
                                    anchors.fill: parent
                                    active: cell.editing
                                    sourceComponent: QQC2.TextField {
                                        text: entries.initialText
                                        Component.onCompleted: {
                                            forceActiveFocus();
                                            cursorPosition = text.length;
                                        }
                                        Keys.onEscapePressed: event => {
                                            entries.editingRow = -1;
                                            entries.forceActiveFocus();
                                        }
                                        onAccepted: entry.commit(cell.index, text)
                                        onActiveFocusChanged: {
                                            if (!activeFocus && cell.editing) {
                                                entry.commit(cell.index, text);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // A QHBoxLayout of two buttons, without a spacer: they share the width.
    RowLayout {
        QQC2.Button {
            objectName: "addEntryButton"
            Layout.fillWidth: true
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
            Layout.fillWidth: true
            text: i18n("Remove")
            icon.name: "list-remove"
            onClicked: {
                root.editor.removeEntries(root.selectedRows);
                root.selectedRows = [];
            }
        }
    }

    // The .ui's 16 px spacer above the test area.
    Item {
        Layout.preferredHeight: 16
    }

    // A flat QGroupBox with no margins: the style draws only its title, bold and centred.
    QQC2.GroupBox {
        id: testArea

        Layout.fillWidth: true
        title: i18n("Test Area")
        padding: 0
        topPadding: label.implicitHeight + spacing
        background: null
        label: QQC2.Label {
            width: testArea.width
            horizontalAlignment: Text.AlignHCenter
            font.bold: true
            text: testArea.title
        }

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
