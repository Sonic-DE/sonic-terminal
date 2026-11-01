/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// A QLabel with a buddy: "&Name" underlines N while Alt is held and Alt+N focuses the buddy.
QQC2.Label {
    id: root

    property string label
    property Item buddy

    Kirigami.MnemonicData.enabled: buddy !== null && buddy.enabled && buddy.visible
    Kirigami.MnemonicData.controlType: Kirigami.MnemonicData.FormLabel
    Kirigami.MnemonicData.label: label
    text: Kirigami.MnemonicData.richTextLabel
    Accessible.name: Kirigami.MnemonicData.plainTextLabel

    Shortcut {
        enabled: root.Kirigami.MnemonicData.enabled
        sequence: root.Kirigami.MnemonicData.sequence
        onActivated: root.buddy.forceActiveFocus(Qt.ShortcutFocusReason)
    }
}
