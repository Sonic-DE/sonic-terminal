/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.sonicde.terminal
import org.sonicde.terminal.app

// The whole terminal as Main.qml lays it out, for a panel: toolbars, tabs and splits, in the
// compat window's coordinates, which are this item's. Only the text area draws a background.
TerminalHost {
    id: host

    // Right click on an empty part of a toolbar.
    signal toolBarContextMenuRequested(Item item, point position)
    // Where the terminals' text is, in this item: what the host leaves its background out of.
    readonly property rect textArea: content.item ? content.item.textArea : Qt.rect(0, 0, 0, 0)

    fullTerminal: true
    focus: true
    clip: true

    Loader {
        id: content
        anchors.fill: parent
        active: host.compatWindow !== null
        focus: true
        sourceComponent: Item {
            readonly property rect textArea: Qt.rect(paneTree.x + paneTree.shownPanesGeometry.x, paneTree.y + paneTree.shownPanesGeometry.y, paneTree.shownPanesGeometry.width, paneTree.shownPanesGeometry.height)

            focus: true

            ChromeAdapter {
                id: adapter
                window: host.compatWindow
            }

            PaneTree {
                id: paneTree
                objectName: "panelPaneTree"
                x: paneTree.centralGeometry.x
                y: paneTree.centralGeometry.y
                width: paneTree.centralGeometry.width
                height: paneTree.centralGeometry.height
                focus: true
                compatWindow: host.compatWindow
            }

            TabBar {
                z: 1
                compatWindow: host.compatWindow
                transparentBackground: true
            }
            SearchTabs {
                z: 2
                compatWindow: host.compatWindow
            }

            ColumnLayout {
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                }
                spacing: 0
                z: 3
                Repeater {
                    model: adapter.topLines
                    delegate: ToolBarLine {
                        required property var modelData
                        Layout.fillWidth: true
                        chrome: adapter
                        line: modelData
                        position: QQC2.ToolBar.Header
                        transparentBackground: true
                        hostContextMenu: true
                        onContextMenuRequested: (item, position) => host.toolBarContextMenuRequested(item, position)
                    }
                }
            }
            ColumnLayout {
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                }
                spacing: 0
                z: 3
                Repeater {
                    model: adapter.bottomLines
                    delegate: ToolBarLine {
                        required property var modelData
                        Layout.fillWidth: true
                        chrome: adapter
                        line: modelData
                        position: QQC2.ToolBar.Footer
                        transparentBackground: true
                        hostContextMenu: true
                        onContextMenuRequested: (item, position) => host.toolBarContextMenuRequested(item, position)
                    }
                }
            }

            CommandBar {
                id: commandBar
                controller: host.controller
            }
            Connections {
                target: host.controller
                function onCommandBarRequested(model) {
                    commandBar.show(model);
                }
            }
        }
    }
}
