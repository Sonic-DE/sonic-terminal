/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import org.sonicde.terminal

// A compat window's central area: each terminal where its (hidden) tab widget and splitters
// put it, and the splitter handles between them. Placed at the central widget's geometry.
Item {
    id: tree

    required property QtObject compatWindow
    // Where the compat window's central widget is, in the window.
    readonly property rect centralGeometry: panes.centralGeometry
    // Around the shown terminals, in this item.
    readonly property rect shownPanesGeometry: panes.shownPanesGeometry

    PaneTreeAdapter {
        id: panes
        window: tree.compatWindow
    }

    // Under the terminals, as QSplitter's handles are under its widgets where they overlap.
    Repeater {
        model: panes.handles
        delegate: StyleElement {
            required property var modelData
            x: modelData.geometry.x
            y: modelData.geometry.y
            width: modelData.geometry.width
            height: modelData.geometry.height
            element: "splitter"
            shape: modelData.orientation
            stylePalette: modelData.palette
        }
    }

    Repeater {
        model: panes
        delegate: TerminalPane {
            required property rect geometry
            required property bool shown
            required property var model
            headerTabBarBase: model.headerTabBarBase
            x: geometry.x
            y: geometry.y
            width: geometry.width
            height: geometry.height
            visible: shown
            Component.onCompleted: view.followsFacade = true
        }
    }
}
