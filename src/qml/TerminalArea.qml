/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import org.sonicde.terminal
import org.sonicde.terminal.app

// A terminal for another program's QML: the host's API, its panes filling it.
TerminalHost {
    id: host

    focus: true
    clip: true

    Loader {
        anchors.fill: parent
        active: host.compatWindow !== null
        focus: true
        sourceComponent: PaneTree {
            compatWindow: host.compatWindow
        }
    }
}
