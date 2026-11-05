/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// The Configure dialog's Thumbnails page (ThumbnailsSettings.ui).
DialogPage {
    id: root

    required property SettingsEditor settings

    title: i18nc("@title Preferences page name", "Thumbnails")
    iconName: "image-jpeg"

    component Setting: SettingCheckBox {
        settings: root.settings
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2

        Setting {
            Layout.columnSpan: 2
            setting: "EnableThumbnails"
            text: i18n("Enable thumbnails generation")
        }
        QQC2.Label {
            Layout.alignment: Qt.AlignRight
            text: i18n("Size:")
        }
        SettingSpinBox {
            settings: root.settings
            setting: "ThumbnailSize"
            from: 16
            to: 1024
            suffix: i18n("px")
        }
        QQC2.Label {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            Layout.preferredWidth: Kirigami.Units.gridUnit * 20
            wrapMode: Text.WordWrap
            textFormat: Text.RichText
            text: i18n("<html><head/><body><p>To use this feature, enable <span style=\" font-weight:600;\">Mouse-&gt;Miscellaneous-&gt;Underline files</span> in your profile settings.</p></body></html>")
        }
        QQC2.Label {
            Layout.alignment: Qt.AlignRight
            text: i18n("Activation:")
        }
        QQC2.Label {
            text: i18n("Mouse hover plus")
        }
        Item {}
        Setting {
            setting: "ThumbnailShift"
            text: i18n("Shift")
        }
        Item {}
        Setting {
            setting: "ThumbnailAlt"
            text: i18n("Alt")
        }
        Item {}
        Setting {
            setting: "ThumbnailCtrl"
            text: i18n("Ctrl")
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
