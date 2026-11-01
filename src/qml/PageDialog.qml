/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// A KPageDialog with the List face: the pages (DialogPage children) listed with their
// icons on the left under a search field, the current page with its title on the right.
// The search works as KPageView's: 400 ms after typing, pages whose name and texts don't
// match are hidden, the first page left is shown, matching controls are outlined.
DialogWindow {
    id: root

    default property alias pages: stack.data
    property alias currentIndex: stack.currentIndex
    readonly property Item currentPage: stack.children[stack.currentIndex] ?? null
    property var hiddenPages: []
    property var outlines: []

    function showPage(page) {
        const index = Array.prototype.indexOf.call(stack.children, page);
        if (index >= 0) {
            stack.currentIndex = index;
        }
    }

    // The texts a user sees on a control, as the labels, buttons and combo boxes KPageView
    // searches.
    function matchingControls(item, text) {
        let found = [];
        for (let i = 0; i < item.children.length; ++i) {
            const child = item.children[i];
            const texts = [];
            if (typeof child.text === "string") {
                texts.push(child.text);
            }
            if (child instanceof QQC2.ComboBox) {
                for (let row = 0; row < child.count; ++row) {
                    texts.push(child.textAt(row));
                }
            }
            if (texts.some(t => t.replace(/&(?!&)/g, "").toLowerCase().includes(text))) {
                found.push(child);
            }
            found = found.concat(matchingControls(child, text));
        }
        return found;
    }

    function search(text) {
        for (const outline of outlines) {
            outline.destroy();
        }
        const hidden = [];
        const outlined = [];
        const lower = text.toLowerCase();
        if (lower.length > 0) {
            for (let i = 0; i < stack.children.length; ++i) {
                const page = stack.children[i];
                const matches = matchingControls(page, lower);
                if (matches.length === 0 && !page.title.toLowerCase().includes(lower)) {
                    hidden.push(page);
                }
                for (const control of matches) {
                    outlined.push(outlineComponent.createObject(control));
                }
            }
        }
        hiddenPages = hidden;
        outlines = outlined;
        if (currentPage && hidden.includes(currentPage)) {
            for (let i = 0; i < stack.children.length; ++i) {
                if (!hidden.includes(stack.children[i])) {
                    stack.currentIndex = i;
                    break;
                }
            }
        }
    }

    Component {
        id: outlineComponent

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: Kirigami.Theme.highlightColor
            border.width: 1
            radius: Kirigami.Units.cornerRadius
        }
    }

    Timer {
        id: searchTimer

        interval: 400
        onTriggered: root.search(searchField.text)
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 0

        ColumnLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: Kirigami.Units.gridUnit * 8
            spacing: Kirigami.Units.smallSpacing

            Kirigami.SearchField {
                id: searchField

                objectName: "pageSearch"
                Layout.fillWidth: true
                focus: true
                placeholderText: i18nc("@info:placeholder", "Search…")
                onTextChanged: searchTimer.restart()
            }

            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: pageList

                    objectName: "pageList"
                    model: stack.children
                    currentIndex: stack.currentIndex
                    clip: true

                    delegate: QQC2.ItemDelegate {
                        required property Item modelData
                        required property int index

                        width: ListView.view.width
                        visible: !root.hiddenPages.includes(modelData)
                        height: visible ? implicitHeight : 0
                        display: QQC2.AbstractButton.TextUnderIcon
                        text: modelData.title
                        icon.name: modelData.iconName
                        icon.width: Kirigami.Units.iconSizes.medium
                        icon.height: Kirigami.Units.iconSizes.medium
                        highlighted: ListView.isCurrentItem
                        onClicked: stack.currentIndex = index
                    }
                }
            }
        }

        Kirigami.Separator {
            Layout.fillHeight: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Heading {
                objectName: "pageTitle"
                level: 2
                text: root.currentPage ? root.currentPage.title : ""
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            StackLayout {
                id: stack

                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
