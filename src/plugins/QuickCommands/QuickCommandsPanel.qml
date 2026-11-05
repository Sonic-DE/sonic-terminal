/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrols as KQuickControls
import org.sonicde.terminal

// The Quick Commands panel: the quick access shortcut, the filter, the tree of
// commands, the form while adding or editing, the Command and Warnings tabs and the buttons.
ColumnLayout {
    id: root

    required property QtObject backend
    // viewMode(), addMode(), editMode().
    property bool adding: false
    property bool editing: false
    readonly property bool formShown: adding || editing

    function viewMode() {
        adding = false;
        editing = false;
    }

    function prepareEdit() {
        const group = groupBox.editText;
        groupBox.model = backend.groups;
        groupBox.editText = group;
    }

    function show(data) {
        if (data.name === undefined) {
            return;
        }
        nameField.text = data.name;
        tooltipField.text = data.tooltip;
        commandField.text = data.command;
        groupBox.editText = data.group;
    }

    spacing: Kirigami.Units.smallSpacing
    Layout.minimumWidth: implicitWidth

    Connections {
        target: root.backend
        function onEditRequested() {
            root.editing = true;
            root.prepareEdit();
        }
        function onRenameRequested(index) {
            tree.edit(index);
        }
        function onWarningsChanged() {
            if (warningField.text !== root.backend.warnings) {
                warningField.text = root.backend.warnings;
            }
        }
    }

    // expandAll() the first time it shows.
    property bool expandedOnce: false
    onVisibleChanged: {
        if (visible && !expandedOnce) {
            expandedOnce = true;
            tree.expandAll();
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.smallSpacing
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing

        QQC2.Label {
            text: i18n("Shortcut:")
        }
        KQuickControls.KeySequenceItem {
            objectName: "keySequenceEdit"
            Layout.fillWidth: true
            // QKeySequenceEdit doesn't check for conflicts.
            checkForConflictsAgainst: KQuickControls.ShortcutType.None
            keySequence: root.backend.shortcut
            onKeySequenceModified: root.backend.shortcut = keySequence
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        spacing: 0

        QQC2.TextField {
            objectName: "filterLine"
            Layout.fillWidth: true
            placeholderText: i18n("Filter...")
            onTextChanged: root.backend.setFilter(text)
        }
        QQC2.CheckBox {
            objectName: "invertFilter"
            text: i18n("Invert")
            onClicked: root.backend.setInvertFilter(checked)
        }
    }

    PanelTree {
        id: tree

        objectName: "commandsTreeView"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.preferredHeight: 192
        Layout.minimumHeight: Kirigami.Units.gridUnit * 4
        enabled: !root.formShown
        model: root.backend.model
        onItemClicked: index => {
            root.show(root.backend.selected(index));
            root.backend.runShellCheck(commandField.text);
        }
        onItemDoubleClicked: index => root.backend.invokeCommand(index)
        onContextMenuRequested: (index, position) => root.backend.createMenu(index, position)
    }

    GridLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        columns: 2
        columnSpacing: 6
        visible: root.formShown

        QQC2.Label {
            text: i18n("Title")
        }
        QQC2.TextField {
            id: nameField

            objectName: "name"
            Layout.fillWidth: true
        }
        QQC2.Label {
            text: i18n("Tooltip")
        }
        QQC2.TextField {
            id: tooltipField

            objectName: "tooltip"
            Layout.fillWidth: true
        }
        QQC2.Label {
            text: i18n("Group")
        }
        QQC2.ComboBox {
            id: groupBox

            objectName: "group"
            Layout.fillWidth: true
            editable: true
            model: [i18n("Default")]
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        spacing: 0

        QQC2.TabBar {
            id: tabs

            Layout.fillWidth: true

            QQC2.TabButton {
                objectName: "commandTab"
                width: implicitWidth
                text: i18n("Command")
            }
            QQC2.TabButton {
                objectName: "warningsTab"
                width: implicitWidth
                text: root.backend.warningsTitle
            }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 192
            currentIndex: tabs.currentIndex

            QQC2.ScrollView {
                QQC2.TextArea {
                    id: commandField

                    objectName: "command"
                    // shellCheckTimer
                    onTextChanged: shellCheckTimer.restart()
                }
            }
            QQC2.ScrollView {
                QQC2.TextArea {
                    id: warningField

                    objectName: "warning"
                    Component.onCompleted: text = root.backend.warnings
                    onTextChanged: root.backend.warnings = text
                }
            }
        }
        Timer {
            id: shellCheckTimer

            interval: 250
            onTriggered: root.backend.runShellCheck(commandField.text)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.bottomMargin: Kirigami.Units.largeSpacing

        Item {
            Layout.fillWidth: true
        }
        QQC2.Button {
            objectName: "btnRun"
            icon.name: "system-run"
            text: i18n("Run")
            onClicked: root.backend.runCommand(commandField.text)
        }
        QQC2.Button {
            objectName: "btnAdd"
            icon.name: "list-add"
            text: i18n("Add")
            visible: !root.formShown
            onClicked: {
                root.adding = true;
                root.prepareEdit();
            }
        }
        QQC2.Button {
            objectName: "btnSave"
            icon.name: "document-save"
            text: i18n("Save")
            visible: root.adding
            onClicked: {
                if (root.backend.saveCommand(nameField.text, tooltipField.text, commandField.text, groupBox.editText)) {
                    root.viewMode();
                }
            }
        }
        QQC2.Button {
            objectName: "btnUpdate"
            icon.name: "view-refresh"
            text: i18n("Update")
            visible: root.editing
            onClicked: {
                if (root.backend.updateCommand(tree.currentIndex, nameField.text, tooltipField.text, commandField.text, groupBox.editText)) {
                    root.viewMode();
                }
            }
        }
        QQC2.Button {
            objectName: "btnCancel"
            icon.name: "dialog-cancel"
            text: i18n("Cancel")
            visible: root.formShown
            onClicked: root.viewMode()
        }
    }
}
