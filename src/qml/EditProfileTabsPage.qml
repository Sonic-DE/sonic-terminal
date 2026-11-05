/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.sonicde.terminal

// The Edit Profile dialog's Tabs page (EditProfileTabsPage.ui): tab title formats and
// colours, and the silence threshold.
DialogPage {
    id: root

    required property ProfileEditor editor

    function edit(name, value) {
        if (value !== editor.value(name)) {
            editor.setValue(name, value);
        }
    }

    title: i18n("Tabs")
    iconName: "preferences-tabs"

    QQC2.GroupBox {
        Layout.fillWidth: true
        title: i18n("Tab Titles")

        TabTitleFields {
            id: renameTabWidget

            objectName: "renameTabWidget"
            anchors.fill: parent
            tabTitle: root.editor.value("LocalTabTitleFormat")
            remoteTabTitle: root.editor.value("RemoteTabTitleFormat")
            Component.onCompleted: {
                setColor(root.editor.value("TabColor"));
                setActivityColor(root.editor.value("TabActivityColor"));
            }
            onTabTitleFormatEdited: text => root.edit("LocalTabTitleFormat", text)
            onRemoteTabTitleFormatEdited: text => root.edit("RemoteTabTitleFormat", text)
            onTabColorChanged: color => root.editor.setValue("TabColor", color)
            onTabActivityColorChanged: color => root.editor.setValue("TabActivityColor", color)
        }
    }

    QQC2.GroupBox {
        Layout.fillWidth: true
        title: i18n("Tab Monitoring")

        RowLayout {
            anchors.fill: parent

            MnemonicLabel {
                label: i18n("Threshold for continuous silence:")
                buddy: silenceSecondsSpinner
            }
            QQC2.SpinBox {
                id: silenceSecondsSpinner

                // KPluralHandlingSpinBox's suffix, for the current value.
                readonly property string suffix: i18ncp("Unit of time", " second", " seconds", value)

                objectName: "silenceSecondsSpinner"
                editable: true
                from: 1
                to: 3600
                textFromValue: (value, locale) => Number(value).toLocaleString(locale, "f", 0) + i18ncp("Unit of time", " second", " seconds", value)
                valueFromText: (text, locale) => Number.fromLocaleString(locale, text.replace(/[^\d.,\s-]/g, "").trim())
                Component.onCompleted: value = root.editor.value("SilenceSeconds")
                onValueModified: root.editor.setValue("SilenceSeconds", value)
                QQC2.ToolTip.text: i18n("The threshold for continuous silence to be detected by Konsole")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            }
            Item {
                Layout.fillWidth: true
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
