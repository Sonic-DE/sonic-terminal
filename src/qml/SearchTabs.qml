/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// The tab search (searchtabs/SearchTabs) of a compat window's QML window, in the window's
// coordinates: a search field over the matching tab titles; Return or a click switches to
// the tab, Escape or leaving the field closes it.
QQC2.Frame {
    id: search

    required property QtObject compatWindow

    SearchTabsAdapter {
        id: tabs
        window: search.compatWindow
    }

    x: tabs.geometry.x
    y: tabs.geometry.y
    width: tabs.geometry.width
    height: tabs.geometry.height
    visible: tabs.open
    padding: 0
    onVisibleChanged: {
        if (visible) {
            field.text = "";
            field.forceActiveFocus();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        QQC2.TextField {
            id: field
            Layout.fillWidth: true
            leftPadding: icon.width + Kirigami.Units.largeSpacing
            placeholderText: tabs.placeholderText
            QQC2.ToolTip.text: tabs.toolTip
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            onTextEdited: tabs.text = text
            // SearchTabs::eventFilter(): navigation keys go to the list.
            Keys.onUpPressed: tabs.currentIndex = tabs.currentIndex - 1
            Keys.onDownPressed: tabs.currentIndex = tabs.currentIndex + 1
            Keys.onPressed: event => {
                const page = Math.max(1, Math.floor(list.height / (list.currentItem ? list.currentItem.height : 1)) - 1);
                if (event.key === Qt.Key_PageUp) {
                    tabs.currentIndex = tabs.currentIndex - page;
                    event.accepted = true;
                } else if (event.key === Qt.Key_PageDown) {
                    tabs.currentIndex = tabs.currentIndex + page;
                    event.accepted = true;
                } else if (event.key === Qt.Key_Escape) {
                    tabs.close();
                    event.accepted = true;
                }
            }
            onAccepted: tabs.activate()
            onActiveFocusChanged: {
                if (!activeFocus && tabs.open) {
                    tabs.close();
                }
            }

            Kirigami.Icon {
                id: icon
                source: "search"
                width: Kirigami.Units.iconSizes.small
                height: width
                anchors.left: parent.left
                anchors.leftMargin: Kirigami.Units.smallSpacing
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: tabs.entries
            currentIndex: tabs.currentIndex
            highlightMoveDuration: 0
            delegate: QQC2.ItemDelegate {
                required property var modelData
                required property int index
                width: ListView.view.width
                highlighted: index === tabs.currentIndex
                // The title's end is kept visible (QTreeView::setTextElideMode(Qt::ElideLeft)).
                contentItem: QQC2.Label {
                    text: modelData.name
                    elide: Text.ElideLeft
                    color: parent.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                }
                // A press would take focus from the field, which closes the search first.
                focusPolicy: Qt.NoFocus
                onClicked: {
                    tabs.currentIndex = index;
                    tabs.activate();
                }
            }
        }
    }
}
