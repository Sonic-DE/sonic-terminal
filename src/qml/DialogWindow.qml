/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// A QDialog as a window of its own (opened by QmlDialogs::open()): its content above the
// button box. OK, Return and accept() emit accepted(); Cancel, Esc, closing and reject()
// emit rejected(); either closes it.
Window {
    id: root

    default property alias content: contentColumn.data
    property alias standardButtons: buttonBox.standardButtons
    property alias buttonBox: buttonBox
    // False keeps the dialog open on OK (an overridden QDialog::accept() refusing).
    property bool acceptable: true

    signal accepted
    signal rejected
    signal applied
    signal defaultsRequested

    property bool done: false

    function accept() {
        if (!acceptable || done) {
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
        Keys.onReturnPressed: root.accept()
        Keys.onEnterPressed: root.accept()

        ColumnLayout {
            id: contentColumn

            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        QQC2.DialogButtonBox {
            id: buttonBox

            Layout.fillWidth: true
            padding: 0
            visible: standardButtons !== QQC2.DialogButtonBox.NoButton
            onAccepted: root.accept()
            onRejected: root.reject()
            onApplied: root.applied()
            onReset: root.defaultsRequested()
            // OK is the default button (QPushButton::setDefault), drawn as such.
            Component.onCompleted: {
                const ok = standardButton(QQC2.DialogButtonBox.Ok);
                if (ok) {
                    ok.Accessible.defaultButton = true;
                }
            }
        }
    }
}
