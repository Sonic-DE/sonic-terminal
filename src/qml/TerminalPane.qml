/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.kde.qqc2desktopstyle.private as StylePrivate
import org.sonicde.terminal

// One terminal: its view (created with its session, in C++) and the parts the facade's
// layout places over it, each where the facade's widgets would be.
Item {
    id: pane

    required property TerminalView view
    // The widget header bar draws a tab bar base line when what is above it is not a tab
    // bar, a terminal or a splitter; the pane tree knows what is above.
    property bool headerTabBarBase: false

    implicitWidth: view ? view.implicitWidth : 0
    implicitHeight: view ? view.implicitHeight : 0

    onViewChanged: {
        if (view) {
            view.parent = pane;
            view.x = 0;
            view.y = 0;
            view.width = Qt.binding(() => pane.width);
            view.height = Qt.binding(() => pane.height);
            // Makes Qt Quick expose the view; its interface is TerminalViewAccessible.
            view.Accessible.role = Accessible.Terminal;
        }
    }
    Component.onCompleted: viewChanged()

    ScrollBarAdapter {
        id: scrollBar
        view: pane.view
    }

    // A facade label with a style sheet background (and border), drawn in its place.
    component StyleSheetLabel: Item {
        id: label
        required property LabelAdapter adapter
        parent: pane.view
        z: adapter.stackOrder + 1
        x: adapter.geometry.x
        y: adapter.geometry.y
        width: adapter.geometry.width
        height: adapter.geometry.height
        visible: adapter.shown

        Rectangle {
            anchors.fill: parent
            color: label.adapter.backgroundColor
            border.width: label.adapter.borderWidth
            border.color: label.adapter.borderColor
        }
        Text {
            x: label.adapter.contentsGeometry.x
            width: label.adapter.contentsGeometry.width
            // On QPainter's baseline, which Text's own centring can miss by a pixel.
            anchors.baseline: label.top
            anchors.baselineOffset: label.adapter.baseline
            text: label.adapter.text
            font: label.adapter.font
            color: label.adapter.textColor
            // Qt::Alignment and Text's horizontal alignment flags have the same values.
            horizontalAlignment: label.adapter.alignment & 0x0f
            textFormat: Text.PlainText
            renderType: Text.NativeRendering
        }
    }

    MessagesAdapter {
        id: messages
        view: pane.view
    }

    // The messages are the view's children like the other parts, so they stack among them.
    Repeater {
        parent: pane.view
        model: messages.messages
        delegate: Item {
            id: message
            required property var modelData
            required property int index
            z: modelData.stackOrder + 1
            x: modelData.geometry.x
            y: modelData.geometry.y
            width: modelData.geometry.width
            height: modelData.geometry.height
            visible: modelData.shown
            clip: true

            MessageFrameItem {
                anchors.fill: parent
                color: message.modelData.color
                parentColor: message.modelData.parentColor
                frameOpacity: message.modelData.opacity
            }
            PixmapItem {
                visible: message.modelData.icon.shown
                x: message.modelData.icon.geometry.x
                y: message.modelData.icon.geometry.y
                width: message.modelData.icon.geometry.width
                height: message.modelData.icon.geometry.height
                image: message.modelData.icon.image
            }
            Text {
                x: message.modelData.text.geometry.x
                y: message.modelData.text.geometry.y
                width: message.modelData.text.geometry.width
                height: message.modelData.text.geometry.height
                text: message.modelData.text.text
                textFormat: message.modelData.text.rich ? Text.RichText : Text.PlainText
                wrapMode: message.modelData.text.wordWrap ? Text.WordWrap : Text.NoWrap
                verticalAlignment: Text.AlignVCenter
                color: message.modelData.text.color
                linkColor: message.modelData.text.linkColor
                font: message.modelData.text.font
                renderType: Text.NativeRendering
                onLinkActivated: link => messages.activateLink(message.index, link)
                onLinkHovered: link => messages.hoverLink(message.index, link)
            }
            Repeater {
                model: message.modelData.buttons
                delegate: Item {
                    id: messageButton
                    required property var modelData
                    required property int index
                    x: modelData.geometry.x
                    y: modelData.geometry.y
                    width: modelData.geometry.width
                    height: modelData.geometry.height

                    StylePrivate.StyleItem {
                        anchors.fill: parent
                        elementType: "toolbutton"
                        flat: messageButton.modelData.autoRaise
                        hover: messageButtonMouse.containsMouse
                        sunken: messageButtonMouse.pressed && messageButtonMouse.containsMouse
                        properties: ({
                                icon: messageButton.modelData.iconName,
                                iconColor: messageButton.modelData.iconColor,
                                iconWidth: messageButton.modelData.iconWidth,
                                iconHeight: messageButton.modelData.iconHeight,
                                toolButtonStyle: Qt.ToolButtonIconOnly
                            })
                    }
                    MouseArea {
                        id: messageButtonMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: messages.clickButton(message.index, messageButton.index)
                    }
                    Controls.ToolTip.text: modelData.toolTip
                    Controls.ToolTip.visible: messageButtonMouse.containsMouse && modelData.toolTip !== ""
                    Controls.ToolTip.delay: headerBar.toolTipDelay
                }
            }
        }
    }

    StyleSheetLabel {
        adapter: LabelAdapter {
            view: pane.view
            label: "hoverLink"
        }
    }
    StyleSheetLabel {
        adapter: LabelAdapter {
            view: pane.view
            label: "resize"
        }
    }

    HeaderBarAdapter {
        id: headerBar
        view: pane.view
    }

    FindBarAdapter {
        id: findBar
        view: pane.view
        // IncrementalSearchBar::focusLineEdit(): focus and select all.
        onFocusRequested: {
            findField.forceActiveFocus(Qt.ActiveWindowFocusReason);
            findField.selectAll();
        }
    }

    Binding {
        target: pane.view
        property: "partHasFocus"
        value: findField.activeFocus
        when: pane.view !== null
    }

    Item {
        id: findBarPart
        parent: pane.view
        // The facade's stacking (TerminalView raises the search bar).
        z: findBar.stackOrder + 1
        x: findBar.geometry.x
        y: findBar.geometry.y
        width: findBar.geometry.width
        height: findBar.geometry.height
        visible: findBar.shown

        Rectangle {
            anchors.fill: parent
            color: findBar.backgroundColor
        }

        // The size grip (Silver draws nothing for it).
        MouseArea {
            x: findBar.gripGeometry.x
            y: findBar.gripGeometry.y
            width: findBar.gripGeometry.width
            height: findBar.gripGeometry.height
            cursorShape: Qt.SizeHorCursor
            onPressed: mouse => findBar.gripPress(mapToGlobal(mouse.x, mouse.y))
            onPositionChanged: mouse => findBar.gripMove(mapToGlobal(mouse.x, mouse.y), mouse.buttons)
            onReleased: findBar.gripRelease()
        }

        Item {
            id: editPart
            x: findBar.editGeometry.x
            y: findBar.editGeometry.y
            width: findBar.editGeometry.width
            height: findBar.editGeometry.height

            // StyleElement, not StyleItem: Silver draws the frame from the line edit's light
            // WindowText and dark Text, which Kirigami's single text colour can't carry.
            StyleElement {
                anchors.fill: parent
                element: "lineedit"
                stylePalette: findBar.editPalette
                styleFont: findBar.editFont
                lineWidth: findBar.editFrameWidth
                focused: findField.activeFocus
                hover: editMouse.containsMouse
            }
            MouseArea {
                id: editMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
                cursorShape: Qt.IBeamCursor
            }

            TextInput {
                id: findField
                x: findBar.textGeometry.x
                y: findBar.textGeometry.y
                width: findBar.textGeometry.width
                height: findBar.textGeometry.height
                clip: true
                font: findBar.editFont
                color: findBar.editTextColor
                selectionColor: findBar.selectionColor
                selectedTextColor: findBar.selectedTextColor
                selectByMouse: true
                renderType: Text.NativeRendering
                text: findBar.text
                // Typing edits the real line edit; its timer emits searchChanged().
                onTextEdited: findBar.setText(text)
                Keys.onPressed: event => {
                    if (findBar.key(event.key, event.modifiers, event.text)) {
                        event.accepted = true;
                    }
                }

                Text {
                    anchors.fill: parent
                    visible: findField.text === "" && findField.preeditText === ""
                    text: findBar.placeholderText
                    color: findBar.placeholderColor
                    font: findBar.editFont
                    elide: Text.ElideRight
                    renderType: Text.NativeRendering
                }
            }
            Connections {
                target: findBar
                function onTextChanged() {
                    if (findField.text !== findBar.text) {
                        findField.text = findBar.text;
                    }
                }
            }

            // QLineEdit's clear button fades in with text in 160 ms (QLineEditIconButton).
            PixmapItem {
                x: findBar.clearButtonGeometry.x
                y: findBar.clearButtonGeometry.y
                width: findBar.clearButtonGeometry.width
                height: findBar.clearButtonGeometry.height
                image: findBar.clearButtonImage
                opacity: findBar.clearButtonShown ? 1 : 0
                visible: opacity > 0
                Behavior on opacity {
                    NumberAnimation {
                        duration: 160
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: findBar.clear()
                }
            }

            Controls.ToolTip.text: findBar.editToolTip
            Controls.ToolTip.visible: editMouse.containsMouse && findBar.editToolTip !== ""
            Controls.ToolTip.delay: headerBar.toolTipDelay
        }

        Repeater {
            model: findBar.buttons
            delegate: Item {
                id: findButton
                required property var modelData
                required property int index
                x: modelData.geometry.x
                y: modelData.geometry.y
                width: modelData.geometry.width
                height: modelData.geometry.height

                StylePrivate.StyleItem {
                    anchors.fill: parent
                    elementType: "toolbutton"
                    flat: true
                    hover: findButtonMouse.containsMouse
                    sunken: findButtonMouse.pressed && findButtonMouse.containsMouse
                    on: findButton.modelData.checked
                    enabled: findButton.modelData.enabled
                    properties: ({
                            icon: findButton.modelData.iconName,
                            iconColor: findBar.textColor,
                            iconWidth: findButton.modelData.iconWidth,
                            iconHeight: findButton.modelData.iconHeight,
                            toolButtonStyle: Qt.ToolButtonIconOnly,
                            menu: findButton.modelData.menu
                        })
                }
                MouseArea {
                    id: findButtonMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: findBar.clickButton(findButton.index, findButton.mapToGlobal(0, findButton.height))
                }
                Controls.ToolTip.text: modelData.toolTip
                Controls.ToolTip.visible: findButtonMouse.containsMouse && modelData.toolTip !== ""
                Controls.ToolTip.delay: headerBar.toolTipDelay
            }
        }
    }

    Item {
        id: headerBarPart
        parent: pane.view
        z: headerBar.stackOrder + 1
        x: headerBar.geometry.x
        y: headerBar.geometry.y
        width: headerBar.geometry.width
        height: headerBar.geometry.height
        visible: headerBar.shown

        // TerminalHeaderBar paints its background, the base line and the unfocused shade
        // under its children.
        Rectangle {
            anchors.fill: parent
            color: headerBar.backgroundColor
        }
        StyleElement {
            anchors.fill: parent
            visible: pane.headerTabBarBase
            element: "tabbarbase"
            shape: 1 // QTabBar::RoundedSouth
            stylePalette: headerBar.palette
        }
        Rectangle {
            anchors.fill: parent
            visible: !headerBar.focused
            color: headerBar.shadowColor
        }

        MouseArea {
            anchors.fill: parent
            onPressed: mouse => headerBar.press(Qt.point(mouse.x, mouse.y))
            onPositionChanged: mouse => headerBar.move(Qt.point(mouse.x, mouse.y))
            onDoubleClicked: mouse => headerBar.doubleClick(mouse.button)
        }

        Repeater {
            model: headerBar.pixmaps
            delegate: PixmapItem {
                required property var modelData
                x: modelData.geometry.x
                y: modelData.geometry.y
                width: modelData.geometry.width
                height: modelData.geometry.height
                image: modelData.image
                alignment: modelData.alignment
            }
        }

        Text {
            x: headerBar.titleGeometry.x
            y: headerBar.titleGeometry.y
            width: headerBar.titleGeometry.width
            height: headerBar.titleGeometry.height
            text: headerBar.title
            font: headerBar.titleFont
            color: headerBar.titleColor
            verticalAlignment: Text.AlignVCenter
            textFormat: Text.PlainText
            renderType: Text.NativeRendering
        }

        Repeater {
            model: headerBar.buttons
            delegate: Item {
                id: button
                required property var modelData
                required property int index
                x: modelData.geometry.x
                y: modelData.geometry.y
                width: modelData.geometry.width
                height: modelData.geometry.height

                StylePrivate.StyleItem {
                    anchors.fill: parent
                    elementType: "toolbutton"
                    flat: true
                    hover: buttonMouse.containsMouse
                    sunken: buttonMouse.pressed && buttonMouse.containsMouse
                    on: button.modelData.checked
                    enabled: button.modelData.enabled
                    properties: ({
                            icon: button.modelData.iconName,
                            // Silver colours the widget's icons with the header's (terminal) text colour.
                            iconColor: headerBar.textColor,
                            iconWidth: button.modelData.iconWidth,
                            iconHeight: button.modelData.iconHeight,
                            toolButtonStyle: Qt.ToolButtonIconOnly
                        })
                }
                MouseArea {
                    id: buttonMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: headerBar.clickButton(button.index)
                }
                Controls.ToolTip.text: modelData.toolTip
                Controls.ToolTip.visible: buttonMouse.containsMouse && modelData.toolTip !== ""
                Controls.ToolTip.delay: headerBar.toolTipDelay
            }
        }
    }

    Item {
        id: scrollBarPart
        parent: pane.view
        z: scrollBar.stackOrder + 1
        x: scrollBar.geometry.x
        y: scrollBar.geometry.y
        width: scrollBar.geometry.width
        height: scrollBar.geometry.height
        visible: scrollBar.shown

        StylePrivate.StyleItem {
            anchors.fill: parent
            elementType: "scrollbar"
            horizontal: false
            minimum: 0
            maximum: scrollBar.styleMaximum
            value: scrollBar.styleValue
            activeControl: scrollBar.activeControl
            sunken: scrollBar.sunken
            hover: scrollBar.hovered
            enabled: scrollBar.enabled
            // The facade's bar takes the terminal's colours under some styles.
            Kirigami.Theme.inherit: !scrollBar.customPalette
            Kirigami.Theme.backgroundColor: scrollBar.backgroundColor
            Kirigami.Theme.textColor: scrollBar.textColor
        }

        Repeater {
            model: scrollBar.searchLines
            delegate: Rectangle {
                required property int modelData
                x: 2
                y: modelData
                width: scrollBarPart.width - 4
                height: 1
                color: scrollBar.searchLineColor
            }
        }

        Repeater {
            model: scrollBar.markers
            delegate: Rectangle {
                required property rect modelData
                x: modelData.x
                y: modelData.y
                width: modelData.width
                height: modelData.height
                color: scrollBar.markerColor
                opacity: 0.75
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
            onPressed: mouse => {
                if (mouse.button === Qt.RightButton) {
                    mouse.accepted = scrollBar.contextMenu(Qt.point(mouse.x, mouse.y), mapToGlobal(mouse.x, mouse.y));
                } else {
                    mouse.accepted = scrollBar.press(Qt.point(mouse.x, mouse.y), mouse.button, mouse.buttons, mouse.modifiers, mouse.flags);
                }
            }
            onPositionChanged: mouse => scrollBar.move(Qt.point(mouse.x, mouse.y), mouse.buttons)
            onReleased: mouse => scrollBar.release(mouse.buttons)
            onCanceled: scrollBar.release(0)
            onExited: scrollBar.exit()
            onWheel: wheel => {
                wheel.accepted = scrollBar.wheel(wheel.angleDelta, wheel.modifiers);
            }
        }

        Controls.ToolTip {
            x: scrollBar.toolTipPosition.x
            y: scrollBar.toolTipPosition.y
            visible: scrollBar.toolTip !== ""
            text: scrollBar.toolTip
        }
    }
}
