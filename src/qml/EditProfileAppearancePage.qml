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

// The Edit Profile dialog's Appearance page (EditProfileAppearancePage.ui): Color scheme &
// font, Cursor, Miscellaneous and Complex Text Layout tabs. Most of its settings show in
// the terminal at once (previews), undone on Cancel.
DialogPage {
    id: root

    required property ProfileEditor editor
    // The list's scheme, as its selection.
    property string selectedScheme: editor.value("ColorScheme")
    // What its buttons may do, again whenever the schemes change (a reset scheme becomes
    // deletable).
    readonly property bool schemeResettable: editor.colorSchemes && editor.canResetColorScheme(selectedScheme)
    readonly property bool schemeDeletable: editor.colorSchemes && editor.isColorSchemeDeletable(selectedScheme)

    function schemeIndex(name) {
        return editor.colorSchemes.findIndex(scheme => scheme.name === name);
    }

    title: i18n("Appearance")
    iconName: "kcolorchooser"

    component SectionLabel: QQC2.Label {
        Layout.alignment: Qt.AlignRight
    }
    component Setting: ProfileCheckBox {
        editor: root.editor
    }
    component Choice: ProfileRadioButton {
        editor: root.editor
    }

    Connections {
        target: root.editor

        function onColorSchemeChosen(name) {
            root.selectedScheme = name;
        }
    }

    QQC2.TabBar {
        id: tabs

        objectName: "tabWidget"
        Layout.fillWidth: true

        QQC2.TabButton {
            text: i18n("Color scheme && font")
        }
        QQC2.TabButton {
            text: i18n("Cursor")
        }
        QQC2.TabButton {
            text: i18n("Miscellaneous")
        }
        QQC2.TabButton {
            text: i18n("Complex Text Layout")
            QQC2.ToolTip.text: i18n("<html><head/><body><p>Complex Text Layout</p></body></html>")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
        }
    }

    StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: tabs.currentIndex

        GridLayout {
            objectName: "colorSchemeTab"
            columns: 3

            Kirigami.InlineMessage {
                objectName: "colorSchemeMessageWidget"
                Layout.columnSpan: 3
                Layout.fillWidth: true
                // Wrapped: its one-line width mustn't widen the dialog.
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                type: Kirigami.MessageType.Warning
                text: root.editor.colorSchemeMessage
                visible: text.length > 0
            }

            QQC2.ScrollView {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                // As tall as the widget page's list (about ten schemes).
                Layout.preferredHeight: Kirigami.Units.gridUnit * 20
                QQC2.ScrollBar.vertical.policy: QQC2.ScrollBar.AlwaysOn
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                Kirigami.Theme.inherit: false

                background: Rectangle {
                    color: Kirigami.Theme.backgroundColor
                    border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2)
                }

                ListView {
                    id: colorSchemeList

                    objectName: "colorSchemeList"
                    clip: true
                    model: root.editor.colorSchemes
                    currentIndex: root.schemeIndex(root.selectedScheme)
                    keyNavigationEnabled: true
                    // The current scheme is scrolled into view, as by QAbstractItemView.
                    Component.onCompleted: positionViewAtIndex(currentIndex, ListView.Contain)
                    onCurrentIndexChanged: {
                        positionViewAtIndex(currentIndex, ListView.Contain);
                        // Keys move the selection, as the widget's list.
                        const name = currentIndex >= 0 ? model[currentIndex].name : "";
                        if (activeFocus && name !== root.selectedScheme) {
                            root.selectedScheme = name;
                            root.editor.selectColorScheme(name);
                        }
                    }

                    delegate: QQC2.ItemDelegate {
                        id: schemeRow

                        required property var modelData
                        required property int index
                        readonly property bool selected: ListView.isCurrentItem
                        // As the widget style draws a view's selection: in the inactive
                        // colours while the view hasn't the focus.
                        readonly property var colors: colorSchemeList.activeFocus ? palette.active : palette.inactive

                        width: ListView.view.width
                        onClicked: {
                            colorSchemeList.forceActiveFocus();
                            root.selectedScheme = modelData.name;
                            root.editor.selectColorScheme(modelData.name);
                        }

                        background: Rectangle {
                            readonly property color highlight: schemeRow.palette.active.highlight

                            color: schemeRow.selected ? schemeRow.colors.highlight
                                : schemeRow.hovered ? Qt.rgba(highlight.r, highlight.g, highlight.b, 0.2) : "transparent"
                        }

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.largeSpacing

                            // ColorSchemeViewDelegate: the scheme's background and text.
                            Rectangle {
                                Layout.preferredWidth: sample.implicitWidth + Kirigami.Units.largeSpacing
                                Layout.fillHeight: true
                                Layout.minimumHeight: sample.implicitHeight + Kirigami.Units.smallSpacing * 2
                                color: schemeRow.modelData.background

                                QQC2.Label {
                                    id: sample

                                    anchors.centerIn: parent
                                    text: i18n("AaZz09...")
                                    font: root.editor.previewFont
                                    color: schemeRow.modelData.foreground
                                }
                            }
                            QQC2.Label {
                                Layout.fillWidth: true
                                text: schemeRow.modelData.description
                                elide: Text.ElideRight
                                color: schemeRow.selected ? schemeRow.colors.highlightedText : schemeRow.colors.text
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                // As wide as its widest button, not more.
                Layout.fillWidth: false

                QQC2.Button {
                    objectName: "newColorSchemeButton"
                    Layout.fillWidth: true
                    text: i18n("New...")
                    icon.name: "list-add"
                    onClicked: root.editor.editColorScheme(root.selectedScheme, true, this)
                    QQC2.ToolTip.text: i18n("Create a new color scheme based upon the selected scheme")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                }
                QQC2.Button {
                    objectName: "editColorSchemeButton"
                    Layout.fillWidth: true
                    text: i18n("Edit...")
                    icon.name: "document-edit"
                    enabled: colorSchemeList.currentIndex >= 0
                    onClicked: root.editor.editColorScheme(root.selectedScheme, false, this)
                    QQC2.ToolTip.text: i18n("Edit the selected color scheme")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                }
                QQC2.Button {
                    objectName: "removeColorSchemeButton"
                    Layout.fillWidth: true
                    text: i18n("Remove")
                    icon.name: "edit-delete"
                    // A scheme that can be reset can't be deleted.
                    enabled: colorSchemeList.currentIndex >= 0 && root.schemeDeletable && !root.schemeResettable
                    onClicked: root.editor.removeColorScheme(root.selectedScheme)
                    QQC2.ToolTip.text: i18n("Delete the selected color scheme")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                }
                QQC2.Button {
                    objectName: "resetColorSchemeButton"
                    Layout.fillWidth: true
                    text: i18n("Defaults")
                    icon.name: "edit-undo"
                    enabled: colorSchemeList.currentIndex >= 0 && root.schemeResettable
                    onClicked: root.editor.resetColorScheme(root.selectedScheme)
                    QQC2.ToolTip.text: i18n("Reset the selected color scheme settings to the default values")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                }
                // KNSWidgets::Button.
                QQC2.Button {
                    objectName: "getNewColorSchemesButton"
                    Layout.fillWidth: true
                    text: i18n("Get New...")
                    icon.name: "get-hot-new-stuff"
                    onClicked: root.editor.getNewColorSchemes(this)
                }
            }

            Kirigami.InlineMessage {
                objectName: "transparencyWarningWidget"
                Layout.columnSpan: 3
                Layout.fillWidth: true
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                type: Kirigami.MessageType.Warning
                text: root.editor.transparencyWarning
                visible: text.length > 0
            }

            SectionLabel {
                text: i18n("Font:")
            }
            QQC2.Label {
                objectName: "fontPreview"
                Layout.fillWidth: true
                Layout.horizontalStretchFactor: 1
                text: root.editor.fontText
                font: root.editor.previewFont
                elide: Text.ElideRight
            }
            QQC2.Button {
                objectName: "chooseFontButton"
                // As wide as the scheme buttons' column, which takes no spare width: the
                // expanding list beside them does.
                Layout.fillWidth: true
                Layout.horizontalStretchFactor: 0
                text: i18n("Choose...")
                icon.name: "preferences-desktop-font"
                onClicked: root.editor.chooseFont(this)
            }

            Item {}
            Setting {
                objectName: "antialiasTextButton"
                Layout.columnSpan: 2
                profileProperty: "AntiAliasFonts"
                preview: true
                text: i18n("Smooth fonts")
            }
            Item {}
            Setting {
                objectName: "boldIntenseButton"
                Layout.columnSpan: 2
                profileProperty: "BoldIntense"
                preview: true
                text: i18n("Draw intense colors in bold font")
            }
            Item {}
            Setting {
                id: useFontLineCharactersButton

                objectName: "useFontLineCharactersButton"
                Layout.columnSpan: 2
                // Profile files' key, misspelled.
                profileProperty: "UseFontLineChararacters"
                preview: true
                text: i18n("Use line characters contained in font")
                toolTip: i18n("Use the selected font for line characters instead of the builtin code")
            }
            Item {}
            Setting {
                objectName: "useFontBrailleCharactersButton"
                Layout.columnSpan: 2
                profileProperty: "UseFontBrailleChararacters"
                preview: true
                enabled: !useFontLineCharactersButton.checked
                text: i18n("Use unicode Braille characters contained in font")
                toolTip: i18n("Use the selected font for unicode Braille characters instead of the builtin code")
            }
        }

        GridLayout {
            objectName: "cursorTab"
            columns: 2

            SectionLabel {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: i18n("Shape:")
            }
            ColumnLayout {
                spacing: 0

                Choice {
                    objectName: "cursorShapeBlock"
                    profileProperty: "CursorShape"
                    value: 0
                    preview: true
                    text: i18n("Block")
                }
                Choice {
                    objectName: "cursorShapeIBeam"
                    profileProperty: "CursorShape"
                    value: 1
                    preview: true
                    text: i18n("I-Beam")
                }
                Choice {
                    objectName: "cursorShapeUnderline"
                    profileProperty: "CursorShape"
                    value: 2
                    preview: true
                    text: i18n("Underline")
                }
            }

            SectionLabel {
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: i18n("Color:")
            }
            ColumnLayout {
                spacing: 0

                Choice {
                    objectName: "autoCursorColorButton"
                    profileProperty: "UseCustomCursorColor"
                    value: false
                    preview: true
                    text: i18n("Match current character")
                    toolTip: i18n("Set the cursor to match the color of the character underneath it.")
                }
                Choice {
                    id: customCursorColorButton

                    objectName: "customCursorColorButton"
                    profileProperty: "UseCustomCursorColor"
                    value: true
                    preview: true
                    text: i18n("Custom cursor color")
                    toolTip: i18n("Use a custom, fixed color for the cursor")
                }
                GridLayout {
                    Layout.leftMargin: Kirigami.Units.gridUnit * 2
                    columns: 2

                    MnemonicLabel {
                        Layout.alignment: Qt.AlignRight
                        label: i18n("Cursor:")
                        buddy: customColorSelectButton
                    }
                    ColorButton {
                        id: customColorSelectButton

                        objectName: "customColorSelectButton"
                        Component.onCompleted: color = root.editor.value("CustomCursorColor")
                        // A custom colour turns on the custom cursor colour.
                        onColorEdited: color => {
                            root.editor.setValue("CustomCursorColor", color, true);
                            customCursorColorButton.click();
                        }
                        QQC2.ToolTip.text: i18n("Select the color used to draw the cursor")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    }
                    MnemonicLabel {
                        Layout.alignment: Qt.AlignRight
                        label: i18n("Text:")
                        buddy: customTextColorSelectButton
                    }
                    ColorButton {
                        id: customTextColorSelectButton

                        objectName: "customTextColorSelectButton"
                        Component.onCompleted: color = root.editor.value("CustomCursorTextColor")
                        onColorEdited: color => {
                            root.editor.setValue("CustomCursorTextColor", color, true);
                            customCursorColorButton.click();
                        }
                        QQC2.ToolTip.text: i18n("Select the color used to draw the character underneath the cursor")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    }
                }
            }

            MnemonicLabel {
                Layout.alignment: Qt.AlignRight
                label: i18n("Blinking:")
                buddy: enableBlinkingCursorButton
            }
            Setting {
                id: enableBlinkingCursorButton

                objectName: "enableBlinkingCursorButton"
                profileProperty: "BlinkingCursorEnabled"
                preview: true
                text: i18n("Enabled")
                toolTip: i18n("Make the cursor blink regularly")
            }
            MnemonicLabel {
                label: i18n("Animation:")
                buddy: enableAnimatingCursorButton
            }
            Setting {
                id: enableAnimatingCursorButton

                objectName: "enableAnimatingCursorButton"
                profileProperty: "AnimatingCursorEnabled"
                preview: true
                text: i18n("Enabled")
                toolTip: i18n("Make the cursor animate smoothly")
            }

            Item {
                Layout.fillHeight: true
            }
        }

        ColumnLayout {
            objectName: "miscTab"

            QQC2.GroupBox {
                Layout.fillWidth: true
                title: i18n("Terminal contents")

                GridLayout {
                    anchors.fill: parent
                    columns: 2

                    MnemonicLabel {
                        Layout.alignment: Qt.AlignRight
                        label: i18n("Line spacing:")
                        buddy: lineSpacingSpinner
                    }
                    QQC2.SpinBox {
                        id: lineSpacingSpinner

                        readonly property string suffix: i18n(" px")

                        objectName: "lineSpacingSpinner"
                        editable: true
                        from: 0
                        to: 20
                        textFromValue: (value, locale) => Number(value).toLocaleString(locale, "f", 0) + suffix
                        valueFromText: (text, locale) => Number.fromLocaleString(locale, text.replace(suffix, "").trim())
                        Component.onCompleted: value = root.editor.value("LineSpacing")
                        onValueModified: root.editor.setValue("LineSpacing", value, true)
                        QQC2.ToolTip.text: i18n("The number of pixels between two lines")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    }
                    MnemonicLabel {
                        Layout.alignment: Qt.AlignRight
                        label: i18n("Margins:")
                        buddy: marginsSpinner
                    }
                    QQC2.SpinBox {
                        id: marginsSpinner

                        readonly property string suffix: i18n(" px")

                        objectName: "marginsSpinner"
                        editable: true
                        from: 0
                        to: 99
                        textFromValue: (value, locale) => Number(value).toLocaleString(locale, "f", 0) + suffix
                        valueFromText: (text, locale) => Number.fromLocaleString(locale, text.replace(suffix, "").trim())
                        Component.onCompleted: value = root.editor.value("TerminalMargin")
                        onValueModified: root.editor.setValue("TerminalMargin", value, true)
                    }
                    MnemonicLabel {
                        Layout.alignment: Qt.AlignRight
                        label: i18n("Align to center:")
                        buddy: alignToCenterButton
                    }
                    Setting {
                        id: alignToCenterButton

                        objectName: "alignToCenterButton"
                        profileProperty: "TerminalCenter"
                        preview: true
                        text: i18n("Enabled")
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    text: i18n("Window:")
                }
                Setting {
                    objectName: "showTerminalSizeHintButton"
                    profileProperty: "ShowTerminalSizeHint"
                    text: i18n("Show hint for terminal size after resizing")
                    toolTip: i18n("Show terminal size in columns and lines in the center of window after resizing")
                }

                Item {}
                RowLayout {
                    Setting {
                        objectName: "displayVerticalLine"
                        profileProperty: "VerticalLine"
                        text: i18n("Display Vertical line at column:")
                    }
                    QQC2.SpinBox {
                        objectName: "displayVerticalLineAtColumn"
                        editable: true
                        from: 0
                        to: 999
                        Component.onCompleted: value = root.editor.value("VerticalLineAtChar")
                        onValueModified: root.editor.setValue("VerticalLineAtChar", value)
                    }
                }

                SectionLabel {
                    text: i18n("Inactive Terminals:")
                }
                Setting {
                    id: dimWhenInactiveCheckbox

                    objectName: "dimWhenInactiveCheckbox"
                    profileProperty: "DimWhenInactive"
                    text: i18n("Darken")
                    toolTip: i18n("Indicate whether the window is active by dimming the colors")
                }
                Item {}
                RowLayout {
                    QQC2.Label {
                        objectName: "dimLabel"
                        enabled: dimWhenInactiveCheckbox.checked
                        text: i18n("Darkening strength:")
                    }
                    QQC2.Slider {
                        objectName: "dimValue"
                        Layout.fillWidth: true
                        enabled: dimWhenInactiveCheckbox.checked
                        from: 0
                        to: 255
                        stepSize: 1
                        // Profile files' key, misspelled.
                        Component.onCompleted: value = root.editor.value("DimmValue")
                        onMoved: root.editor.setValue("DimmValue", Math.round(value))
                    }
                }

                SectionLabel {
                    text: i18n("Active Terminals:")
                }
                Setting {
                    id: borderWhenActiveCheckbox

                    objectName: "borderWhenActiveCheckbox"
                    profileProperty: "BorderWhenActive"
                    text: i18n("Border")
                    toolTip: i18n("Indicate whether the window is active by drawing a border")
                }
                Item {}
                RowLayout {
                    MnemonicLabel {
                        objectName: "borderLabel"
                        enabled: borderWhenActiveCheckbox.checked
                        label: i18n("Color:")
                        buddy: focusBorderColor
                    }
                    ColorButton {
                        id: focusBorderColor

                        objectName: "focusBorderColor"
                        enabled: borderWhenActiveCheckbox.checked
                        Component.onCompleted: color = root.editor.value("FocusBorderColor")
                        onColorEdited: color => root.editor.setValue("FocusBorderColor", color)
                        QQC2.ToolTip.text: i18n("Select the color used to draw the border")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    }
                }

                Setting {
                    objectName: "invertSelectionColorsCheckbox"
                    Layout.columnSpan: 2
                    profileProperty: "InvertSelectionColors"
                    text: i18n("Always invert the colors of selected text")
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }

        GridLayout {
            objectName: "CTLtab"
            columns: 2

            Item {}
            Setting {
                id: wordMode

                objectName: "wordMode"
                profileProperty: "WordMode"
                text: i18n("Word mode")
                toolTip: i18n("Enable word mode text rendering")
            }
            Item {}
            Setting {
                objectName: "wordModeAttr"
                profileProperty: "WordModeAttr"
                enabled: wordMode.checked
                text: i18n("Use the same attributes for whole word")
                toolTip: i18n("Ignore attribute changes mid-word (otherwise, attribute changes break word when rendering)")
            }
            Item {}
            Setting {
                objectName: "wordModeAscii"
                profileProperty: "WordModeAscii"
                enabled: wordMode.checked
                text: i18n("ASCII characters ")
                toolTip: i18n("Render words of ASCII characters together")
            }
            Item {}
            Setting {
                objectName: "wordModeBrahmic"
                profileProperty: "WordModeBrahmic"
                enabled: wordMode.checked
                text: i18n("Brahmic scripts characters")
                toolTip: i18n("Render words of Brahmic scripts characters together")
            }

            SectionLabel {
                text: i18n("Emoji Font:")
            }
            QQC2.Button {
                objectName: "emojiFontPreview"
                Layout.fillWidth: true
                text: root.editor.emojiFontText
                font: root.editor.previewFont
                onClicked: root.editor.chooseEmojiFont(this)
            }

            Item {}
            Setting {
                id: enableBidiRenderingButton

                objectName: "enableBidiRenderingButton"
                profileProperty: "BidiRenderingEnabled"
                text: i18n("Bi-Directional text rendering")
                toolTip: i18n("Enable Bi-Directional display on terminals (valid for Arabic, Farsi or Hebrew only)")
            }
            Item {}
            Setting {
                objectName: "bidiLineLTR"
                profileProperty: "BidiLineLTR"
                enabled: enableBidiRenderingButton.checked
                text: i18n("force LTR line direction")
                toolTip: i18n("Lines are always LTR, rather than determined by first strong character")
            }
            Item {}
            Setting {
                objectName: "enableBidiTableDirOverrideButton"
                profileProperty: "BidiTableDirOverride"
                enabled: enableBidiRenderingButton.checked
                text: i18n("Table characters BiDi mode override")
                toolTip: i18n("Treat table drawing characters as strong LTR.")
            }
            Item {}
            Setting {
                objectName: "ignoreWcWidth"
                profileProperty: "IgnoreWcWidth"
                text: i18n("Override wcwidth")
                toolTip: i18n("Ignore wcwidth of problematic characters")
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
