/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// RenameTabWidget's KColorCombo: "Custom…" (QColorDialog), "None", then KColorCombo's
// colours. setColor() picks the entry, as KColorCombo::setColor() does.
QQC2.ComboBox {
    id: root

    // KColorCombo::color().
    property color color: DialogHelpers.noneColor()
    property color customColor: "white"
    readonly property var colors: [DialogHelpers.noneColor()].concat(DialogHelpers.standardColors())

    // KColorCombo::activated(): the user chose an entry.
    signal colorActivated(color color)

    function setColor(value) {
        if (!value || !value.valid || value.a === 0) {
            currentIndex = 1;
            color = colors[0];
            return;
        }
        for (let i = 0; i < colors.length; ++i) {
            if (Qt.colorEqual(colors[i], value)) {
                currentIndex = i + 1;
                color = value;
                return;
            }
        }
        customColor = value;
        currentIndex = 0;
        color = value;
    }

    function swatch(index) {
        return index === 0 ? customColor : colors[index - 1];
    }

    model: [qsTranslate("KColorCombo", "Custom…", "@item:inlistbox Custom color"), i18nc("@label:listbox No color selected", "None")]
        .concat(DialogHelpers.standardColors().map(() => ""))
    currentIndex: 1

    onActivated: index => {
        if (index === 0) {
            const chosen = DialogHelpers.chooseColor(customColor, root);
            if (chosen) {
                customColor = chosen;
                color = chosen;
            }
        } else {
            color = colors[index - 1];
        }
        colorActivated(color);
    }

    component Swatch: Rectangle {
        property int entry
        property string label

        color: root.swatch(entry)
        border.color: Kirigami.Theme.textColor
        border.width: 1
        implicitHeight: Kirigami.Units.gridUnit

        QQC2.Label {
            anchors.centerIn: parent
            text: parent.label
            color: parent.color.a > 0 && parent.color.hslLightness > 0.5 ? "black" : (parent.color.a > 0 ? "white" : Kirigami.Theme.textColor)
        }
    }

    // The style draws the frame and arrow; the swatch covers the text area.
    displayText: ""
    implicitContentWidthPolicy: QQC2.ComboBox.WidestText

    Swatch {
        x: root.leftPadding
        y: root.topPadding
        width: root.availableWidth
        height: root.availableHeight
        entry: root.currentIndex
        label: root.currentText
    }

    delegate: QQC2.ItemDelegate {
        required property int index
        required property string modelData

        width: ListView.view ? ListView.view.width : implicitWidth
        highlighted: root.highlightedIndex === index
        contentItem: Swatch {
            entry: index
            label: modelData
        }
    }
}
