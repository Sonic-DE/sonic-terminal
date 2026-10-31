/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// The terminal window: the compat window's (a hidden MainWindow's) KXmlGui menu bar and
// toolbars as Kirigami/QQC2 chrome, and its central area. Kirigami is the window type; none
// of its own chrome (actions, drawers, page row) appears.
Kirigami.AbstractApplicationWindow {
    id: root

    required property QtObject controller

    visible: false
    // The widget window is translucent whenever compositing is on, its children painting
    // every pixel (panes with the profile's opacity): no window background then.
    color: controller.translucent ? "transparent" : Kirigami.Theme.backgroundColor
    Binding {
        target: root.background
        property: "visible"
        value: false
        when: root.controller.translucent
    }

    // Ctrl+Q (StandardKey.Quit) belongs to the terminal (XON), not to Kirigami's quit.
    Component.onCompleted: quitAction.enabled = false

    // The compat window's close asks (queryClose()) and closes this window with it.
    onClosing: close => {
        close.accepted = false;
        root.controller.requestClose();
    }

    ChromeAdapter {
        id: adapter
        window: root.controller.compatWindow
    }
    Connections {
        target: root.controller
        // F10 / activate-menu: the first menu opens.
        function onMenuBarActivated() {
            adapter.popupMenu(0);
        }
    }

    // The menu bar heads the header: Kirigami's window leaves room for header only, not
    // menuBar. The bars stack from the window's top edge, so they form one tools area.
    header: ColumnLayout {
        spacing: 0
        visible: adapter.menuBarVisible || adapter.topLines.length > 0

        // No QQC2 Menus: the bar's items open the real QMenus KXmlGui built.
        QQC2.MenuBar {
            id: menuBar
            Layout.fillWidth: true
            visible: adapter.menuBarVisible
            onHeightChanged: adapter.setExtent(adapter.menuBarWidget, height)
            onVisibleChanged: adapter.setExtent(adapter.menuBarWidget, height)

            contentItem: Row {
                Repeater {
                    id: menuItems
                    model: adapter.menus

                    function updateItems() {
                        const items = [];
                        for (let i = 0; i < count; ++i) {
                            items.push(itemAt(i));
                        }
                        adapter.menuBarItems = items;
                    }
                    onItemAdded: updateItems()
                    onItemRemoved: updateItems()

                    delegate: QQC2.MenuBarItem {
                        required property var modelData
                        required property int index
                        text: modelData.text
                        highlighted: adapter.openIndex === index
                        onPressed: if (adapter.openIndex !== index) adapter.popupMenu(index)
                        // Alt + mnemonic clicks without a press.
                        onClicked: if (adapter.openIndex !== index) adapter.popupMenu(index)
                    }
                }
            }
        }

        Repeater {
            model: adapter.topLines
            delegate: ToolBarLine {
                required property var modelData
                Layout.fillWidth: true
                chrome: adapter
                line: modelData
                position: QQC2.ToolBar.Header
            }
        }
    }

    footer: ColumnLayout {
        spacing: 0
        visible: adapter.bottomLines.length > 0
        Repeater {
            model: adapter.bottomLines
            delegate: ToolBarLine {
                required property var modelData
                Layout.fillWidth: true
                chrome: adapter
                line: modelData
                position: QQC2.ToolBar.Footer
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Repeater {
            model: adapter.leftLines
            delegate: ToolBarLine {
                required property var modelData
                Layout.fillHeight: true
                chrome: adapter
                line: modelData
                vertical: true
            }
        }

        // The central area and plugin docks (below) take the room between the side toolbars.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Repeater {
            model: adapter.rightLines
            delegate: ToolBarLine {
                required property var modelData
                Layout.fillHeight: true
                chrome: adapter
                line: modelData
                vertical: true
            }
        }
    }

    // The compat central widget's place, in this content item: the hidden bars have the QML
    // bars' extents, and plugin docks keep their room there.
    Item {
        id: central
        objectName: "centralArea"
        x: paneTree.centralGeometry.x - root.contentItem.x
        y: paneTree.centralGeometry.y - root.contentItem.y
        width: paneTree.centralGeometry.width
        height: paneTree.centralGeometry.height
        clip: true

        PaneTree {
            id: paneTree
            anchors.fill: parent
            compatWindow: root.controller.compatWindow
        }

        // The tab widget's row (tab bar and corner buttons) on the window background.
        Rectangle {
            width: parent.width
            y: tabBar.y - paneTree.centralGeometry.y
            height: tabBar.height
            visible: tabBar.visible
            z: 1
            color: Kirigami.Theme.backgroundColor
        }
        // The tab bar adapter works in the compat window's coordinates.
        Item {
            x: -paneTree.centralGeometry.x
            y: -paneTree.centralGeometry.y
            z: 1
            TabBar {
                id: tabBar
                compatWindow: root.controller.compatWindow
            }
            SearchTabs {
                z: 2
                compatWindow: root.controller.compatWindow
            }
        }
    }

    // Plugin docks, in the compat window's coordinates.
    Item {
        x: -root.contentItem.x
        y: -root.contentItem.y
        DockPanels {
            compatWindow: root.controller.compatWindow
        }
    }
}
