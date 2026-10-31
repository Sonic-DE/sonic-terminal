/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// One line of a compat window's KXmlGui toolbars (ChromeAdapter), as a QQC2 ToolBar: each
// toolbar's actions in order, the last toolbar taking the rest of the line as QMainWindow
// gives it.
QQC2.ToolBar {
    id: bar

    required property ChromeAdapter chrome
    // [{toolBar, buttonStyle, iconSize, items}]
    required property var line
    property bool vertical: false

    // The hidden toolbars take this bar's extent, so the compat central widget lies where
    // the QML central area is.
    function updateExtents() {
        for (const toolBar of bar.line) {
            bar.chrome.setExtent(toolBar.toolBar, bar.vertical ? bar.width : bar.height);
        }
    }
    onHeightChanged: updateExtents()
    onWidthChanged: updateExtents()
    onLineChanged: updateExtents()

    contentItem: GridLayout {
        flow: bar.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight
        rows: bar.vertical ? -1 : 1
        columns: bar.vertical ? 1 : -1
        rowSpacing: 0
        columnSpacing: 0

        Repeater {
            model: bar.line
            delegate: GridLayout {
                id: toolBar

                required property var modelData
                required property int index

                flow: bar.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight
                rows: bar.vertical ? -1 : 1
                columns: bar.vertical ? 1 : -1
                rowSpacing: Kirigami.Units.smallSpacing
                columnSpacing: Kirigami.Units.smallSpacing
                Layout.fillWidth: !bar.vertical && index === bar.line.length - 1
                Layout.fillHeight: bar.vertical && index === bar.line.length - 1

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: eventPoint => bar.chrome.toolBarContextMenu(toolBar.modelData.toolBar, toolBar, eventPoint.position)
                }

                Repeater {
                    model: toolBar.modelData.items
                    delegate: Loader {
                        id: entry

                        required property var modelData

                        Layout.fillWidth: !bar.vertical && modelData.spacer
                        Layout.fillHeight: bar.vertical && modelData.spacer
                        sourceComponent: modelData.spacer ? spacer : modelData.separator ? separator : button

                        Component {
                            id: spacer
                            Item {}
                        }
                        Component {
                            id: separator
                            Kirigami.Separator {
                                implicitWidth: bar.vertical ? Kirigami.Units.gridUnit : 1
                                implicitHeight: bar.vertical ? 1 : Kirigami.Units.gridUnit
                            }
                        }
                        Component {
                            id: button
                            ActionToolButton {
                                chrome: bar.chrome
                                qAction: entry.modelData.action
                                iconName: entry.modelData.iconName
                                popupMode: entry.modelData.popupMode
                                buttonStyle: entry.modelData.buttonStyle
                                iconSize: toolBar.modelData.iconSize
                            }
                        }
                    }
                }
            }
        }
    }
}
