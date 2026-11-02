/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// The Configure dialog's Memory page (MemorySettings.ui; Linux only).
DialogPage {
    id: root

    required property SettingsEditor settings

    title: i18nc("@title Preferences page name", "Memory")
    iconName: "memory"

    // QFormLayout: the label column is as wide as the check box; a third column takes the
    // spare width so the spanning note wraps at the page's width.
    GridLayout {
        Layout.fillWidth: true
        columns: 3

        SettingCheckBox {
            settings: root.settings
            setting: "EnableMemoryMonitoring"
            text: i18n("Enable memory monitoring:")
        }
        Item {
            Layout.column: 2
            Layout.fillWidth: true
        }
        QQC2.Label {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            Layout.preferredWidth: Kirigami.Units.gridUnit * 20
            wrapMode: Text.WordWrap
            text: i18n("Changes will take effect upon restart. If enabled, sustained memory consumption by Konsole exceeding the specified value will cause systemd-oomd to eliminate the tab(s) containing the memory-intensive process(es). Note: enabling might cause greater slowdowns during high memory consumption and specifying excessively low values might still cause Konsole to be killed.")
        }
        QQC2.Label {
            Layout.alignment: Qt.AlignRight
            text: i18n("Memory limit:")
        }
        SettingSpinBox {
            settings: root.settings
            setting: "MemoryLimitValue"
            from: 1
            to: 999999
            suffix: i18n(" MB")
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
