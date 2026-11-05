/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.desktop as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrols as KQuickControls
import org.sonicde.terminal

// Configure Keyboard Shortcuts (KShortcutsDialog with Konsole's options): the search line,
// the tree of actions with their Shortcut and Alternate columns, a row's editor below it,
// the Shortcut Schemes group behind "Manage Schemes", Print, Defaults, OK and Cancel.
DialogWindow {
    id: root

    required property ShortcutsEditorModel editor
    // The row whose editor is open and its column (1 Shortcut, 2 Alternate); -1 when none.
    property int editingRow: -1
    property int editingColumn: 1
    // QTreeView's indentation.
    readonly property int indentation: 20
    readonly property int cellPadding: Kirigami.Units.smallSpacing
    // Bumped when texts change, for the column widths.
    property int textsRevision: 0

    // KShortcutsEditorDelegate::itemActivated(): one editor at a time; the same cell closes it.
    function toggleEditor(row, column) {
        if (editingRow === row && editingColumn === column) {
            editingRow = -1;
            return;
        }
        editingColumn = column;
        editingRow = row;
    }

    // QHeaderView::ResizeToContents for the Action and Shortcut columns.
    function columnWidth(column) {
        textsRevision;
        const texts = editor.columnTexts(column);
        const depths = editor.depths();
        let width = metrics.advanceWidth(column === 0 ? i18ndc("kxmlgui6", "@title:column", "Action") : i18ndc("kxmlgui6", "@title:column", "Shortcut"));
        for (let row = 0; row < texts.length; ++row) {
            let cell = metrics.advanceWidth(texts[row]);
            if (column === 0) {
                cell += (depths[row] + 1) * indentation + Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing;
            } else {
                cell += Kirigami.Units.iconSizes.small;
            }
            width = Math.max(width, cell);
        }
        return Math.ceil(width) + 2 * cellPadding + 1;
    }

    title: i18ndc("kxmlgui6", "@title:window", "Configure Keyboard Shortcuts")
    standardButtons: QQC2.DialogButtonBox.RestoreDefaults | QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Cancel
    width: editor.dialogSize().width
    height: editor.dialogSize().height

    // KGuiItem::assign(..., KStandardGuiItem::defaults())
    Component.onCompleted: {
        const defaults = standardButton(QQC2.DialogButtonBox.RestoreDefaults);
        defaults.text = qsTranslate("KStandardGuiItem", "&Defaults");
        defaults.icon.name = "document-revert";
    }
    onAccepted: editor.save()
    onRejected: editor.undo()
    onDefaultsRequested: editor.allDefault()
    // KShortcutsDialog's destructor.
    onVisibleChanged: {
        if (!visible) {
            editor.saveDialogSize(Qt.size(width, height));
        }
    }

    actionButtons: [
        QQC2.Button {
            objectName: "manageSchemesButton"
            text: i18nd("kxmlgui6", "Manage &Schemes") + (schemesBox.visible ? " <<" : " >>")
            onClicked: schemesBox.visible = !schemesBox.visible
        },
        QQC2.Button {
            objectName: "printButton"
            text: qsTranslate("KStandardGuiItem", "&Print…")
            icon.name: "document-print"
            onClicked: root.editor.print(root.contentItem)
        }
    ]

    FontMetrics {
        id: metrics
    }

    Connections {
        target: root.editor
        function onDataChanged() {
            root.textsRevision++;
        }
        function onModelReset() {
            root.editingRow = -1;
            root.textsRevision++;
        }
    }

    // KTreeWidgetSearchLineWidget
    ClearableTextField {
        objectName: "KTreeWidgetSearchLine"
        Layout.fillWidth: true
        placeholderText: qsTranslate("KTreeWidgetSearchLine", "Search…", "@info:placeholder")
        focus: true
        QQC2.ToolTip.text: i18nd("kxmlgui6", "Search interactively for shortcut names (e.g. Copy) or combination of keys (e.g. Ctrl+C) by typing them here.")
        QQC2.ToolTip.visible: hovered && QQC2.ToolTip.text.length > 0
        onTextChanged: root.editor.filterText = text
    }

    QQC2.Frame {
        Layout.fillWidth: true
        Layout.fillHeight: true
        padding: 0
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false

        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2)
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 1
            spacing: 0

            // The header: Action (sorted ascending), Shortcut, Alternate stretched.
            Row {
                id: header

                readonly property real actionWidth: root.columnWidth(0)
                readonly property real shortcutWidth: root.columnWidth(1)

                Layout.fillWidth: true

                Repeater {
                    model: [i18ndc("kxmlgui6", "@title:column", "Action"), i18ndc("kxmlgui6", "@title:column", "Shortcut"), i18ndc("kxmlgui6", "@title:column", "Alternate")]
                    delegate: Rectangle {
                        required property int index
                        required property string modelData

                        width: index === 0 ? header.actionWidth : index === 1 ? header.shortcutWidth : Math.max(0, header.width - header.actionWidth - header.shortcutWidth)
                        height: headerLabel.implicitHeight + 2 * root.cellPadding
                        color: Kirigami.Theme.alternateBackgroundColor

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: root.cellPadding
                            anchors.rightMargin: root.cellPadding

                            QQC2.Label {
                                id: headerLabel

                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: modelData
                            }
                            Kirigami.Icon {
                                visible: index === 0
                                Layout.preferredWidth: Kirigami.Units.iconSizes.small / 2
                                Layout.preferredHeight: Kirigami.Units.iconSizes.small / 2
                                source: "arrow-down"
                            }
                        }
                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2)
                        }
                    }
                }
            }
            Kirigami.Separator {
                Layout.fillWidth: true
            }

            ListView {
                id: list

                // A click on a row's cell (the parity hook's clickItem): 0 Action, 1 Shortcut, 2 Alternate.
                function clickItem(row, column) {
                    if (root.editor.data(root.editor.index(row, 0), Qt.DisplayRole) === undefined) {
                        return;
                    }
                    positionViewAtIndex(row, ListView.Contain);
                    root.toggleEditor(row, column === 2 ? ShortcutsEditorModel.Alternate : ShortcutsEditorModel.Primary);
                }

                objectName: "list"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.editor
                QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

                delegate: Column {
                    id: entry

                    required property int index
                    required property int kind
                    required property int depth
                    required property string name
                    required property string iconName
                    required property string primary
                    required property string alternate
                    required property string defaultPrimary
                    required property string defaultAlternate
                    required property var primarySequence
                    required property var alternateSequence
                    required property var defaultPrimarySequence
                    required property var defaultAlternateSequence
                    required property bool rowVisible
                    required property bool hasChildren
                    required property bool collapsed
                    readonly property bool isAction: kind === ShortcutsEditorModel.ActionItem
                    readonly property bool editing: root.editingRow === index

                    width: ListView.view.width
                    visible: rowVisible
                    height: rowVisible ? implicitHeight : 0

                    Rectangle {
                        width: entry.width
                        // KShortcutsEditorDelegate::sizeHint(): 4 px over the item's.
                        height: Math.max(nameLabel.implicitHeight, Kirigami.Units.iconSizes.small) + 2 * root.cellPadding + 4
                        color: entry.index % 2 ? Kirigami.Theme.alternateBackgroundColor : Kirigami.Theme.backgroundColor

                        Row {
                            anchors.fill: parent

                            // Action: indentation, the branch arrow, icon and name.
                            Item {
                                width: header.actionWidth
                                height: parent.height

                                Kirigami.Icon {
                                    x: entry.depth * root.indentation + (root.indentation - width) / 2
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: Kirigami.Units.iconSizes.small / 2
                                    height: width
                                    visible: entry.hasChildren
                                    source: entry.collapsed ? "arrow-right" : "arrow-down"
                                }
                                RowLayout {
                                    x: (entry.depth + 1) * root.indentation
                                    width: parent.width - x - root.cellPadding
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: Kirigami.Units.smallSpacing

                                    Kirigami.Icon {
                                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                        visible: entry.isAction
                                        source: entry.iconName
                                    }
                                    QQC2.Label {
                                        id: nameLabel

                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        font.bold: entry.editing
                                        text: entry.name
                                    }
                                }
                                TapHandler {
                                    onTapped: entry.isAction ? root.toggleEditor(entry.index, ShortcutsEditorModel.Primary) : root.editor.toggleCollapsed(entry.index)
                                }
                            }

                            // Shortcut and Alternate: the extend arrow and the sequence.
                            Repeater {
                                model: entry.isAction ? 2 : 0
                                delegate: Rectangle {
                                    id: shortcutCell

                                    required property int index
                                    readonly property int column: index + 1
                                    readonly property bool current: entry.editing && root.editingColumn === column

                                    width: column === 1 ? header.shortcutWidth : Math.max(0, entry.width - header.actionWidth - header.shortcutWidth)
                                    height: parent.height
                                    color: current ? Kirigami.Theme.highlightColor : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: root.cellPadding
                                        anchors.rightMargin: root.cellPadding
                                        spacing: 0

                                        Kirigami.Icon {
                                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                            Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                            source: shortcutCell.current ? "arrow-down" : "arrow-right"
                                            color: shortcutCell.current ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                        }
                                        QQC2.Label {
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                            color: shortcutCell.current ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                            text: shortcutCell.column === 1 ? entry.primary : entry.alternate
                                        }
                                    }
                                    TapHandler {
                                        onTapped: root.toggleEditor(entry.index, shortcutCell.column)
                                    }
                                }
                            }
                        }
                    }

                    // ShortcutEditWidget, extended under the row.
                    Loader {
                        active: entry.editing
                        width: entry.width
                        sourceComponent: Rectangle {
                            id: editorArea

                            readonly property var current: root.editingColumn === ShortcutsEditorModel.Primary ? entry.primarySequence : entry.alternateSequence
                            readonly property var defaultSequence: root.editingColumn === ShortcutsEditorModel.Primary ? entry.defaultPrimarySequence : entry.defaultAlternateSequence
                            readonly property string defaultText: root.editingColumn === ShortcutsEditorModel.Primary ? entry.defaultPrimary : entry.defaultAlternate
                            readonly property string currentText: root.editingColumn === ShortcutsEditorModel.Primary ? entry.primary : entry.alternate
                            // ShortcutEditWidget::setKeySequence(): the default's text selects "Default:".
                            readonly property bool isDefault: currentText === defaultText

                            width: entry.width
                            implicitHeight: editorGrid.implicitHeight + 2 * Kirigami.Units.smallSpacing
                            color: Kirigami.Theme.backgroundColor

                            Rectangle {
                                width: 3
                                height: parent.height
                                color: Kirigami.Theme.highlightColor
                            }

                            GridLayout {
                                id: editorGrid

                                x: root.indentation + Kirigami.Units.smallSpacing
                                y: Kirigami.Units.smallSpacing
                                columns: 3

                                QQC2.RadioButton {
                                    objectName: "defaultRadio"
                                    text: i18ndc("kxmlgui6", "@option:radio", "Default:")
                                    checked: editorArea.isDefault
                                    onToggled: {
                                        if (checked && !root.editor.setShortcut(entry.index, root.editingColumn, editorArea.defaultSequence, root.contentItem)) {
                                            checked = Qt.binding(() => editorArea.isDefault);
                                        }
                                    }
                                }
                                QQC2.Label {
                                    text: editorArea.defaultText.length > 0 ? editorArea.defaultText : i18ndc("kxmlgui6", "No shortcut defined", "None")
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                                QQC2.RadioButton {
                                    objectName: "customRadio"
                                    text: i18ndc("kxmlgui6", "@option:radio", "Custom:")
                                    checked: !editorArea.isDefault
                                }
                                KQuickControls.KeySequenceItem {
                                    objectName: "KKeySequenceWidget"
                                    // Local shortcuts: a key with modifiers (letters refused), several keys.
                                    patterns: KQuickControls.ShortcutPattern.ModifierAndKey
                                    multiKeyShortcutsAllowed: true
                                    // The model runs KKeySequenceWidget's checks.
                                    checkForConflictsAgainst: KQuickControls.ShortcutType.None
                                    keySequence: editorArea.isDefault ? "" : editorArea.current
                                    onKeySequenceModified: {
                                        if (!root.editor.setShortcut(entry.index, root.editingColumn, keySequence, root.contentItem)) {
                                            keySequence = Qt.binding(() => editorArea.isDefault ? "" : editorArea.current);
                                        }
                                    }
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // KShortcutSchemesEditor
    QQC2.GroupBox {
        id: schemesBox

        objectName: "schemesBox"
        Layout.fillWidth: true
        visible: false
        title: i18ndc("kxmlgui6", "@title:group", "Shortcut Schemes")

        RowLayout {
            anchors.fill: parent

            QQC2.Label {
                text: i18nd("kxmlgui6", "Current scheme:")
            }
            QQC2.ComboBox {
                objectName: "schemesList"
                model: root.editor.schemes
                currentIndex: root.editor.schemes.indexOf(root.editor.currentScheme)
                onActivated: index => root.editor.changeScheme(root.editor.schemes[index], root.contentItem)
            }
            QQC2.Button {
                objectName: "newScheme"
                icon.name: "document-new"
                text: i18ndc("kxmlgui6", "@action:button", "New…")
                onClicked: root.editor.newScheme(root.contentItem)
            }
            QQC2.Button {
                objectName: "deleteScheme"
                icon.name: "edit-delete"
                text: i18ndc("kxmlgui6", "@action:button", "Delete")
                enabled: root.editor.schemes.length >= 1
                onClicked: root.editor.deleteScheme(root.contentItem)
            }
            QQC2.Button {
                id: moreActions

                objectName: "moreActions"
                icon.name: "view-more-symbolic"
                text: i18ndc("kxmlgui6", "@action:button", "More Actions")
                onClicked: moreMenu.popup(moreActions, 0, moreActions.height)

                QQC2.Menu {
                    id: moreMenu

                    QQC2.MenuItem {
                        icon.name: "document-save"
                        text: i18ndc("kxmlgui6", "@action:inmenu", "Save shortcuts to scheme")
                        onTriggered: root.editor.saveToScheme(root.contentItem)
                    }
                    QQC2.MenuItem {
                        icon.name: "document-export"
                        text: i18ndc("kxmlgui6", "@action:inmenu", "Export Scheme…")
                        onTriggered: root.editor.exportScheme(root.contentItem)
                    }
                    QQC2.MenuItem {
                        icon.name: "document-import"
                        text: i18ndc("kxmlgui6", "@action:inmenu", "Import Scheme…")
                        onTriggered: root.editor.importScheme(root.contentItem)
                    }
                }
            }
            Item {
                Layout.fillWidth: true
            }
        }
    }
}
