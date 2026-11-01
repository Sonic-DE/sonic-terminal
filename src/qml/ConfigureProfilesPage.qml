/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrols as KQuickControls
import org.sonicde.terminal

// The Profiles settings page (ProfileSettings.ui): the profile list with names and
// shortcuts, and New, Edit, Remove and Set as Default for the selected profile.
DialogPage {
    id: root

    required property ProfileListModel profiles
    readonly property int currentRow: profileList.currentIndex
    readonly property var current: profileList.currentItem

    title: i18nc("@title Preferences page name", "Profiles")
    iconName: "preferences-desktop-theme"

    component PageButton: QQC2.Button {
        Layout.fillWidth: true
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true

        // A framed view, as ProfileSettings' tree view.
        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: Kirigami.Units.gridUnit * 16
            Kirigami.StyleHints.showFramedBackground: true

            ListView {
                id: profileList

                objectName: "profileListView"
                model: root.profiles
                currentIndex: -1
                clip: true

                // A reset (a profile added, removed or renamed) clears a tree view's selection.
                Connections {
                    target: root.profiles

                    function onModelReset() {
                        profileList.currentIndex = -1;
                    }
                }

                header: RowLayout {
                    width: ListView.view.width

                    QQC2.Label {
                        Layout.fillWidth: true
                        leftPadding: Kirigami.Units.smallSpacing
                        text: i18nc("@title:column Profile name", "Name")
                        font.bold: true
                    }
                    QQC2.Label {
                        rightPadding: Kirigami.Units.smallSpacing
                        text: i18nc("@title:column Profile keyboard shortcut", "Shortcut")
                        font.bold: true
                    }
                }

                delegate: QQC2.ItemDelegate {
                    id: row

                    required property int index
                    required property string name
                    required property string iconName
                    required property string profileToolTip
                    required property bool bold
                    required property var shortcut
                    required property bool editable
                    required property bool deletable
                    required property bool isDefault

                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem
                    onClicked: profileList.currentIndex = index
                    onDoubleClicked: root.profiles.editProfile(index)
                    QQC2.ToolTip.text: profileToolTip
                    QQC2.ToolTip.visible: hovered && profileToolTip.length > 0
                    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval

                    contentItem: RowLayout {
                        Kirigami.Icon {
                            source: row.iconName
                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                            Layout.preferredHeight: Kirigami.Units.iconSizes.small
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: row.name
                            font.bold: row.bold
                            elide: Text.ElideRight
                        }
                        KQuickControls.KeySequenceItem {
                            objectName: "shortcut"
                            keySequence: row.shortcut
                            checkForConflictsAgainst: KQuickControls.ShortcutType.None
                            onKeySequenceModified: root.profiles.setShortcut(row.index, keySequence)
                        }
                    }
                }
            }
        }

        Kirigami.Separator {
            Layout.fillHeight: true
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignTop

            PageButton {
                objectName: "newProfileButton"
                text: i18n("&New...")
                icon.name: "list-add"
                onClicked: root.profiles.createProfile(root.currentRow)
                QQC2.ToolTip.text: i18n("Create a new profile based on the selected profile")
            }
            PageButton {
                objectName: "editProfileButton"
                text: i18n("&Edit...")
                icon.name: "document-edit"
                enabled: root.current !== null && root.current.editable
                onClicked: root.profiles.editProfile(root.currentRow)
                QQC2.ToolTip.text: i18n("Edit the selected profile (this button is disabled for read-only profiles)")
            }
            PageButton {
                objectName: "deleteProfileButton"
                text: i18n("&Remove")
                icon.name: "edit-delete"
                enabled: root.current !== null && root.current.deletable && !root.current.isDefault
                onClicked: root.profiles.deleteProfile(root.currentRow)
                QQC2.ToolTip.text: i18n("Delete the selected profile")
            }
            PageButton {
                objectName: "setAsDefaultButton"
                text: i18n("&Set as Default")
                icon.name: "dialog-ok-apply"
                enabled: root.current !== null && !root.current.isDefault
                onClicked: root.profiles.setDefault(root.currentRow)
                QQC2.ToolTip.text: i18n("Set the selected profile as the default for new terminal sessions")
            }
        }
    }
}
