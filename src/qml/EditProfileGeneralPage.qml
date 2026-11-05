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

// The Edit Profile dialog's General page (EditProfileGeneralPage.ui): General Settings and
// Semantic Integration tabs, with the name check's message above them.
DialogPage {
    id: root

    required property ProfileEditor editor
    property alias nameEdit: profileNameEdit

    function selectProfileName() {
        tabs.currentIndex = 0;
        profileNameEdit.forceActiveFocus();
        profileNameEdit.selectAll();
    }

    title: i18nc("@title:tab Generic, common options", "General")
    iconName: "utilities-terminal"

    component SectionLabel: QQC2.Label {
        Layout.alignment: Qt.AlignRight
    }
    component Setting: ProfileCheckBox {
        editor: root.editor
    }
    component Choice: ProfileRadioButton {
        editor: root.editor
    }
    component Gap: Item {
        Layout.columnSpan: 3
        Layout.preferredHeight: Kirigami.Units.largeSpacing
    }
    // Never, When URL hints show, Always (Enum::HintsNever, HintsURL, HintsAlways).
    component HintChoices: ColumnLayout {
        required property string profileProperty
        required property string namePrefix

        spacing: 0

        Choice {
            objectName: parent.namePrefix + "Never"
            profileProperty: parent.profileProperty
            value: 0
            text: i18n("Never")
        }
        Choice {
            objectName: parent.namePrefix + "URL"
            profileProperty: parent.profileProperty
            value: 1
            text: i18n("When URL hints show")
        }
        Choice {
            objectName: parent.namePrefix + "Always"
            profileProperty: parent.profileProperty
            value: 2
            text: i18n("Always")
        }
    }

    Connections {
        target: root.editor

        function onMessageChanged() {
            message.visible = true;
            tabs.currentIndex = 0;
        }
        function onNameReverted() {
            profileNameEdit.text = root.editor.value("Name");
            root.selectProfileName();
        }
    }

    Kirigami.InlineMessage {
        id: message

        objectName: "generalPageMessageWidget"
        Layout.fillWidth: true
        // Wrapped: its one-line width mustn't widen the dialog.
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        type: Kirigami.MessageType.Error
        showCloseButton: true
        text: root.editor.message
        visible: false
    }

    QQC2.TabBar {
        id: tabs

        objectName: "tabWidget"
        Layout.fillWidth: true

        QQC2.TabButton {
            text: i18n("General Settings")
        }
        QQC2.TabButton {
            text: i18n("Semantic Integration")
        }
    }

    StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: tabs.currentIndex

        GridLayout {
            objectName: "general"
            columns: 3

            QQC2.Button {
                id: iconSelectButton

                objectName: "iconSelectButton"
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                icon.name: root.editor.value("Icon")
                icon.width: Kirigami.Units.iconSizes.large
                icon.height: Kirigami.Units.iconSizes.large
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Select the icon displayed on tabs using this profile")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                onClicked: {
                    root.editor.chooseIcon(iconSelectButton);
                    icon.name = root.editor.value("Icon");
                }
            }
            ColumnLayout {
                Layout.columnSpan: 2
                Layout.fillWidth: true

                ClearableTextField {
                    id: profileNameEdit

                    objectName: "profileNameEdit"
                    Layout.fillWidth: true
                    placeholderText: i18n("Profile name")
                    enabled: root.editor.nameEditable
                    text: root.editor.nameEditable ? root.editor.value("Name") : root.editor.groupNames()
                    QQC2.ToolTip.text: i18n("A descriptive name for the profile")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    // As QLineEdit::textChanged: also the clear button.
                    onTextChanged: {
                        if (root.editor.nameEditable && text !== root.editor.value("Name")) {
                            root.editor.setValue("Name", text);
                        }
                    }
                }
                QQC2.CheckBox {
                    objectName: "setAsDefaultButton"
                    text: root.editor.defaultProfileText
                    Component.onCompleted: checked = root.editor.setAsDefault
                    onToggled: root.editor.setAsDefault = checked
                }
            }

            Gap {}

            MnemonicLabel {
                Layout.alignment: Qt.AlignRight
                label: i18n("Command:")
                buddy: commandEdit
            }
            QQC2.TextField {
                id: commandEdit

                objectName: "commandEdit"
                Layout.columnSpan: 2
                Layout.fillWidth: true
                placeholderText: i18n("/bin/sh")
                text: root.editor.commandLine()
                QQC2.ToolTip.text: i18n("The command to execute when new terminal sessions are created using this profile")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                onTextEdited: root.editor.setCommandLine(text)
            }

            MnemonicLabel {
                Layout.alignment: Qt.AlignRight
                label: i18n("I&nitial directory:")
                buddy: initialDirEdit
            }
            ClearableTextField {
                id: initialDirEdit

                objectName: "initialDirEdit"
                Layout.fillWidth: true
                placeholderText: root.editor.homeDirectory
                text: root.editor.value("Directory")
                QQC2.ToolTip.text: i18n("The initial working directory for new terminal sessions using this profile")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                onTextChanged: {
                    if (text !== root.editor.value("Directory")) {
                        root.editor.setValue("Directory", text);
                    }
                }
            }
            QQC2.ToolButton {
                id: dirSelectButton

                objectName: "dirSelectButton"
                text: i18n("...")
                icon.name: "folder-open"
                display: QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.text: i18n("Choose the initial directory")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                onClicked: {
                    root.editor.chooseInitialDirectory(dirSelectButton);
                    initialDirEdit.text = root.editor.value("Directory");
                }
            }

            Item {}
            Setting {
                objectName: "startInSameDirButton"
                Layout.columnSpan: 2
                profileProperty: "StartInCurrentSessionDir"
                text: i18n("Start in same directory as current session")
            }

            Item {}
            Setting {
                objectName: "inheritContainerButton"
                Layout.columnSpan: 2
                profileProperty: "InheritContainerContext"
                enabled: root.editor.containersEnabled
                text: i18n("Enter same container as current session")
                toolTip: i18n("When opening a new tab or split, automatically enter the same container (Toolbox, Distrobox) as the current session")
            }

            Item {
                visible: containerWarning.visible
            }
            Kirigami.InlineMessage {
                id: containerWarning

                objectName: "containerWarningWidget"
                Layout.columnSpan: 2
                Layout.fillWidth: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                type: Kirigami.MessageType.Warning
                text: root.editor.containersDisabledReason
                visible: !root.editor.containersEnabled
            }

            MnemonicLabel {
                Layout.alignment: Qt.AlignRight
                label: i18n("Always start in container:")
                buddy: containerCombo
            }
            QQC2.ComboBox {
                id: containerCombo

                objectName: "containerCombo"
                Layout.columnSpan: 2
                Layout.fillWidth: true
                enabled: root.editor.containersEnabled
                model: root.editor.containers
                textRole: "text"
                Component.onCompleted: {
                    const current = root.editor.value("ContainerName");
                    currentIndex = Math.max(0, model.findIndex(container => container.key === current));
                }
                onActivated: index => root.editor.setValue("ContainerName", model[index].key)
                QQC2.ToolTip.text: i18n("Always start new sessions using this profile inside the selected container")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            }

            SectionLabel {
                text: i18n("Environment:")
            }
            QQC2.Button {
                id: environmentEditButton

                objectName: "environmentEditButton"
                Layout.columnSpan: 2
                text: i18n("Edit...")
                icon.name: "document-edit"
                QQC2.ToolTip.text: i18n("Edit the list of environment variables and associated values")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                onClicked: root.editor.editEnvironment(environmentEditButton)
            }

            Gap {}

            MnemonicLabel {
                Layout.alignment: Qt.AlignRight
                label: i18n("Initial terminal si&ze:")
                buddy: terminalColumnsEntry
            }
            RowLayout {
                Layout.columnSpan: 2

                QQC2.SpinBox {
                    id: terminalColumnsEntry

                    // KPluralHandlingSpinBox's suffix, for the current value.
                    readonly property string suffix: i18ncp("Suffix of the number of columns (N columns). The leading space is needed to separate it from the number value.", " column", " columns", value)

                    objectName: "terminalColumnsEntry"
                    editable: true
                    from: 1
                    to: 999
                    textFromValue: (value, locale) => Number(value).toLocaleString(locale, "f", 0) + i18ncp("Suffix of the number of columns (N columns). The leading space is needed to separate it from the number value.", " column", " columns", value)
                    valueFromText: (text, locale) => Number.fromLocaleString(locale, text.replace(/[^\d.,\s-]/g, "").trim())
                    Component.onCompleted: value = root.editor.value("TerminalColumns")
                    onValueModified: root.editor.setValue("TerminalColumns", value)
                }
                QQC2.SpinBox {
                    id: terminalRowsEntry

                    readonly property string suffix: i18ncp("Suffix of the number of rows (N rows). The leading space is needed to separate it from the number value.", " row", " rows", value)

                    objectName: "terminalRowsEntry"
                    editable: true
                    from: 1
                    to: 999
                    textFromValue: (value, locale) => Number(value).toLocaleString(locale, "f", 0) + i18ncp("Suffix of the number of rows (N rows). The leading space is needed to separate it from the number value.", " row", " rows", value)
                    valueFromText: (text, locale) => Number.fromLocaleString(locale, text.replace(/[^\d.,\s-]/g, "").trim())
                    Component.onCompleted: value = root.editor.value("TerminalRows")
                    onValueModified: root.editor.setValue("TerminalRows", value)
                }
            }

            Item {}
            QQC2.Label {
                objectName: "useCurrentWindowSizeNote"
                Layout.columnSpan: 2
                Layout.fillWidth: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                wrapMode: Text.WordWrap
                text: i18n("<html><head/><body><p><span style=\" font-style:italic;\">Settings → Configure Konsole → General → Remember window size</span> must be disabled for these entries to work.</p></body></html>")
            }

            MnemonicLabel {
                Layout.alignment: Qt.AlignRight
                label: i18n("&Terminal bell mode:")
                buddy: terminalBellCombo
            }
            QQC2.ComboBox {
                id: terminalBellCombo

                objectName: "terminalBellCombo"
                Layout.fillWidth: true
                model: [i18n("System Bell"), i18n("System Notifications"), i18n("Visual Bell"), i18n("Ignore Bell Events")]
                Component.onCompleted: currentIndex = root.editor.value("BellMode")
                onActivated: index => root.editor.setValue("BellMode", index)
            }
            Item {}

            Item {
                Layout.fillHeight: true
            }
        }

        GridLayout {
            objectName: "semantic"
            columns: 2

            Setting {
                objectName: "semanticUpDown"
                Layout.columnSpan: 2
                profileProperty: "SemanticUpDown"
                text: i18n("Up/Down Arrows emulation")
            }
            Setting {
                objectName: "semanticInputClick"
                Layout.columnSpan: 2
                profileProperty: "SemanticInputClick"
                text: i18n("Mouse click in input line moves cursor")
            }

            SectionLabel {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: i18n("Semantic hints:")
            }
            HintChoices {
                profileProperty: "SemanticHints"
                namePrefix: "semanticHints"
            }
            SectionLabel {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: i18n("Red error bars:")
            }
            HintChoices {
                profileProperty: "ErrorBars"
                namePrefix: "errorBars"
            }
            SectionLabel {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: i18n("Red error background:")
            }
            HintChoices {
                profileProperty: "ErrorBackground"
                namePrefix: "errorBackground"
            }
            SectionLabel {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: i18n("Alternating bars:")
            }
            HintChoices {
                profileProperty: "AlternatingBars"
                namePrefix: "alternatingBars"
            }
            SectionLabel {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: i18n("Alternating background:")
            }
            HintChoices {
                profileProperty: "AlternatingBackground"
                namePrefix: "alternatingBackground"
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
