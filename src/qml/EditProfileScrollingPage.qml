/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// The Edit Profile dialog's Scrolling page (EditProfileScrollingPage.ui).
DialogPage {
    id: root

    required property ProfileEditor editor

    title: i18n("Scrolling")
    iconName: "preferences-scroll"

    component SectionLabel: QQC2.Label {
        Layout.alignment: Qt.AlignRight
    }
    component Choice: ProfileRadioButton {
        editor: root.editor
    }
    component Setting: ProfileCheckBox {
        editor: root.editor
    }
    // The .ui's fixed 16 px vertical spacers.
    component Gap: Item {
        Layout.columnSpan: 2
        Layout.preferredHeight: 16
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2

        SectionLabel {
            Layout.alignment: Qt.AlignRight | Qt.AlignTop
            text: i18n("Scrollback:")
        }
        HistorySizeFields {
            id: historySizeWidget

            objectName: "historySizeWidget"
            Layout.fillWidth: true
            mode: root.editor.value("HistoryMode")
            Component.onCompleted: setLineCount(root.editor.value("HistorySize"))
            onHistoryModeChanged: mode => root.editor.setValue("HistoryMode", mode)
            onHistorySizeChanged: lines => root.editor.setValue("HistorySize", lines)
        }

        Gap {}

        SectionLabel {
            text: i18n("Scroll Page Up/Down:")
        }
        Choice {
            objectName: "scrollHalfPage"
            profileProperty: "ScrollFullPage"
            value: 0
            text: i18n("Half screen heigh&t")
            toolTip: i18n("Scroll the page the half height of window")
        }
        Item {}
        Choice {
            objectName: "scrollFullPage"
            profileProperty: "ScrollFullPage"
            value: 1
            text: i18n("F&ull screen height")
            toolTip: i18n("Scroll the page the full height of window")
        }

        Gap {}

        SectionLabel {
            text: i18n("Scrollbar position:")
        }
        Choice {
            objectName: "scrollBarRightButton"
            profileProperty: "ScrollBarPosition"
            value: 1
            text: i18n("Ri&ght side")
            toolTip: i18n("Show the scroll bar on the right side of the terminal window")
        }
        Item {}
        Choice {
            objectName: "scrollBarLeftButton"
            profileProperty: "ScrollBarPosition"
            value: 0
            text: i18n("&Left side")
            toolTip: i18n("Show the scroll bar on the left side of the terminal window")
        }
        Item {}
        Choice {
            objectName: "scrollBarHiddenButton"
            profileProperty: "ScrollBarPosition"
            value: 2
            text: i18n("Hidden")
        }

        Gap {}

        SectionLabel {
            text: i18n("Highlighting:")
        }
        Setting {
            objectName: "highlightScrolledLinesButton"
            profileProperty: "HighlightScrolledLines"
            text: i18n("Highlight the lines coming into view")
        }

        SectionLabel {
            text: i18n("Reflow Lines:")
        }
        Setting {
            objectName: "reflowLinesButton"
            profileProperty: "ReflowLines"
            text: i18n("Reflow lines when terminal resizes")
        }

        SectionLabel {
            text: i18n("Color of scrollbar markers:")
        }
        ColorButton {
            objectName: "markerColorButton"
            Layout.fillWidth: true
            Component.onCompleted: color = root.editor.value("MarkerColor")
            onColorEdited: color => root.editor.setValue("MarkerColor", color)
        }

        SectionLabel {
            text: i18n("Size of each marker (as a percentage of scrollbar length):")
        }
        // QDoubleSpinBox with one decimal: the value in tenths.
        QQC2.SpinBox {
            id: markerSizeWidget

            readonly property real realFrom: 0
            readonly property real realTo: 100
            readonly property string suffix: i18n(" %")

            objectName: "markerSizeWidget"
            Layout.fillWidth: true
            editable: true
            from: realFrom * 10
            to: realTo * 10
            textFromValue: (value, locale) => Number(value / 10).toLocaleString(locale, "f", 1) + suffix
            valueFromText: (text, locale) => Math.round(Number.fromLocaleString(locale, text.replace(suffix, "").trim()) * 10)
            validator: DoubleValidator {
                bottom: markerSizeWidget.realFrom
                top: markerSizeWidget.realTo
                decimals: 1
            }
            Component.onCompleted: value = Math.round(root.editor.value("MarkerSize") * 10)
            onValueModified: root.editor.setValue("MarkerSize", value / 10)
        }

        SectionLabel {
            text: i18n("Color of scrollbar search highlighting lines:")
        }
        ColorButton {
            objectName: "searchHighligheterColorButton"
            Layout.fillWidth: true
            Component.onCompleted: color = root.editor.value("SearchLineColor")
            onColorEdited: color => root.editor.setValue("SearchLineColor", color)
        }

        SectionLabel {
            text: i18n("Search highlighting line opacity:")
        }
        QQC2.Slider {
            objectName: "labelSearchHighlighterOpacitySlider"
            Layout.fillWidth: true
            from: 0
            to: 200
            stepSize: 1
            // QSlider's default: no tick marks.
            Kirigami.StyleHints.tickMarkStepSize: -1
            Component.onCompleted: value = root.editor.value("SearchLineOpacity")
            onMoved: root.editor.setValue("SearchLineOpacity", Math.round(value))
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
