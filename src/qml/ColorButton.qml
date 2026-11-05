/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// KColorButton: a button showing its colour; QColorDialog changes it.
QQC2.Button {
    id: root

    property color color

    // KColorButton::changed(): the user chose a colour.
    signal colorEdited(color color)

    implicitWidth: Kirigami.Units.gridUnit * 4
    onClicked: {
        const chosen = DialogHelpers.chooseColor(color, root);
        if (chosen) {
            color = chosen;
            colorEdited(chosen);
        }
    }

    contentItem: Item {
        implicitHeight: Kirigami.Units.iconSizes.small

        Rectangle {
            anchors.fill: parent
            color: root.color
            border.color: Kirigami.Theme.textColor
            border.width: 1
            opacity: root.enabled ? 1 : 0.5
        }
    }
}
