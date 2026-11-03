/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// CopyInputDialog ("Copy Input"; SessionController::copyInputToSelectedTabs()): the sessions
// with check boxes, filtered; Select All / Deselect All act on the selected rows when
// more than one is selected, else on all.
DialogWindow {
    id: root

    required property CopyInputModel sessions
    // Rows selected as in an extended-selection QTreeView.
    property var selectedRows: []
    property int anchorRow: -1

    function select(row, modifiers) {
        if (modifiers & Qt.ShiftModifier && anchorRow >= 0) {
            const rows = [];
            for (let i = Math.min(anchorRow, row); i <= Math.max(anchorRow, row); ++i) {
                rows.push(i);
            }
            selectedRows = rows;
        } else if (modifiers & Qt.ControlModifier) {
            selectedRows = selectedRows.includes(row) ? selectedRows.filter(r => r !== row) : selectedRows.concat([row]);
            anchorRow = row;
        } else {
            selectedRows = [row];
            anchorRow = row;
        }
        sessionList.currentIndex = row;
    }

    title: i18n("Copy Input")
    standardButtons: QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Cancel

    RowLayout {
        Layout.fillWidth: true

        MnemonicLabel {
            label: i18n("Filter:")
            buddy: filterEdit
        }
        ClearableTextField {
            id: filterEdit

            objectName: "filterEdit"
            Layout.fillWidth: true
            focus: true
            onTextChanged: {
                root.sessions.filterText = text;
                root.selectedRows = [];
                root.anchorRow = -1;
            }
        }
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        // QListView's size hint, which sizes the widget dialog.
        Layout.preferredWidth: 256
        Layout.preferredHeight: 192
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2)
        }

        ListView {
            id: sessionList

            objectName: "sessionList"
            model: root.sessions
            clip: true
            activeFocusOnTab: true
            currentIndex: -1

            Keys.onSpacePressed: {
                if (currentIndex >= 0) {
                    const item = itemAtIndex(currentIndex);
                    if (item && item.enabled) {
                        root.sessions.setChecked(currentIndex, !item.model.checked);
                    }
                }
            }
            Keys.onUpPressed: event => root.select(Math.max(currentIndex - 1, 0), event.modifiers)
            Keys.onDownPressed: event => root.select(Math.min(currentIndex + 1, count - 1), event.modifiers)

            delegate: QQC2.ItemDelegate {
                id: row

                required property int index
                required property var model

                width: ListView.view.width
                enabled: model.checkable
                highlighted: root.selectedRows.includes(index)

                contentItem: RowLayout {
                    QQC2.CheckBox {
                        checked: row.model.checked
                        onToggled: root.sessions.setChecked(row.index, checked)
                    }
                    Kirigami.Icon {
                        source: row.model.iconName
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: row.model.title
                        elide: Text.ElideRight
                    }
                }

                TapHandler {
                    acceptedModifiers: Qt.KeyboardModifierMask
                    onTapped: {
                        sessionList.forceActiveFocus();
                        root.select(row.index, point.modifiers);
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true

        QQC2.Button {
            objectName: "selectAllButton"
            text: i18n("Select All")
            onClicked: root.sessions.setSelectionChecked(root.selectedRows, true)
        }
        QQC2.Button {
            objectName: "deselectAllButton"
            text: i18n("Deselect All")
            onClicked: root.sessions.setSelectionChecked(root.selectedRows, false)
        }
        Item {
            Layout.fillWidth: true
        }
    }
}
