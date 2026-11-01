/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// HistorySizeWidget: no, fixed size or unlimited scrollback (Enum::HistoryModeEnum).
ColumnLayout {
    id: root

    // Enum::NoHistory, FixedSizeHistory, UnlimitedHistory; -1 while none is chosen.
    property int mode: -1
    readonly property alias lineCount: spinner.value

    // The user changed them (HistorySizeWidget's signals).
    signal historyModeChanged(int mode)
    signal historySizeChanged(int lines)

    function setLineCount(lines) {
        spinner.value = lines;
        spinner.stepSize = Math.floor(lines / 10);
    }

    spacing: 0

    function syncMode() {
        for (const button of modeGroup.buttons) {
            button.checked = button.mode === mode;
        }
    }

    onModeChanged: syncMode()
    Component.onCompleted: syncMode()

    QQC2.ButtonGroup {
        id: modeGroup

        onClicked: button => {
            root.mode = button.mode;
            root.historyModeChanged(button.mode);
        }
    }

    component ModeButton: QQC2.RadioButton {
        required property int mode

        QQC2.ButtonGroup.group: modeGroup
        QQC2.ToolTip.visible: hovered && QQC2.ToolTip.text.length > 0
        QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
    }

    component WarningButton: QQC2.ToolButton {
        id: warningButton

        property alias message: tip.text
        property bool shown

        icon.name: "emblem-warning"
        // Keeps its place while hidden, as the widget's size policy does.
        opacity: shown ? 1 : 0
        enabled: shown
        onClicked: tip.open()

        // QWhatsThis::showText(): until the next click.
        QQC2.ToolTip {
            id: tip

            parent: warningButton.parent
            x: (parent.width - width) / 2
            y: parent.height
            timeout: -1
            closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside
        }
    }

    RowLayout {
        ModeButton {
            id: fixedButton

            objectName: "fixedSizeHistoryButton"
            mode: 1
            text: i18n("Fi&xed size:")
            QQC2.ToolTip.text: i18n("Limit the remembered output to a fixed number of lines")
            // Once the spinner is enabled by the new mode.
            onClicked: Qt.callLater(() => {
                spinner.forceActiveFocus();
                spinner.contentItem.selectAll();
            })
        }
        QQC2.SpinBox {
            id: spinner

            objectName: "historyLineSpinner"
            // KPluralHandlingSpinBox's suffix, for the current value.
            readonly property string suffix: i18ncp("@label:textbox Unit of scrollback", " line", " lines", value)

            enabled: fixedButton.checked
            editable: true
            from: 1
            to: 1000000
            value: 1000
            textFromValue: (value, locale) => Number(value).toLocaleString(locale, "f", 0) + i18ncp("@label:textbox Unit of scrollback", " line", " lines", value)
            valueFromText: (text, locale) => Number.fromLocaleString(locale, text.replace(/[^\d.,\s-]/g, "").trim())
            QQC2.ToolTip.text: i18n("Number of lines of output to remember")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            onValueModified: root.historySizeChanged(value)
        }
        WarningButton {
            shown: fixedButton.checked
            message: i18nc("@info:whatsthis", "When using this option, the scrollback data will be saved to RAM. If you choose a huge value, your system may run out of free RAM and cause serious issues with your system.")
        }
        Item {
            Layout.fillWidth: true
        }
    }

    RowLayout {
        ModeButton {
            id: unlimitedButton

            objectName: "unlimitedHistoryButton"
            mode: 2
            text: i18nc("Save all lines to the scrollback history", "&Unlimited")
            QQC2.ToolTip.text: i18n("Remember all output produced by the terminal")
        }
        WarningButton {
            shown: unlimitedButton.checked
            message: xi18nc("@info:tooltip", "When using this option, the scrollback data will be written unencrypted to temporary files. Those temporary files will be deleted automatically when Konsole is closed in a normal manner.<nl/>Use <emphasis>Settings → Configure Konsole → Temporary Files</emphasis> to select the location of the temporary files.")
        }
        Item {
            Layout.fillWidth: true
        }
    }

    ModeButton {
        objectName: "noHistoryButton"
        mode: 0
        text: i18nc("Do not save any lines to the scrollback history", "&None")
        QQC2.ToolTip.text: i18n("Do not remember previous output")
    }
}
