/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// The Colour Scheme Editor (ColorSchemeEditor): a scheme's description, colour table,
// transparency and wallpaper. OK and Apply save it; a colour cell opens QColorDialog.
DialogWindow {
    id: root

    required property ColorSchemeEditorBackend editor

    title: editor.title
    standardButtons: QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Cancel | QQC2.DialogButtonBox.Apply
    onAccepted: editor.save()
    onApplied: editor.save()

    component RowLabel: QQC2.Label {
        Layout.alignment: Qt.AlignRight
    }
    component PercentLabel: QQC2.Label {
        required property int percent

        Layout.minimumWidth: metrics.advanceWidth
        horizontalAlignment: Text.AlignRight
        text: percent + "%"

        TextMetrics {
            id: metrics

            text: "100%"
        }
    }

    RowLayout {
        Layout.fillWidth: true

        RowLabel {
            text: i18n("Description:")
        }
        ClearableTextField {
            objectName: "descriptionEdit"
            Layout.fillWidth: true
            // Set once: a binding would be written back from its own change.
            Component.onCompleted: text = root.editor.description
            onTextChanged: root.editor.description = text
        }
    }

    // QTableWidget: Name, Color, Intense color, Faint color; a colour cell is clicked.
    QQC2.Frame {
        id: colorTable

        objectName: "colorTable"
        // The cell at row, column as a click on it (the parity hook's clickItem).
        function clickItem(row, column) {
            if (column > 0) {
                cells.itemAt(row * 3 + column - 1).choose();
            }
        }

        Layout.fillWidth: true
        Layout.fillHeight: true
        // QTableWidget's size hint: the dialog opens with the table scrolling.
        Layout.preferredWidth: 256
        Layout.preferredHeight: 192
        padding: 0
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false

        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2)
        }

        QQC2.ScrollView {
            anchors.fill: parent

            GridLayout {
                id: table

                readonly property var headers: [i18nc("@label:listbox Column header text for color names", "Name"),
                    i18nc("@label:listbox Column header text for the actual colors", "Color"),
                    i18nc("@label:listbox Column header text for the actual intense colors", "Intense color"),
                    i18nc("@label:listbox Column header text for the actual faint colors", "Faint color")]
                // The three colour columns share one width, the widest header's, and grow equally.
                readonly property real colorColumnWidth: Math.ceil(Math.max(...headers.slice(1).map(text => headerMetrics.advanceWidth(text))))
                    + 2 * Kirigami.Units.smallSpacing

                width: colorTable.availableWidth
                columns: 4
                rowSpacing: 1
                columnSpacing: 1

                FontMetrics {
                    id: headerMetrics
                    font.bold: true
                }

                Repeater {
                    model: table.headers
                    delegate: QQC2.Label {
                        required property string modelData
                        required property int index

                        Layout.fillWidth: index > 0
                        Layout.preferredWidth: index > 0 ? table.colorColumnWidth : -1
                        Layout.horizontalStretchFactor: index > 0 ? 1 : -1
                        padding: Kirigami.Units.smallSpacing
                        font.bold: true
                        text: modelData
                    }
                }

                Repeater {
                    id: cells

                    // Each row: its name, then one cell per colour column.
                    model: root.editor.colorNames.length * 3
                    delegate: Item {
                        id: cell

                        required property int index
                        readonly property int row: Math.floor(index / 3)
                        readonly property int column: index % 3
                        property color color: root.editor.color(row, column)

                        function choose() {
                            const chosen = DialogHelpers.chooseColor(color, cell);
                            if (chosen) {
                                root.editor.setColor(row, column, chosen);
                                color = chosen;
                            }
                        }

                        Layout.row: row + 1
                        Layout.column: column + 1
                        Layout.fillWidth: true
                        Layout.preferredWidth: table.colorColumnWidth
                        Layout.horizontalStretchFactor: 1
                        implicitHeight: Kirigami.Units.gridUnit + Kirigami.Units.smallSpacing

                        Rectangle {
                            anchors.fill: parent
                            color: cell.color
                        }
                        MouseArea {
                            id: cellMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: cell.choose()
                        }
                        QQC2.ToolTip.text: [i18nc("@info:tooltip", "Click to choose color"),
                            i18nc("@info:tooltip", "Click to choose intense color"),
                            i18nc("@info:tooltip", "Click to choose Faint color")][column]
                        QQC2.ToolTip.visible: cellMouse.containsMouse
                        QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    }
                }

                Repeater {
                    model: root.editor.colorNames
                    delegate: QQC2.Label {
                        required property string modelData
                        required property int index

                        Layout.row: index + 1
                        Layout.column: 0
                        padding: Kirigami.Units.smallSpacing
                        text: modelData
                    }
                }
            }
        }
    }

    QQC2.CheckBox {
        objectName: "randomizedBackgroundCheck"
        text: i18n("Randomly adjust colors for each session")
        checked: root.editor.randomizedBackground
        onToggled: root.editor.randomizedBackground = checked
        QQC2.ToolTip.text: i18n("Hue and saturation values of default foreground and background colors are randomized by default. Some color schemes might use different randomization settings.\nTo see any effect, set colors with saturation value greater than 0.")
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
    }
    QQC2.CheckBox {
        objectName: "blurCheckBox"
        text: i18n("Blur background")
        checked: root.editor.blur
        onToggled: root.editor.blur = checked
    }

    // Each row is its own QHBoxLayout in the .ui: a label at its width, then an expanding
    // slider, line edit or (Preferred, sharing the rest with the label) combo box.
    RowLayout {
        Layout.fillWidth: true

        RowLabel {
            text: i18n("Background color transparency:")
        }
        QQC2.Slider {
            id: transparencySlider

            objectName: "transparencySlider"
            Layout.fillWidth: true
            from: 0
            to: 100
            stepSize: 1
            // QSlider's default: no tick marks.
            Kirigami.StyleHints.tickMarkStepSize: -1
            value: root.editor.transparency
            onMoved: root.editor.transparency = Math.round(value)
        }
        PercentLabel {
            objectName: "transparencyPercentLabel"
            percent: transparencySlider.value
        }
    }

    RowLayout {
        Layout.fillWidth: true

        RowLabel {
            text: i18n("Wallpaper transparency:")
        }
        QQC2.Slider {
            id: wallpaperTransparencySlider

            objectName: "wallpaperTransparencySlider"
            Layout.fillWidth: true
            enabled: root.editor.wallpaperEnabled
            from: 0
            to: 100
            stepSize: 1
            Kirigami.StyleHints.tickMarkStepSize: -1
            value: root.editor.wallpaperTransparency
            onMoved: root.editor.wallpaperTransparency = Math.round(value)
        }
        PercentLabel {
            objectName: "wallpaperTransparencyPercentLabel"
            percent: wallpaperTransparencySlider.value
        }
    }

    RowLayout {
        Layout.fillWidth: true

        QQC2.Label {
            text: i18n("Background image:")
        }
        ClearableTextField {
            id: wallpaperPath

            objectName: "wallpaperPath"
            Layout.fillWidth: true
            // QLineEdit's size hint (17 'x's), not the desktop style's 200 px minimum.
            Layout.preferredWidth: lineEditMetrics.advanceWidth + leftPadding + rightPadding

            TextMetrics {
                id: lineEditMetrics

                font: wallpaperPath.font
                text: "xxxxxxxxxxxxxxxxx"
            }
            Component.onCompleted: text = root.editor.wallpaperPath
            onTextChanged: root.editor.wallpaperPath = text
            QQC2.ToolTip.text: i18n("Edit the path of the background image")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
        }
        QQC2.ToolButton {
            id: wallpaperSelectButton

            objectName: "wallpaperSelectButton"
            text: i18n("...")
            icon.name: "image-x-generic"
            display: QQC2.AbstractButton.IconOnly
            onClicked: {
                root.editor.chooseWallpaper(wallpaperSelectButton);
                wallpaperPath.text = root.editor.wallpaperPath;
            }
            QQC2.ToolTip.text: i18n("Choose the background image")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
        }
    }

    RowLayout {
        Layout.fillWidth: true

        QQC2.Label {
            Layout.fillWidth: true
            text: i18n("Wallpaper scaling:")
        }
        QQC2.ComboBox {
            objectName: "wallpaperScalingType"
            Layout.fillWidth: true
            enabled: root.editor.wallpaperEnabled
            model: [i18n("Tile"), i18n("Stretch"), i18n("Crop"), i18n("Adapt"), i18n("NoScaling")]
            currentIndex: root.editor.wallpaperScaling
            onActivated: index => root.editor.wallpaperScaling = index
        }
    }

    RowLayout {
        Layout.fillWidth: true

        QQC2.Label {
            Layout.fillWidth: true
            text: i18n("Wallpaper flip:")
        }
        QQC2.ComboBox {
            objectName: "wallpaperFlipType"
            Layout.fillWidth: true
            enabled: root.editor.wallpaperEnabled
            model: [i18n("NoFlip"), i18n("Horizontal"), i18n("Vertical"), i18n("Both")]
            currentIndex: root.editor.wallpaperFlip
            onActivated: index => root.editor.wallpaperFlip = index
        }
    }

    RowLayout {
        Layout.fillWidth: true

        QQC2.Label {
            text: i18n("Horizontal Anchor:")
        }
        QQC2.Slider {
            id: horizontalAnchorSlider

            objectName: "wallpaperHorizontalAnchorSlider"
            Layout.fillWidth: true
            enabled: root.editor.wallpaperEnabled
            from: 0
            to: 2
            stepSize: 1
            snapMode: QQC2.Slider.SnapAlways
            value: root.editor.horizontalAnchor
            onMoved: root.editor.horizontalAnchor = Math.round(value)
        }
        QQC2.Label {
            objectName: "wallpaperHorizontalAnchorPosition"
            text: ["Left", "Center", "Right"][Math.round(horizontalAnchorSlider.value)]
        }
    }

    RowLayout {
        Layout.fillWidth: true

        QQC2.Label {
            text: i18n("Vertical Anchor:")
        }
        QQC2.Slider {
            id: verticalAnchorSlider

            objectName: "wallpaperVerticalAnchorSlider"
            Layout.fillWidth: true
            enabled: root.editor.wallpaperEnabled
            from: 0
            to: 2
            stepSize: 1
            snapMode: QQC2.Slider.SnapAlways
            value: root.editor.verticalAnchor
            onMoved: root.editor.verticalAnchor = Math.round(value)
        }
        QQC2.Label {
            objectName: "wallpaperVerticalAnchorPosition"
            text: ["Top", "Middle", "Bottom"][Math.round(verticalAnchorSlider.value)]
        }
    }

    Kirigami.InlineMessage {
        objectName: "transparencyWarningWidget"
        Layout.fillWidth: true
        // Wrapped: its one-line width mustn't widen the dialog.
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        type: Kirigami.MessageType.Warning
        text: i18nc("@info:status", "The background transparency setting will not be used because your desktop does not appear to support transparent windows.")
        visible: !root.editor.supportsTransparency
    }
}
