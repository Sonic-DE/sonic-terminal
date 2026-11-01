/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import org.kde.kirigami as Kirigami

// A QLineEdit with setClearButtonEnabled(true).
Kirigami.ActionTextField {
    id: root

    rightActions: Kirigami.Action {
        icon.name: root.mirrored ? "edit-clear-locationbar-ltr" : "edit-clear-locationbar-rtl"
        visible: root.text.length > 0 && root.enabled && !root.readOnly
        onTriggered: root.clear()
    }
}
