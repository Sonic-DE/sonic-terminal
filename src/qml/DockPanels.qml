/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// Old widget plugins' docks in a compat window, in the window's coordinates: a title with a
// close button where the dock's title bar is, and the plugin's own widget embedded below it.
Item {
    id: panels

    required property QtObject compatWindow

    DockAdapter {
        id: docks
        window: panels.compatWindow
    }

    // By count: a delegate (and its WindowContainer, which holds the embedded window) lives
    // as long as its dock, not as long as one version of the list.
    Repeater {
        model: docks.docks.length
        delegate: Item {
            id: dock
            required property int index
            readonly property var modelData: docks.docks[index] ?? {}
            visible: modelData.visible ?? false

            QQC2.ToolBar {
                x: dock.modelData.titleRect?.x ?? 0
                y: dock.modelData.titleRect?.y ?? 0
                width: dock.modelData.titleRect?.width ?? 0
                height: dock.modelData.titleRect?.height ?? 0
                padding: 0
                position: QQC2.ToolBar.Header

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Kirigami.Units.smallSpacing
                    spacing: 0
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: dock.modelData.title ?? ""
                        elide: Text.ElideRight
                    }
                    QQC2.ToolButton {
                        Layout.preferredHeight: parent.height
                        Layout.preferredWidth: parent.height
                        display: QQC2.AbstractButton.IconOnly
                        icon.name: "window-close"
                        text: dock.modelData.title ?? ""
                        onClicked: docks.closeDock(dock.modelData.dock)
                    }
                }
            }

            // DockAdapter shows the widget once it is embedded here.
            WindowContainer {
                x: dock.modelData.contentRect?.x ?? 0
                y: dock.modelData.contentRect?.y ?? 0
                width: dock.modelData.contentRect?.width ?? 0
                height: dock.modelData.contentRect?.height ?? 0
                window: dock.modelData.contentWindow ?? null
            }
        }
    }
}
