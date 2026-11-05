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

// The SSH Manager panel: the quick access shortcut, the filter, Manage profile,
// the tree of entries, the error message, the entry's form while adding or editing, and the
// buttons.
ColumnLayout {
    id: root

    required property QtObject backend
    // showInfoPane(); while it's shown the tree is disabled, as a click would lose the form.
    property bool paneShown: false
    // Adding, or editing the tree's current entry.
    property bool editing: false
    // handleImportedData(): the imported entry's fields stay read only.
    property bool imported: false
    property string errorText
    property bool hasSelection: false
    property bool canEdit: false
    property bool canDelete: true
    property string deleteToolTip: i18n("Delete selected SSH entry")

    function fields() {
        return {
            name: nameField.text,
            host: hostField.text,
            port: portField.text,
            sshKey: keyField.text,
            username: userField.text,
            profileName: profileBox.currentText,
            useSshConfig: useConfig.checked,
            folder: editing ? "not-used-here" : folderBox.editText,
            imported: imported
        };
    }

    function clearInfo() {
        paneShown = false;
        errorText = "";
        nameField.text = "";
        hostField.text = "";
        portField.text = "22";
        keyField.text = "";
    }

    function showPane() {
        keyField.text = "";
        folderBox.model = backend.folders;
        imported = false;
        editing = false;
        paneShown = true;
    }

    function editEntry() {
        const data = backend.entry(tree.currentIndex);
        if (data.name === undefined) {
            return;
        }
        clearInfo();
        showPane();
        hostField.text = data.host;
        nameField.text = data.name;
        portField.text = data.port;
        keyField.text = data.sshKey;
        const profile = profileBox.find(data.profileName);
        profileBox.currentIndex = profile >= 0 ? profile : 0;
        userField.text = data.username;
        useConfig.checked = data.useSshConfig;
        editing = true;
        imported = data.imported;
        if (imported) {
            errorText = i18n("Imported SSH Profile <br/> Some settings are read only.");
        }
    }

    function submit() {
        const error = backend.checkFields(fields());
        if (error.length > 0) {
            errorText = error;
            return;
        }
        if (editing) {
            backend.saveEdit(tree.currentIndex, fields());
        } else {
            backend.addEntry(fields());
        }
        clearInfo();
    }

    spacing: Kirigami.Units.smallSpacing
    Layout.minimumWidth: implicitWidth

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
            text: i18n("Shortcut")
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

        ClearableTextField {
            objectName: "filterText"
            Layout.fillWidth: true
            placeholderText: i18n("Filter...")
            onTextChanged: root.backend.setFilter(text)
        }
        QQC2.CheckBox {
            objectName: "btnInvertFilter"
            text: i18n("Invert")
            QQC2.ToolTip.text: i18n("Show entries that don't match the filter")
            QQC2.ToolTip.visible: hovered
            onClicked: root.backend.setInvertFilter(checked)
        }
    }

    QQC2.CheckBox {
        objectName: "btnManageProfile"
        Layout.leftMargin: Kirigami.Units.largeSpacing
        text: i18n("Manage profile")
        checked: root.backend.manageProfile
        QQC2.ToolTip.text: i18n("Change profile on connect/disconnect")
        QQC2.ToolTip.visible: hovered
        onClicked: root.backend.manageProfile = checked
    }

    PanelTree {
        id: tree

        objectName: "treeView"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.preferredHeight: 192
        Layout.minimumHeight: Kirigami.Units.gridUnit * 4
        enabled: !root.paneShown
        model: root.backend.model
        canEdit: index => root.backend.canEdit(index)
        // handleTreeClick()
        onItemClicked: (index, button) => {
            const actions = root.backend.rowActions(index);
            root.hasSelection = true;
            root.canEdit = actions.canEdit;
            root.canDelete = actions.canDelete;
            root.deleteToolTip = actions.deleteToolTip;
            if (button === Qt.MiddleButton) {
                root.backend.connectInNewTab(index);
            }
        }
        onItemDoubleClicked: index => root.backend.connectTo(index)
        onContextMenuRequested: (index, position) => root.backend.createMenu(index, position)
    }

    // KMessageWidget
    Kirigami.InlineMessage {
        objectName: "errorPanel"
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        visible: false
        showCloseButton: true
        type: Kirigami.MessageType.Information
        text: root.errorText
        // Its close button sets visible, so no binding.
        onTextChanged: visible = text.length > 0
        onVisibleChanged: {
            if (!visible) {
                root.errorText = "";
            }
        }
    }

    GridLayout {
        objectName: "sshInfoPane"
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        columns: 2
        visible: root.paneShown

        QQC2.Label {
            text: i18n("Identifier")
        }
        QQC2.TextField {
            id: nameField

            objectName: "name"
            Layout.fillWidth: true
            placeholderText: i18n("The name of this entry on the list")
            QQC2.ToolTip.text: i18n("A name for this ssh entry")
            QQC2.ToolTip.visible: hovered
        }
        QQC2.Label {
            text: i18n("SSH Hostname")
        }
        QQC2.TextField {
            id: hostField

            objectName: "hostname"
            Layout.fillWidth: true
            enabled: !root.imported
            placeholderText: i18n("The hostname of the server")
            QQC2.ToolTip.text: placeholderText
            QQC2.ToolTip.visible: hovered
        }
        QQC2.Label {
            text: i18n("Port")
        }
        QQC2.TextField {
            id: portField

            objectName: "port"
            Layout.fillWidth: true
            enabled: !root.imported
            text: "22"
            // full port range, users can setup ssh on any port they want
            validator: IntValidator {
                bottom: 0
                top: 65535
            }
            QQC2.ToolTip.text: i18n("The port of the server, 22 is the default for ssh connections")
            QQC2.ToolTip.visible: hovered
        }
        QQC2.Label {
            text: i18n("SSH Key")
        }
        RowLayout {
            Layout.fillWidth: true

            QQC2.TextField {
                id: keyField

                objectName: "sshkey"
                Layout.fillWidth: true
                enabled: !root.imported
                placeholderText: i18n("The private key")
                QQC2.ToolTip.text: i18n("The private key to be used for this connection")
                QQC2.ToolTip.visible: hovered
            }
            QQC2.ToolButton {
                objectName: "btnFindSshKey"
                icon.name: "folder-open"
                text: i18n("...")
                display: QQC2.AbstractButton.IconOnly
                onClicked: {
                    const file = root.backend.chooseSshKey();
                    if (file.length > 0) {
                        keyField.text = file;
                    }
                }
            }
        }
        Item {
            implicitWidth: 1
        }
        QQC2.CheckBox {
            id: useConfig

            objectName: "useSshConfig"
            enabled: !root.imported
            text: i18n("Use ~/.ssh/config for key")
            QQC2.ToolTip.text: i18n("If this is set, we don't rely on the SSH key set above, but on the configuration on ~/.ssh/config")
            QQC2.ToolTip.visible: hovered
        }
        QQC2.Label {
            text: i18n("Username")
        }
        QQC2.TextField {
            id: userField

            objectName: "username"
            Layout.fillWidth: true
            enabled: !root.imported
            placeholderText: i18n("Your username")
            QQC2.ToolTip.text: i18n("The username for the connection")
            QQC2.ToolTip.visible: hovered
        }
        // For adding only: to edit the folder, the user will drag & drop.
        QQC2.Label {
            visible: !root.editing
            text: i18n("Folder")
        }
        QQC2.ComboBox {
            id: folderBox

            objectName: "folder"
            Layout.fillWidth: true
            visible: !root.editing
            editable: true
            model: root.backend.folders
            QQC2.ToolTip.text: i18n("For organizational purposes, this entry will be inside of this folder.")
            QQC2.ToolTip.visible: hovered
        }
        QQC2.Label {
            text: i18n("Profile")
        }
        QQC2.ComboBox {
            id: profileBox

            objectName: "profile"
            Layout.fillWidth: true
            model: root.backend.profiles
            QQC2.ToolTip.text: i18n("The profile to be active when the ssh session starts")
            QQC2.ToolTip.visible: hovered
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
            objectName: "newSSHConfig"
            icon.name: "list-add"
            visible: !root.paneShown
            QQC2.ToolTip.text: i18n("Add a new SSH Entry")
            QQC2.ToolTip.visible: hovered
            onClicked: root.showPane()
        }
        QQC2.Button {
            objectName: "btnDelete"
            icon.name: "edit-delete"
            visible: !root.paneShown
            enabled: root.canDelete
            QQC2.ToolTip.text: root.deleteToolTip
            QQC2.ToolTip.visible: hovered
            onClicked: root.backend.triggerDelete(tree.currentIndex)
        }
        QQC2.Button {
            objectName: "btnEdit"
            text: i18n("Edit")
            visible: !root.paneShown
            // Nothing selected, nothing to edit.
            enabled: root.hasSelection && root.canEdit
            QQC2.ToolTip.text: i18n("Edit selected entry")
            QQC2.ToolTip.visible: hovered
            onClicked: root.editEntry()
        }
        QQC2.Button {
            objectName: "btnAdd"
            text: root.editing ? i18n("Update") : i18n("Add")
            visible: root.paneShown
            QQC2.ToolTip.text: i18n("Add new SSH Configuration")
            QQC2.ToolTip.visible: hovered
            onClicked: root.submit()
        }
        QQC2.Button {
            objectName: "btnCancel"
            text: i18n("Cancel")
            visible: root.paneShown
            QQC2.ToolTip.text: i18n("Cancel Add or Edit")
            QQC2.ToolTip.visible: hovered
            onClicked: root.clearInfo()
        }
    }
}
