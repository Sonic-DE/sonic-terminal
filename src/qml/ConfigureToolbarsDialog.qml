/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// Configure Toolbars (KEditToolBar): the toolbar chosen above, its actions and the
// available ones side by side, moved by the arrow buttons, double clicks or dragging. OK
// saves after a change, Apply saves and stays open, Defaults resets every toolbar once
// confirmed; the strings are KEditToolBar's.
DialogWindow {
    id: root

    required property KToolBarEditor editor
    // KEditToolBar's: a change was made (else OK only closes), and one is unsaved (Apply).
    property bool accepting: false
    property bool unsaved: false
    // The tooltip of the element selected last, below the lists.
    property string helpText

    signal newToolBarConfig

    function changed() {
        accepting = true;
        unsaved = true;
    }

    function insertSelected() {
        if (availableList.view.currentIndex < 0) {
            return;
        }
        const row = editor.insertAction(availableList.view.currentIndex, currentList.view.currentIndex, false);
        if (row >= 0) {
            currentList.view.currentIndex = row;
        }
    }

    // KEditToolBarWidget's drops: index is where the element was dropped in the list.
    function dropped(onCurrentList, index, fromCurrentList, row) {
        if (onCurrentList) {
            const after = index > 0 ? index - 1 : -1;
            if (fromCurrentList) {
                editor.moveAction(row, after);
            } else {
                editor.insertAction(row, after, true);
            }
        } else if (fromCurrentList) {
            editor.removeAction(row);
        }
        changed();
        editor.setCurrentToolBar(editor.currentToolBar);
    }

    function changeText() {
        const item = currentList.view.currentItem;
        if (!item) {
            return;
        }
        const row = currentList.view.currentIndex;
        const dialog = iconTextDialog.createObject(root, {
            iconText: item.model.text,
            textAlongsideIconHidden: item.model.textAlongsideIconHidden,
            transientParent: root
        });
        dialog.accepted.connect(() => root.editor.setActionIconText(row, dialog.iconText, dialog.textAlongsideIconHidden));
        dialog.visibleChanged.connect(() => {
            if (!dialog.visible) {
                dialog.destroy();
            }
        });
        dialog.show();
    }

    title: i18ndc("kxmlgui6", "@title:window", "Configure Toolbars")
    standardButtons: QQC2.DialogButtonBox.RestoreDefaults | QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Apply | QQC2.DialogButtonBox.Cancel
    acceptCheck: () => {
        if (!root.accepting) {
            root.reject();
            return false;
        }
        // Not again when Apply saved the last change.
        if (root.unsaved) {
            root.editor.save();
            root.newToolBarConfig();
        }
        return true;
    }
    onApplied: {
        editor.save();
        unsaved = false;
        newToolBarConfig();
    }
    onDefaultsRequested: {
        if (!DialogHelpers.confirmToolBarReset(root.contentItem)) {
            return;
        }
        accepting = false;
        availableList.filterField.clear();
        currentList.filterField.clear();
        editor.restoreDefaults();
        unsaved = false;
        newToolBarConfig();
    }
    Component.onCompleted: {
        const apply = standardButton(QQC2.DialogButtonBox.Apply);
        apply.enabled = Qt.binding(() => root.unsaved);
        // KStandardGuiItem's tooltips, which KEditToolBar's buttons get.
        const tooltips = [[apply, qsTranslate("KStandardGuiItem", "Apply changes")],
            [standardButton(QQC2.DialogButtonBox.RestoreDefaults), qsTranslate("KStandardGuiItem", "Reset all items to their default values")]];
        for (const [button, text] of tooltips) {
            button.QQC2.ToolTip.text = text;
            button.QQC2.ToolTip.delay = Qt.styleHints.mousePressAndHoldInterval;
            button.QQC2.ToolTip.visible = Qt.binding(() => button.hovered);
        }
        // setMinimumSize(sizeHint())
        minimumWidth = width;
        minimumHeight = height;
    }

    Connections {
        target: root.editor

        function onChanged() {
            root.changed();
        }
    }

    Component {
        id: iconTextDialog

        IconTextDialog {}
    }

    // A list of elements with its label and filter (KListWidgetSearchLine: the rows whose
    // text contains it, ignoring case, 200 ms after the last change).
    component ActionList: ColumnLayout {
        id: list

        required property QtObject model
        required property string label
        required property bool isCurrentList
        property alias view: listView
        property alias filterField: filterField
        property string filter

        signal activated

        Layout.fillWidth: true
        Layout.fillHeight: true

        MnemonicLabel {
            label: list.label
            buddy: listView
        }
        ClearableTextField {
            id: filterField

            objectName: list.isCurrentList ? "currentFilter" : "availableFilter"
            Layout.fillWidth: true
            placeholderText: i18nd("kxmlgui6", "Filter")
            onTextChanged: filterTimer.restart()

            Timer {
                id: filterTimer

                interval: 200
                onTriggered: list.filter = filterField.text
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            // QListWidget's size hint, and the minimum sizes KEditToolBarWidget gives them.
            Layout.preferredWidth: 256
            Layout.preferredHeight: list.isCurrentList ? 192 : 200
            Layout.minimumWidth: 180
            Layout.minimumHeight: list.isCurrentList ? 100 : 200

            QQC2.ScrollView {
                anchors.fill: parent
                Kirigami.StyleHints.showFramedBackground: true

                ListView {
                    id: listView

                    objectName: list.isCurrentList ? "currentList" : "availableList"
                    model: list.model
                    currentIndex: -1
                    clip: true
                    onCurrentIndexChanged: root.helpText = currentItem ? currentItem.model.statusText : ""

                    // A reload clears the selection, as clearing a QListWidget does.
                    Connections {
                        target: list.model

                        function onModelReset() {
                            listView.currentIndex = -1;
                        }
                    }

                    delegate: QQC2.ItemDelegate {
                        id: entry

                        required property var model
                        required property int index
                        readonly property bool matches: list.filter.length === 0 || model.text.toLowerCase().includes(list.filter.toLowerCase())
                        // As the widget style draws a view's selection: in the inactive colours
                        // while the view hasn't the focus.
                        readonly property var colors: listView.activeFocus ? palette.active : palette.inactive

                        width: ListView.view.width
                        visible: matches
                        height: visible ? implicitHeight : 0
                        highlighted: ListView.isCurrentItem
                        onClicked: {
                            listView.currentIndex = index;
                            listView.forceActiveFocus(Qt.MouseFocusReason);
                        }
                        onDoubleClicked: {
                            listView.currentIndex = index;
                            list.activated();
                        }

                        background: Rectangle {
                            readonly property color highlight: entry.palette.active.highlight

                            color: entry.highlighted ? entry.colors.highlight
                                : entry.hovered ? Qt.rgba(highlight.r, highlight.g, highlight.b, 0.2) : "transparent"
                        }
                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing

                            // Actions without an icon keep its room, so their texts line up.
                            Kirigami.Icon {
                                visible: entry.model.tag === "Action"
                                source: entry.model.icon
                                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                            }
                            QQC2.Label {
                                Layout.fillWidth: true
                                text: entry.model.text
                                elide: Text.ElideRight
                                color: entry.highlighted ? entry.colors.highlightedText : entry.colors.text
                            }
                        }

                        DragHandler {
                            target: null
                            onActiveChanged: {
                                if (active) {
                                    dragProxy.fromCurrentList = list.isCurrentList;
                                    dragProxy.row = entry.index;
                                    dragProxy.text = entry.model.text;
                                    dragProxy.icon = entry.model.icon;
                                    dragProxy.move(entry.mapToItem(root.contentItem, centroid.position));
                                    dragProxy.Drag.active = true;
                                } else {
                                    dragProxy.Drag.drop();
                                    dragProxy.Drag.active = false;
                                }
                            }
                            onCentroidChanged: {
                                if (active) {
                                    dragProxy.move(entry.mapToItem(root.contentItem, centroid.position));
                                }
                            }
                        }
                    }
                }
            }

            DropArea {
                anchors.fill: parent
                keys: ["application/x-kde-action-list"]
                // no internal drag in the available list
                onEntered: drag => drag.accepted = list.isCurrentList || drag.source.fromCurrentList
                onDropped: drop => {
                    const position = mapToItem(listView.contentItem, drop.x, drop.y);
                    // Between rows, as onto an item that takes no drops: above or below its middle.
                    let index = listView.indexAt(position.x, position.y);
                    if (index < 0) {
                        index = listView.count;
                    } else if (position.y > listView.itemAtIndex(index).y + listView.itemAtIndex(index).height / 2) {
                        ++index;
                    }
                    drop.accept(Qt.MoveAction);
                    root.dropped(list.isCurrentList, index, drop.source.fromCurrentList, drop.source.row);
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true

        // The toolbar, chosen only when there is more than one.
        MnemonicLabel {
            visible: toolBarCombo.visible
            label: i18nd("kxmlgui6", "&Toolbar:")
            buddy: toolBarCombo
        }
        QQC2.ComboBox {
            id: toolBarCombo

            objectName: "toolBarCombo"
            Layout.fillWidth: true
            visible: count > 1
            model: root.editor.toolBars
            currentIndex: root.editor.currentToolBar
            onActivated: index => root.editor.setCurrentToolBar(index)
        }
        Kirigami.Separator {
            Layout.fillWidth: true
            visible: toolBarCombo.visible
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ActionList {
                id: availableList

                model: root.editor.availableActions
                label: i18nd("kxmlgui6", "A&vailable actions:")
                isCurrentList: false
                onActivated: root.insertSelected()
            }

            // up above, remove left, insert right and down below, between stretching rows
            GridLayout {
                Layout.fillWidth: false
                Layout.fillHeight: true
                columns: 3
                rowSpacing: 0
                columnSpacing: 0

                Item {
                    Layout.row: 0
                    Layout.fillHeight: true
                }
                QQC2.ToolButton {
                    objectName: "upButton"
                    Layout.row: 1
                    Layout.column: 1
                    icon.name: "go-up"
                    display: QQC2.AbstractButton.IconOnly
                    // A QToolButton without autoRaise draws its frame.
                    flat: false
                    focusPolicy: Qt.TabFocus
                    autoRepeat: true
                    enabled: currentList.view.currentIndex > 0
                    onClicked: currentList.view.currentIndex = root.editor.moveUp(currentList.view.currentIndex)
                }
                QQC2.ToolButton {
                    objectName: "removeButton"
                    Layout.row: 2
                    Layout.column: 0
                    icon.name: Qt.application.layoutDirection === Qt.RightToLeft ? "go-next" : "go-previous"
                    display: QQC2.AbstractButton.IconOnly
                    flat: false
                    focusPolicy: Qt.TabFocus
                    enabled: currentList.view.currentIndex >= 0
                    onClicked: root.editor.removeAction(currentList.view.currentIndex)
                }
                QQC2.ToolButton {
                    objectName: "insertButton"
                    Layout.row: 2
                    Layout.column: 2
                    icon.name: Qt.application.layoutDirection === Qt.RightToLeft ? "go-previous" : "go-next"
                    display: QQC2.AbstractButton.IconOnly
                    flat: false
                    focusPolicy: Qt.TabFocus
                    enabled: availableList.view.currentIndex >= 0
                    onClicked: root.insertSelected()
                }
                QQC2.ToolButton {
                    objectName: "downButton"
                    Layout.row: 3
                    Layout.column: 1
                    icon.name: "go-down"
                    display: QQC2.AbstractButton.IconOnly
                    flat: false
                    focusPolicy: Qt.TabFocus
                    autoRepeat: true
                    enabled: currentList.view.currentIndex >= 0 && currentList.view.currentIndex < currentList.view.count - 1
                    onClicked: currentList.view.currentIndex = root.editor.moveDown(currentList.view.currentIndex)
                }
                Item {
                    Layout.row: 4
                    Layout.fillHeight: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ActionList {
                    id: currentList

                    model: root.editor.currentActions
                    label: i18nd("kxmlgui6", "Curr&ent actions:")
                    isCurrentList: true
                    onActivated: root.editor.removeAction(view.currentIndex)
                }
                RowLayout {
                    readonly property bool actionSelected: currentList.view.currentItem !== null && currentList.view.currentItem.model.tag === "Action"

                    Layout.fillWidth: true

                    QQC2.Button {
                        objectName: "changeIconButton"
                        text: i18ndc("kxmlgui6", "@action:button", "Change &Icon…")
                        icon.name: "preferences-desktop-icons"
                        enabled: parent.actionSelected
                        onClicked: root.editor.setActionIcon(currentList.view.currentIndex, DialogHelpers.chooseToolBarIcon(root.contentItem))
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    QQC2.Button {
                        objectName: "changeTextButton"
                        text: i18ndc("kxmlgui6", "@action:button", "Change Te&xt…")
                        icon.name: "edit-rename"
                        enabled: parent.actionSelected
                        onClicked: root.changeText()
                    }
                }
            }
        }

        QQC2.Label {
            objectName: "helpArea"
            Layout.fillWidth: true
            // A wrapped text's implicit width is its one-line width.
            Layout.preferredWidth: 0
            wrapMode: Text.WordWrap
            text: root.helpText
        }
        Kirigami.Separator {
            Layout.fillWidth: true
        }
    }

    // What a drag carries, drawn under the pointer as QListWidget's drag pixmap.
    Rectangle {
        id: dragProxy

        property bool fromCurrentList
        property int row: -1
        property string text
        property var icon

        function move(position) {
            x = position.x - width / 2;
            y = position.y - height / 2;
        }

        parent: root.contentItem
        visible: Drag.active
        z: 1
        width: proxyRow.implicitWidth + 2 * Kirigami.Units.smallSpacing
        height: proxyRow.implicitHeight + 2 * Kirigami.Units.smallSpacing
        color: Kirigami.Theme.backgroundColor
        opacity: 0.8
        Drag.keys: ["application/x-kde-action-list"]
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2

        RowLayout {
            id: proxyRow

            anchors.centerIn: parent
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: dragProxy.icon
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
            }
            QQC2.Label {
                text: dragProxy.text
            }
        }
    }
}
