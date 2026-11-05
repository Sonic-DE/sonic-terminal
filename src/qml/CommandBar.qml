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

// KCommandBar over the window: a frameless search field over the list of actions, each with
// its group in a softer colour, the filter's matches highlighted and its shortcut as keys.
// Return or a click runs the current one (a menu's actions replace the list); Escape, a
// click elsewhere or the focus leaving close it. Its parent is the room between the bars.
QQC2.Popup {
    id: bar

    property CommandBarModel model
    required property QtObject controller

    function show(model) {
        bar.model = model;
        field.text = "";
        list.currentIndex = 0;
        open();
        field.forceActiveFocus();
    }

    function activate(row) {
        if (!model || row < 0) {
            return;
        }
        if (model.activate(row)) {
            // A menu's actions: the filter starts again.
            field.text = "";
            list.currentIndex = 0;
        } else {
            close();
        }
    }

    // KCommandBar::show(): a share of the room, within bounds, centred 6 px below the bars.
    width: Math.min(parent.width, Math.max(parent.width / 2.4, 500))
    height: Math.min(parent.height, Math.max(parent.height / 2, 250))
    x: Math.round((parent.width - width) / 2)
    y: 6
    padding: 1
    focus: true
    closePolicy: QQC2.Popup.CloseOnPressOutside | QQC2.Popup.CloseOnEscape
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    onClosed: {
        bar.model = null;
        bar.controller.commandBarClosed();
    }

    SystemPalette {
        id: palette
    }

    // A sunken styled panel with a drop shadow (offset 0, 4, blur 48, the palette's Dark).
    background: Kirigami.ShadowedRectangle {
        color: Kirigami.Theme.backgroundColor
        border.width: 1
        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.3)
        shadow.size: 24
        shadow.yOffset: 4
        shadow.color: palette.dark
    }

    contentItem: ColumnLayout {
        spacing: 0

        QQC2.TextField {
            id: field

            objectName: "commandBarField"
            Layout.fillWidth: true
            leftPadding: searchIcon.width + 2 * Kirigami.Units.smallSpacing
            rightPadding: clearButton.width + 2 * Kirigami.Units.smallSpacing
            topPadding: Kirigami.Units.smallSpacing
            bottomPadding: Kirigami.Units.smallSpacing
            background: null
            onTextChanged: {
                if (bar.model) {
                    bar.model.filterText = text;
                }
                list.currentIndex = 0;
            }
            // Navigation keys go to the list.
            Keys.onUpPressed: list.decrementCurrentIndex()
            Keys.onDownPressed: list.incrementCurrentIndex()
            Keys.onPressed: event => {
                if (event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
                    const page = Math.max(1, Math.floor(list.height / Math.max(1, list.rowHeight)));
                    list.currentIndex = Math.max(0, Math.min(list.count - 1, list.currentIndex + (event.key === Qt.Key_PageUp ? -page : page)));
                    event.accepted = true;
                }
            }
            Keys.onReturnPressed: bar.activate(list.currentIndex)
            Keys.onEnterPressed: bar.activate(list.currentIndex)

            Kirigami.Icon {
                id: searchIcon

                anchors.left: parent.left
                anchors.leftMargin: Kirigami.Units.smallSpacing
                anchors.verticalCenter: parent.verticalCenter
                width: Kirigami.Units.iconSizes.small
                height: width
                source: "search"
            }
            QQC2.ToolButton {
                id: clearButton

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: field.text.length > 0
                icon.name: Qt.application.layoutDirection === Qt.RightToLeft ? "edit-clear-locationbar-ltr" : "edit-clear-locationbar-rtl"
                onClicked: field.text = ""
            }
        }

        // The list's top border.
        Kirigami.Separator {
            Layout.fillWidth: true
        }

        ListView {
            id: list

            readonly property real rowHeight: Math.max(metrics.height, Kirigami.Units.iconSizes.small) + 2 * Kirigami.Units.smallSpacing
            // PM_FocusFrameHMargin and the shortcut keys' margin.
            readonly property int hMargin: 2
            readonly property int keyMargin: hMargin + 2

            objectName: "commandBarList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: bar.model
            currentIndex: 0
            boundsBehavior: Flickable.StopAtBounds
            QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

            FontMetrics {
                id: metrics
            }

            delegate: Rectangle {
                id: row

                required property int index
                required property string display
                required property string iconName
                required property int groupLength
                required property var matches
                required property string shortcut
                required property string toolTip
                readonly property bool selected: ListView.isCurrentItem

                // The group in the placeholder colour, the matches in the link colour; none when selected.
                function styledText() {
                    const escape = text => text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
                    if (selected) {
                        return escape(display);
                    }
                    const kinds = new Array(display.length).fill(0);
                    for (let i = 0; i < groupLength; ++i) {
                        kinds[i] = 1;
                    }
                    for (let m = 0; m + 1 < matches.length; m += 2) {
                        for (let i = matches[m]; i < matches[m] + matches[m + 1] && i < kinds.length; ++i) {
                            kinds[i] = 2;
                        }
                    }
                    const colors = ["", Kirigami.Theme.disabledTextColor, Kirigami.Theme.linkColor];
                    let html = "";
                    let start = 0;
                    for (let i = 1; i <= display.length; ++i) {
                        if (i === display.length || kinds[i] !== kinds[start]) {
                            const part = escape(display.substring(start, i));
                            html += kinds[start] === 0 ? part : "<span style=\"color:" + colors[kinds[start]] + "\">" + part + "</span>";
                            start = i;
                        }
                    }
                    return html;
                }

                width: ListView.view.width
                height: list.rowHeight
                color: selected ? Kirigami.Theme.highlightColor : "transparent"

                QQC2.ToolTip.text: toolTip
                QQC2.ToolTip.visible: hover.hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                HoverHandler {
                    id: hover
                }
                TapHandler {
                    onTapped: {
                        list.currentIndex = row.index;
                        bar.activate(row.index);
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: list.hMargin
                    anchors.rightMargin: list.hMargin
                    spacing: 0

                    Kirigami.Icon {
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                        Layout.rightMargin: 2 * list.hMargin
                        visible: bar.model && bar.model.hasIcons
                        source: row.iconName
                        color: row.selected ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: list.hMargin
                        elide: Text.ElideRight
                        textFormat: Text.StyledText
                        color: row.selected ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        text: row.styledText()
                    }
                    // ShortcutStyleDelegate: keys on rounded keycaps, separators plain.
                    Row {
                        Layout.rightMargin: list.hMargin
                        Repeater {
                            model: row.shortcut.length > 0 && bar.model ? bar.model.shortcutSegments(row.shortcut) : []
                            delegate: Item {
                                id: segment

                                required property int index
                                required property string modelData
                                readonly property bool isKey: index % 2 === 0

                                width: Math.ceil(metrics.advanceWidth(modelData)) + 2 * list.keyMargin
                                height: metrics.lineSpacing

                                Rectangle {
                                    visible: segment.isKey
                                    width: parent.width
                                    height: parent.height
                                    y: 1
                                    radius: 3
                                    color: palette.shadow
                                }
                                Rectangle {
                                    visible: segment.isKey
                                    width: parent.width
                                    height: parent.height
                                    radius: 3
                                    color: palette.window
                                }
                                Text {
                                    anchors.fill: parent
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    color: segment.isKey ? palette.buttonText : (row.selected ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor)
                                    text: segment.modelData
                                }
                            }
                        }
                    }
                }
            }

            // "No commands to display" / "No commands matching the filter": larger, half opaque.
            QQC2.Label {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                visible: text.length > 0
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
                opacity: 0.5
                font.pointSize: Math.round(Kirigami.Theme.defaultFont.pointSize * 1.3)
                text: bar.model ? bar.model.placeholderText : ""
            }
        }
    }
}
