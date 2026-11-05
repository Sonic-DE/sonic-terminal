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

// The Edit Profile dialog's Mouse page (EditProfileMousePage.ui): Text interaction and
// Miscellaneous tabs.
DialogPage {
    id: root

    required property ProfileEditor editor

    title: i18n("Mouse")
    iconName: "input-mouse"

    component SectionLabel: QQC2.Label {
        Layout.alignment: Qt.AlignRight
    }
    component Choice: ProfileRadioButton {
        editor: root.editor
    }
    component Setting: ProfileCheckBox {
        editor: root.editor
    }
    component Gap: Item {
        Layout.columnSpan: 2
        Layout.preferredHeight: Kirigami.Units.largeSpacing
    }

    QQC2.TabBar {
        id: tabs

        objectName: "tabWidget"
        Layout.fillWidth: true

        QQC2.TabButton {
            text: i18n("Text interaction")
        }
        QQC2.TabButton {
            text: i18n("Miscellaneous")
        }
    }

    StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: tabs.currentIndex

        GridLayout {
            objectName: "tab"
            columns: 2

            MnemonicLabel {
                Layout.alignment: Qt.AlignRight
                label: i18n("Word characters:")
                buddy: wordCharacterEdit
                QQC2.ToolTip.text: i18n("Characters which are considered part of a word when double-clicking to select whole words in the terminal.")
            }
            QQC2.TextField {
                id: wordCharacterEdit

                objectName: "wordCharacterEdit"
                Layout.fillWidth: true
                text: root.editor.value("WordCharacters")
                onTextEdited: root.editor.setValue("WordCharacters", text)
                QQC2.ToolTip.text: i18n("Characters which are considered part of a word when double-clicking to select whole words in the terminal.")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            }

            Gap {}

            SectionLabel {
                text: i18n("Triple-click selects:")
            }
            Choice {
                objectName: "tripleClickSelectsTheWholeLine"
                profileProperty: "TripleClickMode"
                value: 0
                text: i18n("&The whole line")
            }
            Item {}
            Choice {
                objectName: "tripleClickSelectsFromMousePosition"
                profileProperty: "TripleClickMode"
                value: 1
                text: i18n("From mouse position to the end of &line")
            }

            Gap {}

            SectionLabel {
                text: i18n("Middle-click pastes:")
            }
            Choice {
                objectName: "pasteFromClipboardButton"
                profileProperty: "MiddleClickPasteMode"
                value: 1
                text: i18n("From clipboard")
            }
            Item {}
            Choice {
                objectName: "pasteFromX11SelectionButton"
                profileProperty: "MiddleClickPasteMode"
                value: 0
                text: i18n("From selection")
            }

            Gap {}

            SectionLabel {
                text: i18n("Copy options:")
            }
            Setting {
                objectName: "copyTextToClipboardButton"
                profileProperty: "AutoCopySelectedText"
                text: i18n("Copy on select")
                toolTip: i18n("Automatically copy selected text into clipboard")
            }
            Item {}
            Setting {
                objectName: "copyTextAsHTMLButton"
                profileProperty: "CopyTextAsHTML"
                text: i18n("Copy text as HTML")
                toolTip: i18n("Copy text as HTML (including formatting, font faces, colors... etc)")
            }
            Item {}
            Setting {
                objectName: "trimLeadingSpacesButton"
                profileProperty: "TrimLeadingSpacesInSelectedText"
                text: i18n("Trim leading spaces")
                toolTip: i18n("Trim leading spaces in selected text, useful in some instances")
            }
            Item {}
            Setting {
                objectName: "trimTrailingSpacesButton"
                profileProperty: "TrimTrailingSpacesInSelectedText"
                text: i18n("Trim trailing spaces")
                toolTip: i18n("Trim trailing spaces in selected text, useful in some instances")
            }

            Item {
                Layout.fillHeight: true
            }
        }

        ColumnLayout {
            objectName: "tab_2"

            Setting {
                id: underlineLinksButton

                objectName: "underlineLinksButton"
                profileProperty: "UnderlineLinksEnabled"
                text: i18n("Underline links")
                toolTip: i18n("Text recognized as a link or an email address will be underlined when hovered by the mouse pointer.")
            }
            Setting {
                id: underlineFilesButton

                objectName: "underlineFilesButton"
                profileProperty: "UnderlineFilesEnabled"
                text: i18n("Underline files")
                toolTip: i18n("Text recognized as a file will be underlined when hovered by the mouse pointer.")
            }
            RowLayout {
                MnemonicLabel {
                    label: i18n("Text Editor Command: ")
                    buddy: textEditorCombo
                    QQC2.ToolTip.text: i18n("Text editor to use when opening text file URLs at a given line/column.")
                }
                QQC2.ComboBox {
                    id: textEditorCombo

                    objectName: "textEditorCombo"
                    Layout.fillWidth: true
                    model: root.editor.textEditors
                    textRole: "text"
                    Component.onCompleted: currentIndex = root.editor.value("TextEditorCmd")
                    onActivated: index => root.editor.setValue("TextEditorCmd", index)
                    QQC2.ToolTip.text: i18n("Text editor to use when opening text file URLs at a given line/column.")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval

                    // Editors not installed look disabled, as the widget's model items.
                    delegate: QQC2.ItemDelegate {
                        required property var modelData
                        required property int index

                        width: textEditorCombo.width
                        text: modelData.text
                        icon.name: modelData.iconName
                        enabled: modelData.enabled
                        highlighted: textEditorCombo.highlightedIndex === index
                    }
                }
                QQC2.Button {
                    id: textEditorCustomBtn

                    objectName: "textEditorCustomBtn"
                    text: i18n("Edit...")
                    icon.name: "document-edit"
                    // Enum::CustomTextEditor
                    enabled: textEditorCombo.currentIndex === 6
                    onClicked: root.editor.editCustomTextEditor(textEditorCustomBtn)
                }
                Item {
                    Layout.fillWidth: true
                }
            }
            Setting {
                objectName: "openLinksByDirectClickButton"
                profileProperty: "OpenLinksByDirectClickEnabled"
                enabled: underlineLinksButton.checked || underlineFilesButton.checked
                text: i18n("Open files/links by direct click")
                toolTip: i18n("Text recognized as a file, link or an email address can be opened by direct mouse click.")
            }

            Setting {
                id: allowLinkEscapeSequenceButton

                objectName: "allowLinkEscapeSequenceButton"
                profileProperty: "AllowEscapedLinks"
                text: i18n("Allow escape sequences for links")
            }
            QQC2.Label {
                objectName: "allowLinkEscapeSequenceButtonWarning"
                Layout.fillWidth: true
                // Wrapped: its one-line width mustn't widen the dialog.
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                wrapMode: Text.WordWrap
                text: i18n("<b>WARNING</b>: This has security implications as it allows malicious URLs to be shown as another URL or hidden.<br>Make sure you understand the implications before turning this on.")
            }
            RowLayout {
                QQC2.Label {
                    text: i18n("Allowed link formats: ")
                }
                QQC2.TextField {
                    objectName: "linkEscapeSequenceTexts"
                    Layout.fillWidth: true
                    enabled: allowLinkEscapeSequenceButton.checked
                    text: root.editor.value("EscapedLinksSchema")
                    validator: RegularExpressionValidator {
                        regularExpression: /(([a-z]*:\/\/;)*([A-Za-z*]:\/\/))/
                    }
                    onTextEdited: root.editor.setValue("EscapedLinksSchema", text)
                    QQC2.ToolTip.text: i18n("The formats of possible links, like http://, https:// and file://")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                }
            }

            Setting {
                objectName: "ctrlRequiredForDragButton"
                profileProperty: "CtrlRequiredForDrag"
                text: i18n("Require Ctrl key for drag && drop")
                toolTip: i18n("Selected text will require control key plus click to drag.")
            }
            Setting {
                objectName: "enableAlternateScrollingButton"
                profileProperty: "AlternateScrolling"
                text: i18n("Enable Alternate Screen buffer scrolling")
                toolTip: i18n("Mouse scroll wheel will emulate up/down key presses in programs that use the Alternate Screen buffer (e.g. less)")
            }
            Setting {
                objectName: "dropUrlsAsText"
                profileProperty: "DropUrlsAsText"
                text: i18n("Disable drag && drop menu for files && URLs")
                toolTip: i18n("Always paste dropped files and URLs as text without offering move, copy and link actions.")
            }
            Setting {
                objectName: "enableMouseWheelZoomButton"
                profileProperty: "MouseWheelZoomEnabled"
                text: i18n("Allow Ctrl+scrollwheel to zoom text size")
                toolTip: i18n("Pressing Ctrl+scrollwheel will increase/decrease the text size.")
            }
            Setting {
                objectName: "allowColorFilters"
                profileProperty: "ColorFilterEnabled"
                text: i18n("Preview Colors on hover")
                toolTip: i18n("When positioning the mouse over a hexadecimal color it will be displayed in a frame next to the mouse pointer")
            }
            Item {
                Layout.fillHeight: true
            }
            Setting {
                objectName: "allowMouseTrackingButton"
                profileProperty: "AllowMouseTracking"
                text: i18n("Allow terminal applications to handle clicks and drags")
                toolTip: i18n("Some terminal applications have mouse support, this allows them to get it by default. It can be toggled on/off while running as well.")
            }
        }
    }
}
