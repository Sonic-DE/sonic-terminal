/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// A compat window's (hidden) tab bar as a QQC2 TabBar, in the window's coordinates: each tab
// where the hidden bar lays it out, its close and scroll buttons, and the tab widget's corner
// buttons. Input goes through the hidden widgets (TabBarAdapter, ToolButtonsAdapter).
Item {
    id: root

    required property QtObject compatWindow

    TabBarAdapter {
        id: tabs
        window: root.compatWindow
        onChanged: root.syncTabs()
    }
    Component.onCompleted: syncTabs()

    // One TabButton per tab, inside the TabBar's content model (the style draws a tab from its
    // place in the bar).
    function syncTabs() {
        while (bar.count < tabs.tabs.length) {
            bar.addItem(tabButton.createObject(bar, {tabIndex: bar.count}));
        }
        while (bar.count > tabs.tabs.length) {
            bar.removeItem(bar.itemAt(bar.count - 1));
        }
    }

    x: tabs.geometry.x
    y: tabs.geometry.y
    width: tabs.geometry.width
    height: tabs.geometry.height
    visible: tabs.shown
    z: tabs.stackOrder + 1

    QQC2.TabBar {
        id: bar
        objectName: "tabBarControl"
        anchors.fill: parent
        position: tabs.atBottom ? QQC2.TabBar.Footer : QQC2.TabBar.Header
        currentIndex: tabs.currentIndex
        // The widget is painted on its window's background.
        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
        }
        // A ListView, since TabBar shows only the tabs its ListView lays out; it reproduces the
        // hidden bar's tabRect()s: tabs in a row overlapping as the style makes them, scrolled
        // by the hidden bar's offset.
        contentItem: ListView {
            readonly property var first: tabs.tabs[0]?.rect
            readonly property var second: tabs.tabs[1]?.rect
            model: bar.contentModel
            orientation: ListView.Horizontal
            interactive: false
            spacing: first && second ? second.x - (first.x + first.width) : 0
            contentX: first ? -first.x : 0
        }
    }

    Component {
        id: tabButton
        QQC2.TabButton {
            id: tab
            property int tabIndex: -1
            readonly property var tabData: tabs.tabs[tabIndex] ?? {}
            width: tabData.rect?.width ?? 0
            height: tabData.rect?.height ?? 0
            visible: tabData.visible ?? false
            checked: tabIndex === tabs.currentIndex
            // QTabBar's: clicking a tab leaves the focus in the terminal.
            focusPolicy: Qt.TabFocus
            // The style draws the frame; the icon and title go where it puts them for the hidden
            // tab, beside the close button (the desktop style can't leave room for it).
            Accessible.name: tabData.label ?? ""
            Kirigami.Icon {
                readonly property size size: tab.tabData.iconSize ?? Qt.size(0, 0)
                visible: source !== ""
                source: tab.tabData.iconName ?? ""
                width: size.width
                height: size.height
                x: (tab.tabData.textRect?.x ?? 0) - width - 4
                y: (tab.tabData.textRect?.y ?? 0) + ((tab.tabData.textRect?.height ?? 0) - height) / 2
            }
            QQC2.Label {
                x: tab.tabData.textRect?.x ?? 0
                y: tab.tabData.textRect?.y ?? 0
                width: tab.tabData.textRect?.width ?? 0
                height: tab.tabData.textRect?.height ?? 0
                text: tab.tabData.label ?? ""
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                // The activity colour; otherwise the style's.
                color: tab.tabData.textColor?.valid ? tab.tabData.textColor : Kirigami.Theme.textColor
            }
            QQC2.ToolTip.text: tabData.toolTip ?? ""
            QQC2.ToolTip.visible: hovered && QQC2.ToolTip.text !== ""
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

            // DetachableTabBar's tab colour and progress bar.
            Rectangle {
                readonly property bool progress: (tab.tabData.progress ?? -1) >= 0
                visible: (tab.tabData.color?.valid ?? false) || progress
                x: (tab.tabData.stripRect?.x ?? 0) - tab.x
                y: (tab.tabData.stripRect?.y ?? 0) - tab.y
                width: tab.tabData.stripRect?.width ?? 0
                height: tab.tabData.stripRect?.height ?? 0
                color: tab.tabData.color?.valid ? tab.tabData.color : Kirigami.Theme.highlightColor
                opacity: progress ? 0.3 : 1
                Rectangle {
                    visible: parent.progress
                    width: parent.width * (tab.tabData.progress ?? 0) / 100
                    height: parent.height
                    color: parent.color
                    opacity: 1 / parent.opacity
                }
            }
        }
    }

    // Presses, drags, the context menu and the wheel go through the hidden bar's handlers;
    // hover passes on to the tabs.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        property int pressedIndex: -1
        onPressed: mouse => {
            if (mouse.button === Qt.RightButton) {
                tabs.contextMenu(Qt.point(mouse.x, mouse.y));
                return;
            }
            if (mouse.button === Qt.LeftButton) {
                pressedIndex = tabs.tabAt(Qt.point(mouse.x, mouse.y));
            }
            tabs.press(Qt.point(mouse.x, mouse.y), mouse.button);
        }
        onReleased: mouse => {
            tabs.release(Qt.point(mouse.x, mouse.y), mouse.button, pressedIndex);
            pressedIndex = -1;
        }
        onDoubleClicked: mouse => tabs.doubleClick(Qt.point(mouse.x, mouse.y), mouse.button)
        onWheel: wheel => tabs.wheel(wheel.angleDelta)
    }

    // Close buttons, then scroll buttons (TabBarAdapter's button order).
    Repeater {
        model: tabs.closeButtons
        delegate: QQC2.ToolButton {
            required property var modelData
            required property int index
            x: modelData.geometry.x
            y: modelData.geometry.y
            width: modelData.geometry.width
            height: modelData.geometry.height
            padding: 0
            focusPolicy: Qt.NoFocus
            display: QQC2.AbstractButton.IconOnly
            // As the style's tab close indicator: the title bar close icon, small.
            icon.name: "window-close-symbolic"
            icon.width: Kirigami.Units.iconSizes.small
            icon.height: Kirigami.Units.iconSizes.small
            text: modelData.toolTip
            QQC2.ToolTip.text: modelData.toolTip
            QQC2.ToolTip.visible: hovered && modelData.toolTip !== ""
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: tabs.clickButton(index)
        }
    }
    Repeater {
        model: tabs.scrollButtons
        delegate: QQC2.ToolButton {
            required property var modelData
            required property int index
            x: modelData.geometry.x
            y: modelData.geometry.y
            width: modelData.geometry.width
            height: modelData.geometry.height
            padding: 0
            focusPolicy: Qt.NoFocus
            enabled: modelData.enabled
            display: QQC2.AbstractButton.IconOnly
            icon.name: index === 0 ? "go-previous" : "go-next"
            onClicked: tabs.clickButton(tabs.closeButtons.length + index)
        }
    }

    // The tab widget's corner buttons (new tab, search tabs, close tab), in the window.
    component CornerButtons: Item {
        property alias corner: buttons.root

        ToolButtonsAdapter {
            id: buttons
        }
        Repeater {
            model: buttons.buttons
            delegate: QQC2.ToolButton {
                id: cornerButton
                required property var modelData
                required property int index
                x: modelData.geometry.x
                y: modelData.geometry.y
                width: modelData.geometry.width
                height: modelData.geometry.height
                // QToolButton's default: no focus by clicking.
                focusPolicy: Qt.TabFocus
                enabled: modelData.enabled
                icon.name: modelData.iconName
                text: modelData.text !== "" ? modelData.text : modelData.toolTip
                display: modelData.text !== "" ? QQC2.AbstractButton.TextBesideIcon : QQC2.AbstractButton.IconOnly
                Accessible.role: modelData.hasMenu ? Accessible.ButtonMenu : Accessible.Button
                down: press.pressed && press.containsMouse
                QQC2.ToolTip.text: modelData.toolTip
                QQC2.ToolTip.visible: hovered && modelData.toolTip !== ""
                QQC2.ToolTip.delay: buttons.toolTipDelay

                // QToolButton's rules (instant, delayed and menu-button popups) through the
                // real button.
                MouseArea {
                    id: press
                    anchors.fill: parent
                    pressAndHoldInterval: Qt.styleHints.mousePressAndHoldInterval
                    onPressed: mouse => buttons.press(cornerButton.index, Qt.point(mouse.x, mouse.y))
                    onPressAndHold: buttons.holdPress(cornerButton.index)
                    onReleased: mouse => buttons.release(cornerButton.index, Qt.point(mouse.x, mouse.y), containsMouse)
                }
            }
        }
    }
    CornerButtons {
        parent: root.parent
        z: root.z
        corner: tabs.leftCorner
    }
    CornerButtons {
        parent: root.parent
        z: root.z
        corner: tabs.rightCorner
    }
}
