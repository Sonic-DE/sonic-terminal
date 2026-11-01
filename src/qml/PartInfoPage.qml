/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// The KPart settings dialog's Part Info page (PartInfo.ui).
DialogPage {
    title: i18nc("@title Preferences page name", "Part Info")
    iconName: "dialog-information"

    QQC2.Label {
        Layout.fillWidth: true
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        wrapMode: Text.WordWrap
        textFormat: Text.RichText
        text: i18n("<html><head/><body><p>Applications that use KonsolePart share profiles.</p><p>They do not share with Konsole or other applications:<ul><li>default profile</li><li>show in menu</li><li>shortcuts</li></ul></p><p>Feel free to open a Konsole <a href=\"https://bugs.kde.org/enter_bug.cgi?format=guided&product=konsole&component=kpart\">bug report</a> if you desire a change to how these are handled.</body></html>")
    }

    Item {
        Layout.fillHeight: true
    }
}
