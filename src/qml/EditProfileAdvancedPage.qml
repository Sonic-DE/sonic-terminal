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

// The Edit Profile dialog's Advanced page (EditProfileAdvancedPage.ui).
DialogPage {
    id: root

    required property ProfileEditor editor

    function updateUrlHintsModifiers() {
        let modifiers = 0;
        for (const button of [urlHintsModifierShift, urlHintsModifierCtrl, urlHintsModifierAlt, urlHintsModifierMeta]) {
            if (button.checked) {
                modifiers |= button.modifier;
            }
        }
        editor.setValue("UrlHintsModifiers", modifiers);
    }

    title: i18nc("@title:tab Complex options", "Advanced")
    iconName: "preferences-other"

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
    component ModifierButton: QQC2.ToolButton {
        required property int modifier

        checkable: true
        // A QToolButton without autoRaise.
        flat: false
        Component.onCompleted: checked = (root.editor.value("UrlHintsModifiers") & modifier) !== 0
        onToggled: root.updateUrlHintsModifiers()
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2

        SectionLabel {
            text: i18n("Key combination to show URL hints:")
        }
        RowLayout {
            ModifierButton {
                id: urlHintsModifierShift

                objectName: "urlHintsModifierShift"
                modifier: Qt.ShiftModifier
                text: i18n("Shift")
            }
            ModifierButton {
                id: urlHintsModifierCtrl

                objectName: "urlHintsModifierCtrl"
                modifier: Qt.ControlModifier
                text: i18n("Ctrl")
            }
            ModifierButton {
                id: urlHintsModifierAlt

                objectName: "urlHintsModifierAlt"
                modifier: Qt.AltModifier
                text: i18n("Alt")
            }
            ModifierButton {
                id: urlHintsModifierMeta

                objectName: "urlHintsModifierMeta"
                modifier: Qt.MetaModifier
                text: i18nc("key on keyboard", "Meta")
            }
        }

        Gap {}

        SectionLabel {
            text: i18nc("Items that do not fit in other categories", "Miscellaneous:")
        }
        Setting {
            objectName: "enableReverseUrlHints"
            profileProperty: "ReverseUrlHints"
            text: i18n("Reverse URL hint numbering")
            toolTip: i18n("Number URL hints in reverse, starting from the bottom")
        }
        Item {}
        Setting {
            objectName: "enableBlinkingTextButton"
            profileProperty: "BlinkingTextEnabled"
            text: i18n("Allow blinking text")
            toolTip: i18n("Allow terminal programs to create blinking sections of text")
        }
        Item {}
        Setting {
            objectName: "enableFlowControlButton"
            profileProperty: "FlowControlEnabled"
            text: i18n("Flow control")
            toolTip: i18n("Allow the output to be suspended by pressing Ctrl+S")
        }

        Gap {}

        MnemonicLabel {
            Layout.alignment: Qt.AlignRight
            label: i18n("&Default character encoding:")
            buddy: selectEncodingButton
        }
        QQC2.Button {
            id: selectEncodingButton

            objectName: "selectEncodingButton"
            Layout.fillWidth: true
            text: root.editor.value("DefaultEncoding")
            onClicked: {
                root.editor.chooseEncoding(selectEncodingButton);
                text = root.editor.value("DefaultEncoding");
            }
        }

        SectionLabel {
            text: i18n("Shortcut for peeking the primary screen:")
        }
        KQuickControls.KeySequenceItem {
            objectName: "peekPrimaryWidget"
            keySequence: root.editor.keySequence("PeekPrimaryKeySequence")
            // QKeySequenceEdit doesn't check for conflicts.
            checkForConflictsAgainst: KQuickControls.ShortcutType.None
            onKeySequenceModified: root.editor.setValue("PeekPrimaryKeySequence", keySequence)
        }

        SectionLabel {
            text: i18n("Set interval between autosaves:")
        }
        QQC2.SpinBox {
            readonly property string suffix: i18n(" ms")

            objectName: "autoSaveIntervalWidget"
            // Next to its label (Joseph's decision for this page); the .ui right-aligned it.
            editable: true
            from: 0
            to: 3600000
            textFromValue: (value, locale) => Number(value).toLocaleString(locale, "f", 0) + suffix
            valueFromText: (text, locale) => Number.fromLocaleString(locale, text.replace(suffix, "").trim())
            Component.onCompleted: value = root.editor.value("AutoSaveInterval")
            onValueModified: root.editor.setValue("AutoSaveInterval", value)
        }

        SectionLabel {
            Layout.alignment: Qt.AlignRight | Qt.AlignTop
            text: i18n("Line Numbers:")
        }
        ColumnLayout {
            spacing: 0

            Choice {
                objectName: "lineNumsNever"
                profileProperty: "LineNumbers"
                value: 0
                text: i18n("&Never")
            }
            Choice {
                objectName: "lineNumsURL"
                profileProperty: "LineNumbers"
                value: 1
                text: i18nc("When showing URL hints", "When &URL hints show")
            }
            Choice {
                objectName: "lineNumsAlways"
                profileProperty: "LineNumbers"
                value: 2
                text: i18nc("Always", "A&lways")
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
