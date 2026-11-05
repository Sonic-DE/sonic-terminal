/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2

// A KPageDialog with the Tabbed face: one tab per page (DialogPage children).
DialogWindow {
    id: root

    default property alias pages: stack.data
    property alias currentIndex: stack.currentIndex

    QQC2.TabBar {
        id: tabBar

        Layout.fillWidth: true
        currentIndex: stack.currentIndex
        onCurrentIndexChanged: stack.currentIndex = currentIndex

        Repeater {
            model: stack.children

            QQC2.TabButton {
                required property Item modelData

                text: modelData.title
                icon.name: modelData.iconName
            }
        }
    }

    StackLayout {
        id: stack

        Layout.fillWidth: true
        Layout.fillHeight: true
    }
}
