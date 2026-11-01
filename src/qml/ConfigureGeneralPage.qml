/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// The Configure dialog's General page (GeneralSettings.ui).
DialogPage {
    id: root

    required property SettingsEditor settings
    // The progress check box needs D-Bus (HAVE_DBUS).
    property bool hasDBus: true

    title: i18nc("@title Preferences page name", "General")
    iconName: "utilities-terminal"

    component SectionLabel: QQC2.Label {
        Layout.alignment: Qt.AlignRight
    }
    component Setting: SettingCheckBox {
        settings: root.settings
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2

        SectionLabel {
            text: i18n("Process and window:")
        }
        Setting {
            setting: "RememberWindowSize"
            text: i18n("Remember window size")
            toolTip: i18n("If enabled, profile settings will be ignored")
        }
        Item {}
        Setting {
            setting: "UseSingleInstance"
            text: i18n("Run all Konsole windows in a single process")
            toolTip: i18n("When launching Konsole re-use existing process if possible")
        }
        Item {}
        Setting {
            setting: "AllowMenuAccelerators"
            text: i18n("Enable menu accelerators")
        }
        Item {}
        Setting {
            setting: "ShowWindowTitleOnTitleBar"
            text: i18n("Show window title on the titlebar")
        }
        Item {}
        Setting {
            setting: "RemoveWindowTitleBarAndFrame"
            text: i18n("Remove window titlebar and frame")
        }
        Item {}
        Setting {
            setting: "FocusFollowsMouse"
            text: i18n("Focus terminals when the mouse pointer is moved over them")
        }
        Item {}
        Setting {
            setting: "EnableSecuritySensitiveDBusAPI"
            text: i18n("Enable the security sensitive parts of the DBus API")
        }
        Item {}
        Setting {
            setting: "ForceNewTabs"
            text: i18n("Force new tabs")
        }
        Item {
            visible: root.hasDBus
        }
        Setting {
            setting: "ShowProgressInTaskBar"
            text: i18n("Show progress in taskbar")
            visible: root.hasDBus
        }

        Item {
            Layout.columnSpan: 2
            Layout.preferredHeight: Kirigami.Units.gridUnit
        }

        SectionLabel {
            text: i18nc("@item:intext Search options", "Search:")
        }
        Setting {
            setting: "SearchCaseSensitive"
            text: i18n("Case sensitive")
            toolTip: i18n("Sets whether the search is case sensitive")
        }
        Item {}
        Setting {
            setting: "SearchRegExpression"
            text: i18n("Match using regular expressions")
        }
        Item {}
        Setting {
            setting: "SearchHighlightMatches"
            text: i18n("Highlight all matches")
        }
        Item {}
        Setting {
            setting: "SearchReverseSearch"
            text: i18n("Search backwards")
            toolTip: i18n("Sets whether search should start from the bottom")
        }
        Item {}
        Setting {
            setting: "SearchNoWrap"
            text: i18n("No wrap")
            toolTip: i18n("Sets whether search should stop instead of wrapping")
        }

        Item {
            Layout.columnSpan: 2
            Layout.preferredHeight: Kirigami.Units.gridUnit
        }

        SectionLabel {
            text: i18n("Notifications:")
        }
        QQC2.Button {
            objectName: "enableAllMessagesButton"
            text: i18n("Enable all \"Don't Ask Again\" messages")
            onClicked: DialogHelpers.enableAllMessages()
            QQC2.ToolTip.text: i18n("All dialogs will be shown again")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
