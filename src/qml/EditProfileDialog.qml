/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.sonicde.terminal

// The Edit Profile dialog (EditProfileDialog): its pages edit the editor's profile; Apply
// and OK store the edits once the name is valid, Cancel undoes the previews.
PageDialog {
    id: root

    required property ProfileEditor editor

    // EditProfileDialog::selectProfileName().
    function selectProfileName() {
        showPage(generalPage);
        generalPage.selectProfileName();
    }

    title: editor.title
    standardButtons: QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Cancel | QQC2.DialogButtonBox.Apply
    acceptCheck: () => editor.accept()

    onRejected: editor.reject()
    onApplied: editor.apply()

    Component.onCompleted: buttonBox.standardButton(QQC2.DialogButtonBox.Apply).enabled = Qt.binding(() => editor.applyEnabled)

    Connections {
        target: root.editor

        function onMessageChanged() {
            root.showPage(generalPage);
        }
    }

    EditProfileGeneralPage {
        id: generalPage

        editor: root.editor
    }
    EditProfileTabsPage {
        editor: root.editor
    }
    EditProfileScrollingPage {
        editor: root.editor
    }
    EditProfileMousePage {
        editor: root.editor
    }
    EditProfileAdvancedPage {
        editor: root.editor
    }
}
