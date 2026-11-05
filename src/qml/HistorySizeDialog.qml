/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.kde.kirigami as Kirigami

// HistorySizeDialog ("Adjust Scrollback"; SessionController::showHistoryOptions()).
DialogWindow {
    id: root

    property alias mode: fields.mode
    property int initialLineCount: 1000
    readonly property alias lineCount: fields.lineCount

    title: i18nc("@title:window", "Adjust Scrollback")
    standardButtons: QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Cancel
    // After the initial properties, which the fields' own completion may precede.
    Component.onCompleted: fields.setLineCount(initialLineCount)

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        visible: true
        type: Kirigami.MessageType.Information
        showCloseButton: false
        text: i18nc("@info:status", "Any adjustments are only temporary to this session.")
    }

    HistorySizeFields {
        id: fields

        Layout.fillWidth: true
    }
}
