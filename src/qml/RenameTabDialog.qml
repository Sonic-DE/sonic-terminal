/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2

// RenameTabDialog ("Tab Properties"; SessionController::renameSession()).
DialogWindow {
    id: root

    property alias tabTitle: fields.tabTitle
    property alias remoteTabTitle: fields.remoteTabTitle
    property color initialColor
    property color initialActivityColor
    readonly property alias color: fields.color
    readonly property alias activityColor: fields.activityColor
    // The field focused first: the remote one for a remote session.
    property bool focusRemote: false

    title: i18n("Tab Properties")
    standardButtons: QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Cancel

    TabTitleFields {
        id: fields

        Component.onCompleted: {
            setColor(root.initialColor);
            setActivityColor(root.initialActivityColor);
            (root.focusRemote ? remoteTabTitleEdit : tabTitleEdit).forceActiveFocus();
        }
    }
}
