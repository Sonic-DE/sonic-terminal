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
            // QTreeView's size hint: the list scrolls rather than grow the dialog.
            Layout.preferredWidth: 256
            Layout.preferredHeight: 192
            Kirigami.StyleHints.showFramedBackground: true

            ListView {
                id: profileList

                // The Shortcut column's width: its title's or its widest shortcut's
                // (ResizeToContents); the row being given a new shortcut, -1 when none.
                property real shortcutWidth: shortcutTitleMetrics.advanceWidth + 2 * Kirigami.Units.smallSpacing
                property int editingRow: -1

                TextMetrics {
                    id: shortcutTitleMetrics

                    text: i18nc("@title:column Profile keyboard shortcut", "Shortcut")
                }

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

                // QHeaderView: plain titles, a line between the columns and under them.
                header: ColumnLayout {
                    width: ListView.view.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        QQC2.Label {
                            Layout.fillWidth: true
                            padding: Kirigami.Units.smallSpacing
                            text: i18nc("@title:column Profile name", "Name")
                        }
                        Kirigami.Separator {
                            Layout.fillHeight: true
                        }
                        QQC2.Label {
                            Layout.preferredWidth: profileList.shortcutWidth
                            padding: Kirigami.Units.smallSpacing
                            text: i18nc("@title:column Profile keyboard shortcut", "Shortcut")
                        }
                    }
                    Kirigami.Separator {
                        Layout.fillWidth: true
                    }
                }

                delegate: QQC2.ItemDelegate {
                    id: row

                    required property int index
                    required property string name
                    required property string iconName
                    required property string profileToolTip
                    required property bool bold
                    required property string shortcutText
                    required property bool editable
                    required property bool deletable
                    required property bool isDefault

                    readonly property bool editingShortcut: profileList.editingRow === index

                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem
                    onClicked: profileList.currentIndex = index
                    // On the Shortcut column the shortcut is edited (ShortcutItemDelegate's
                    // editor); elsewhere the profile (ProfileSettings::doubleClicked()).
                    onDoubleClicked: {
                        if (shortcutCell.contains(shortcutCell.mapFromItem(row, pressX, pressY))) {
                            profileList.editingRow = index;
                        } else {
                            root.profiles.editProfile(index);
                        }
                    }
                    QQC2.ToolTip.text: profileToolTip
                    QQC2.ToolTip.visible: hovered && profileToolTip.length > 0
                    QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval

                    // alternatingRowColors
                    background: Rectangle {
                        color: row.highlighted ? Kirigami.Theme.highlightColor
                            : row.index % 2 === 1 ? Kirigami.Theme.alternateBackgroundColor : "transparent"
                    }

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
                            color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                            elide: Text.ElideRight
                        }
                        Item {
                            id: shortcutCell

                            Layout.preferredWidth: profileList.shortcutWidth
                            Layout.fillHeight: true

                            QQC2.Label {
                                id: shortcutText

                                anchors.fill: parent
                                leftPadding: Kirigami.Units.smallSpacing
                                rightPadding: Kirigami.Units.smallSpacing
                                verticalAlignment: Text.AlignVCenter
                                visible: !row.editingShortcut
                                color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                text: row.shortcutText
                                onImplicitWidthChanged: profileList.shortcutWidth = Math.max(profileList.shortcutWidth, implicitWidth)
                            }
                            // ShortcutItemDelegate's editor, a FilteredKeySequenceEdit: chords
                            // with modifiers are recorded (QKeySequenceEdit: up to four, done
                            // a second after the last); Return or Enter keeps what's there,
                            // Backspace or Delete clears it, other keys without modifiers do
                            // nothing; Esc closes it unchanged and losing the focus commits it
                            // (the view's handling of an editor).
                            Loader {
                                anchors.fill: parent
                                active: row.editingShortcut
                                onLoaded: item.forceActiveFocus()
                                sourceComponent: QQC2.TextField {
                                    id: recorder

                                    property var chords: []
                                    property bool recorded: false
                                    property bool done: false

                                    function finish(store) {
                                        if (done) {
                                            return;
                                        }
                                        done = true;
                                        chordTimer.stop();
                                        if (store && recorded) {
                                            root.profiles.setShortcutChords(row.index, chords);
                                        }
                                        profileList.editingRow = -1;
                                    }

                                    objectName: "shortcut"
                                    readOnly: true
                                    text: recorded ? root.profiles.chordsText(chords) : row.shortcutText

                                    Timer {
                                        id: chordTimer

                                        interval: 1000
                                        onTriggered: recorder.finish(true)
                                    }

                                    Keys.onPressed: event => {
                                        event.accepted = true;
                                        const modifiers = event.modifiers & (Qt.ShiftModifier | Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier);
                                        if (event.key === Qt.Key_Escape && modifiers === 0) {
                                            finish(false);
                                        } else if (modifiers === 0) {
                                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                                finish(true);
                                            } else if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) {
                                                chords = [];
                                                recorded = true;
                                                finish(true);
                                            }
                                        } else if (![Qt.Key_Shift, Qt.Key_Control, Qt.Key_Alt, Qt.Key_Meta, Qt.Key_AltGr].includes(event.key)) {
                                            chords = (recorded ? chords : []).concat([event.key | modifiers]);
                                            recorded = true;
                                            if (chords.length === 4) {
                                                finish(true);
                                            } else {
                                                chordTimer.restart();
                                            }
                                        }
                                    }
                                    onActiveFocusChanged: {
                                        if (!activeFocus) {
                                            finish(true);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Kirigami.Separator {
            Layout.fillHeight: true
        }

        // As wide as its widest button: the expanding view takes the rest (a layout fills
        // by default).
        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: false

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
