/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.kde.kirigami as Kirigami

// A plugin panel's QTreeView without a header over a tree model: the current row, clicks
// reported by button, a double click (which expands a branch unless it edits), a context
// menu request, and a row's text edited in place.
QQC2.Frame {
    id: root

    property alias model: tree.model
    // What a double click on a row edits, when it does.
    property var canEdit: index => false
    readonly property var currentIndex: selection.currentIndex
    readonly property bool hasCurrent: selection.hasSelection

    signal itemClicked(var index, int button)
    signal itemDoubleClicked(var index)
    signal contextMenuRequested(var index, point globalPosition)

    function expandAll() {
        tree.expandRecursively();
    }

    function setCurrent(index) {
        selection.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect);
    }

    // The row's text in an editor; Return or leaving it sets it, Escape keeps the old one.
    function edit(index) {
        tree.editingRow = tree.rowAtIndex(index);
    }

    function click(index, button, position) {
        setCurrent(index);
        tree.forceActiveFocus();
        if (button === Qt.RightButton) {
            contextMenuRequested(index, position);
        } else {
            itemClicked(index, button);
        }
    }

    function doubleClick(index) {
        setCurrent(index);
        if (canEdit(index)) {
            edit(index);
        } else {
            tree.toggleExpanded(tree.rowAtIndex(index));
        }
        itemDoubleClicked(index);
    }

    function find(text, parent) {
        for (let row = 0; row < model.rowCount(parent); ++row) {
            const index = model.index(row, 0, parent);
            if (model.data(index) === text) {
                return index;
            }
            const inner = find(text, index);
            if (inner) {
                return inner;
            }
        }
        return null;
    }

    // The parity hook's clickItem by text: "click", "double" or "context", on the row shown
    // (as QAbstractItemView::scrollTo() expands to it).
    function clickItemByText(text, kind) {
        const index = find(text, tree.rootIndex);
        if (!index) {
            return false;
        }
        tree.expandToIndex(index);
        tree.forceLayout();
        if (kind === "double") {
            click(index, Qt.LeftButton);
            doubleClick(index);
        } else if (kind === "context") {
            const global = root.mapToGlobal(0, 0);
            click(index, Qt.RightButton, Qt.point(global.x, global.y));
        } else {
            click(index, Qt.LeftButton);
        }
        return true;
    }

    padding: 1
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2)
    }

    contentItem: TreeView {
        id: tree

        property int editingRow: -1

        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // The rows handle their own clicks.
        pointerNavigationEnabled: false
        columnWidthProvider: () => tree.width
        onWidthChanged: forceLayout()
        selectionModel: ItemSelectionModel {
            id: selection
            model: tree.model
        }
        QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

        delegate: Rectangle {
            id: row

            required property TreeView treeView
            required property bool isTreeNode
            required property bool expanded
            required property bool hasChildren
            required property int depth
            required property int row
            required property int column
            required property bool current
            required property var model
            readonly property int indentation: 20
            readonly property var itemIndex: treeView.index(row, column)

            implicitWidth: treeView.width
            implicitHeight: label.implicitHeight + 2
            color: current ? Kirigami.Theme.highlightColor : "transparent"

            QQC2.ToolTip.text: row.model.toolTip ?? ""
            QQC2.ToolTip.visible: hover.hovered && QQC2.ToolTip.text.length > 0
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

            HoverHandler {
                id: hover
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                onSingleTapped: (eventPoint, button) => {
                    const global = row.mapToGlobal(eventPoint.position);
                    root.click(row.itemIndex, button, Qt.point(global.x, global.y));
                }
                onDoubleTapped: (eventPoint, button) => {
                    if (button === Qt.LeftButton) {
                        root.doubleClick(row.itemIndex);
                    }
                }
            }

            Kirigami.Icon {
                x: row.depth * row.indentation + (row.indentation - width) / 2
                anchors.verticalCenter: parent.verticalCenter
                width: Kirigami.Units.iconSizes.small / 2
                height: width
                visible: row.hasChildren
                source: row.expanded ? "arrow-down" : "arrow-right"
                color: row.current ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor

                TapHandler {
                    onTapped: row.treeView.toggleExpanded(row.row)
                }
            }
            QQC2.Label {
                id: label

                x: (row.depth + 1) * row.indentation
                width: parent.width - x
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                visible: tree.editingRow !== row.row
                color: row.current ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                text: row.model.display ?? ""
            }
            Loader {
                x: label.x
                width: label.width
                height: parent.height
                active: tree.editingRow === row.row
                sourceComponent: QQC2.TextField {
                    function commit() {
                        if (tree.editingRow === row.row) {
                            tree.editingRow = -1;
                            root.model.setData(row.itemIndex, text, Qt.EditRole);
                        }
                    }

                    text: row.model.display ?? ""
                    Component.onCompleted: {
                        forceActiveFocus();
                        selectAll();
                    }
                    Keys.onEscapePressed: tree.editingRow = -1
                    onAccepted: commit()
                    onActiveFocusChanged: {
                        if (!activeFocus) {
                            commit();
                        }
                    }
                }
            }
        }
    }
}
