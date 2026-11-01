/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// The Configure dialog's Temporary Files page (TemporaryFilesSettings.ui): three Bool
// settings shown as one choice, and the custom directory (a KUrlRequester).
DialogPage {
    id: root

    required property SettingsEditor settings
    readonly property var locations: ["scrollbackUseSystemLocation", "scrollbackUseCacheLocation", "scrollbackUseSpecifiedLocation"]

    function sync() {
        for (const button of locationGroup.buttons) {
            button.checked = settings.value(button.setting);
        }
        // Not under the user's typing (the stored URL shows the path a little differently).
        if (!directoryEdit.activeFocus) {
            directoryEdit.text = settings.path(directoryEdit.setting);
        }
    }

    title: i18nc("@title Preferences page name", "Temporary Files")
    iconName: "folder-temp"
    Component.onCompleted: sync()

    Connections {
        target: root.settings

        function onChanged() {
            root.sync();
        }
    }

    QQC2.ButtonGroup {
        id: locationGroup

        onClicked: button => {
            for (const setting of root.locations) {
                root.settings.setValue(setting, setting === button.setting);
            }
        }
    }

    component Location: QQC2.RadioButton {
        required property string setting

        objectName: "kcfg_" + setting
        QQC2.ButtonGroup.group: locationGroup
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 3

        QQC2.Label {
            Layout.alignment: Qt.AlignRight
            text: i18n("Scrollback file location:")
        }
        Location {
            Layout.columnSpan: 2
            setting: "scrollbackUseSystemLocation"
            text: i18nc("@option:radio File location; <filename>%1</filename>: path to directory placeholder", "System temporary directory (%1)", DialogHelpers.temporaryDirectory())
        }
        Item {}
        Location {
            Layout.columnSpan: 2
            setting: "scrollbackUseCacheLocation"
            text: i18nc("@option:radio File location; <filename>%1</filename>: path to directory placeholder", "User cache directory (%1)", DialogHelpers.cacheDirectory())
        }
        Item {}
        Location {
            id: customLocation

            Layout.columnSpan: 2
            setting: "scrollbackUseSpecifiedLocation"
            text: i18nc("@option:radio Custom (file location); followed by text entry field", "Custom:")
        }
        Item {}
        Item {
            implicitWidth: Kirigami.Units.gridUnit
        }
        RowLayout {
            Layout.fillWidth: true
            enabled: customLocation.checked
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: directoryEdit

                readonly property string setting: "scrollbackUseSpecifiedLocationDirectory"

                objectName: "kcfg_" + setting
                Layout.fillWidth: true
                onTextEdited: root.settings.setValue(setting, text)
            }
            QQC2.Button {
                icon.name: "document-open"
                onClicked: {
                    const path = DialogHelpers.chooseDirectory(directoryEdit.text, this);
                    if (path.length > 0) {
                        directoryEdit.text = path;
                        root.settings.setValue(directoryEdit.setting, path);
                    }
                }
                QQC2.ToolTip.text: i18nd("kio6", "Open file dialog")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
