/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// The Configure dialog's Tab Bar / Splitters page (TabBarSettings.ui): Appearance,
// Behavior and Splits tabs.
DialogPage {
    id: root

    required property SettingsEditor settings
    // The tab bar options are disabled while it is never shown.
    readonly property bool tabBarShown: !alwaysHideTabBar.checked

    title: i18nc("@title Preferences page name", "Tab Bar / Splitters")
    iconName: "preferences-tabs"

    component SectionLabel: QQC2.Label {
        Layout.alignment: Qt.AlignRight
    }
    component Choice: SettingRadioButton {
        settings: root.settings
    }
    component Setting: SettingCheckBox {
        settings: root.settings
    }
    component Gap: Item {
        Layout.columnSpan: 2
        Layout.preferredHeight: Kirigami.Units.largeSpacing
    }

    QQC2.TabBar {
        id: tabs

        objectName: "tabs"
        Layout.fillWidth: true

        QQC2.TabButton {
            text: i18nc("@title:tab Tab bar settings", "Appearance")
        }
        QQC2.TabButton {
            text: i18nc("@title:tab Tab bar settings", "Behavior")
        }
        QQC2.TabButton {
            text: i18n("Splits")
        }
    }

    StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: tabs.currentIndex

        GridLayout {
            objectName: "appearanceTab"
            columns: 2

            SectionLabel {
                text: i18n("Show:")
            }
            Choice {
                setting: "TabBarVisibility"
                choice: "ShowTabBarWhenNeeded"
                text: i18n("When needed")
            }
            Item {}
            Choice {
                setting: "TabBarVisibility"
                choice: "AlwaysShowTabBar"
                text: i18n("Alwa&ys")
            }
            Item {}
            Choice {
                id: alwaysHideTabBar

                setting: "TabBarVisibility"
                choice: "AlwaysHideTabBar"
                text: i18n("&Never")
            }
            Gap {}

            SectionLabel {
                text: i18n("Position:")
                enabled: root.tabBarShown
            }
            Choice {
                setting: "TabBarPosition"
                choice: "Top"
                text: i18n("Above terminal area")
                enabled: root.tabBarShown
            }
            Item {}
            Choice {
                setting: "TabBarPosition"
                choice: "Bottom"
                text: i18n("Be&low terminal area")
                enabled: root.tabBarShown
            }
            Item {}
            Choice {
                setting: "TabBarPosition"
                choice: "Left"
                text: i18n("To the left of terminal area")
                enabled: root.tabBarShown
            }
            Item {}
            Choice {
                setting: "TabBarPosition"
                choice: "Right"
                text: i18n("To the &right of terminal area")
                enabled: root.tabBarShown
            }
            Gap {}

            SectionLabel {
                text: i18n("Show Close Tab button:")
                enabled: root.tabBarShown
            }
            Choice {
                setting: "CloseTabButton"
                choice: "OnEachTab"
                text: i18n("On ea&ch tab")
                enabled: root.tabBarShown
            }
            Item {}
            Choice {
                setting: "CloseTabButton"
                choice: "OnTabBar"
                text: i18n("On &the tab bar")
                enabled: root.tabBarShown
            }
            Item {}
            Choice {
                setting: "CloseTabButton"
                choice: "None"
                text: i18nc("Do not show a close button", "None")
                enabled: root.tabBarShown
            }
            Gap {}

            SectionLabel {
                text: i18n("Show Search Tabs button:")
                enabled: root.tabBarShown
            }
            Choice {
                setting: "SearchTabsButton"
                choice: "ShowSearchTabsButton"
                text: i18n("Always")
                enabled: root.tabBarShown
            }
            Item {}
            Choice {
                setting: "SearchTabsButton"
                choice: "HideSearchTabsButton"
                text: i18n("Never")
                enabled: root.tabBarShown
            }
            Gap {}

            SectionLabel {
                text: i18nc("@item:intext Miscellaneous Options", "Miscellaneous:")
                enabled: root.tabBarShown
            }
            Setting {
                setting: "NewTabButton"
                text: i18n("Show 'New Tab' button")
            }
            Item {}
            Setting {
                setting: "ExpandTabWidth"
                text: i18n("Expand individual tab widths to full window")
                enabled: root.tabBarShown
            }
            Item {
                Layout.columnSpan: 2
                Layout.fillHeight: true
            }
        }

        GridLayout {
            objectName: "behaviorTab"
            columns: 2

            SectionLabel {
                text: i18n("Put new tabs:")
            }
            Choice {
                setting: "NewTabBehavior"
                choice: "PutNewTabAtTheEnd"
                text: i18n("At &the end")
            }
            Item {}
            Choice {
                setting: "NewTabBehavior"
                choice: "PutNewTabAfterCurrentTab"
                text: i18n("After current &tab")
            }
            Gap {}

            SectionLabel {
                text: i18nc("@item:intext Miscellaneous Options", "Miscellaneous:")
                enabled: root.tabBarShown
            }
            Setting {
                setting: "CloseTabOnMiddleMouseButton"
                text: i18n("Close tab on middle-click")
                enabled: root.tabBarShown
            }
            Item {
                Layout.columnSpan: 2
                Layout.fillHeight: true
            }
        }

        GridLayout {
            objectName: "splitTab"
            columns: 2

            SectionLabel {
                text: i18n("Show Header:")
            }
            Choice {
                setting: "SplitViewVisibility"
                choice: "ShowSplitHeaderWhenNeeded"
                text: i18nc("@option:radio When needed show the split header", "When needed")
            }
            Item {}
            Choice {
                setting: "SplitViewVisibility"
                choice: "AlwaysShowSplitHeader"
                text: i18nc("@option:radio Always show the split header", "Always")
            }
            Item {}
            Choice {
                setting: "SplitViewVisibility"
                choice: "AlwaysHideSplitHeader"
                text: i18nc("@option:radio Never show the split header", "Never")
            }
            Gap {}

            SectionLabel {
                text: i18n("Drag Handle Size:")
            }
            Choice {
                setting: "SplitDragHandleSize"
                choice: "SplitDragHandleSmall"
                text: i18nc("@option:radio Small height/width of splitter widget", "Small")
            }
            Item {}
            Choice {
                setting: "SplitDragHandleSize"
                choice: "SplitDragHandleMedium"
                text: i18nc("@option:radio Medium height/width of splitter widget", "Medium")
            }
            Item {}
            Choice {
                setting: "SplitDragHandleSize"
                choice: "SplitDragHandleLarge"
                text: i18nc("@option:radio Large height/width of splitter widget", "Large")
            }
            Item {
                Layout.columnSpan: 2
                Layout.fillHeight: true
            }
        }
    }
}
