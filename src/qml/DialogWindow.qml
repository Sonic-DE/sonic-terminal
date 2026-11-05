/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.kde.kirigami as Kirigami

// A QDialog as a window of its own (opened by QmlDialogs::open()): its content above the
// button box. OK, Return and accept() emit accepted(); Cancel, Esc, closing and reject()
// emit rejected(); either closes it.
Window {
    id: root

    default property alias content: contentColumn.data
    // QDialogButtonBox's ActionRole buttons: right-aligned, before OK and Cancel.
    property alias actionButtons: actionRow.data
    property int standardButtons: QQC2.DialogButtonBox.NoButton
    // Asked on OK; false keeps the dialog open (an overridden QDialog::accept() refusing).
    property var acceptCheck: () => true
    // The button Return presses (QPushButton::setDefault): OK, or Cancel.
    property int defaultButton: QQC2.DialogButtonBox.Ok

    signal accepted
    signal rejected
    signal applied
    signal defaultsRequested
    signal helpRequested

    property bool done: false
    // QDialogButtonBox's KDE layout puts the Help and Reset roles on the left, the others on the right.
    readonly property int leftButtons: QQC2.DialogButtonBox.Help | QQC2.DialogButtonBox.Reset | QQC2.DialogButtonBox.RestoreDefaults

    function standardButton(button) {
        return (button & leftButtons) ? leftBox.standardButton(button) : buttonBox.standardButton(button);
    }

    function accept() {
        if (done || !acceptCheck()) {
            return;
        }
        done = true;
        accepted();
        close();
    }

    function reject() {
        if (done) {
            return;
        }
        done = true;
        rejected();
        close();
    }

    flags: Qt.Dialog
    color: Kirigami.Theme.backgroundColor
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false
    width: Math.max(layout.implicitWidth, 200) + 2 * Kirigami.Units.largeSpacing
    height: layout.implicitHeight + 2 * Kirigami.Units.largeSpacing
    minimumWidth: layout.Layout.minimumWidth + 2 * Kirigami.Units.largeSpacing
    minimumHeight: layout.Layout.minimumHeight + 2 * Kirigami.Units.largeSpacing

    onClosing: reject()

    ColumnLayout {
        id: layout

        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing
        // Keys no control takes come here, as a QDialog gets those its widgets ignore.
        focus: true

        Keys.onEscapePressed: root.reject()
        Keys.onReturnPressed: root.defaultButton === QQC2.DialogButtonBox.Cancel ? root.reject() : root.accept()
        Keys.onEnterPressed: root.defaultButton === QQC2.DialogButtonBox.Cancel ? root.reject() : root.accept()

        ColumnLayout {
            id: contentColumn

            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.mediumSpacing
            visible: root.standardButtons !== QQC2.DialogButtonBox.NoButton

            QQC2.DialogButtonBox {
                id: leftBox

                padding: 0
                alignment: Qt.AlignLeft
                standardButtons: root.standardButtons & root.leftButtons
                visible: standardButtons !== QQC2.DialogButtonBox.NoButton
                onHelpRequested: root.helpRequested()
                onReset: root.defaultsRequested()
            }

            Item {
                Layout.fillWidth: true
            }

            RowLayout {
                id: actionRow

                spacing: Kirigami.Units.mediumSpacing
                visible: children.length > 0
            }

            QQC2.DialogButtonBox {
                id: buttonBox

                padding: 0
                alignment: Qt.AlignRight
                standardButtons: root.standardButtons & ~root.leftButtons
                onAccepted: root.accept()
                onRejected: root.reject()
                onApplied: root.applied()
                // The default button is drawn as such.
                Component.onCompleted: {
                    const button = standardButton(root.defaultButton);
                    if (button) {
                        button.Accessible.defaultButton = true;
                    }
                }
            }
        }
    }
}
