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

// One line of a compat window's KXmlGui toolbars (ChromeAdapter), as a QQC2 ToolBar: each
// toolbar's actions in order, the last toolbar taking the rest of the line as QMainWindow
// gives it.
QQC2.ToolBar {
    id: bar

    required property ChromeAdapter chrome
    // [{toolBar, buttonStyle, iconSize, items}]
    required property var line
    property bool vertical: false
    // In a panel: it looks like part of the panel, and the host has its own menu.
    property bool transparentBackground: false
    property bool hostContextMenu: false
    signal contextMenuRequested(Item item, point position)

    Binding {
        target: bar.background
        property: "visible"
        value: false
        when: bar.transparentBackground
    }

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
                    onTapped: eventPoint => {
                        if (bar.hostContextMenu) {
                            bar.contextMenuRequested(toolBar, eventPoint.position);
                        } else {
                            bar.chrome.toolBarContextMenu(toolBar.modelData.toolBar, toolBar, eventPoint.position);
                        }
                    }
                }

                // QToolBar's handle while the positions aren't locked: drag it to a window edge.
                Item {
                    id: grip
                    objectName: "toolBarGrip"
                    property point dropPosition
                    visible: toolBar.modelData.movable ?? false
                    implicitWidth: bar.vertical ? Kirigami.Units.gridUnit : Kirigami.Units.smallSpacing * 2
                    implicitHeight: bar.vertical ? Kirigami.Units.smallSpacing * 2 : Kirigami.Units.gridUnit
                    Layout.fillHeight: !bar.vertical
                    Layout.fillWidth: bar.vertical

                    Kirigami.Separator {
                        anchors.centerIn: parent
                        width: bar.vertical ? parent.width * 0.6 : 1
                        height: bar.vertical ? 1 : parent.height * 0.6
                    }
                    HoverHandler {
                        cursorShape: Qt.SizeAllCursor
                    }
                    DragHandler {
                        id: gripDrag
                        target: null
                        onCentroidChanged: {
                            if (active) {
                                grip.dropPosition = centroid.scenePosition;
                                dropIndicator.area = bar.chrome.dropArea(grip.dropPosition, Qt.size(grip.Window.width, grip.Window.height));
                            }
                        }
                        onActiveChanged: {
                            if (!active) {
                                bar.chrome.moveToolBar(toolBar.modelData.toolBar, grip.dropPosition, Qt.size(grip.Window.width, grip.Window.height));
                            }
                        }
                    }

                    // Where the toolbar goes, as QMainWindow shows it while one is dragged.
                    Rectangle {
                        id: dropIndicator
                        objectName: "toolBarDropIndicator"
                        // Qt::ToolBarArea: 1 left, 2 right, 4 top, 8 bottom.
                        property int area: 0
                        readonly property bool horizontal: area === 4 || area === 8
                        readonly property real extent: bar.vertical ? bar.width : bar.height
                        parent: QQC2.Overlay.overlay
                        visible: gripDrag.active && area !== 0
                        x: area === 2 ? (parent?.width ?? 0) - width : 0
                        y: area === 8 ? (parent?.height ?? 0) - height : 0
                        width: horizontal ? (parent?.width ?? 0) : extent
                        height: horizontal ? extent : (parent?.height ?? 0)
                        color: Qt.alpha(Kirigami.Theme.highlightColor, 0.3)
                        border.color: Kirigami.Theme.highlightColor
                        border.width: 1
                    }
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
