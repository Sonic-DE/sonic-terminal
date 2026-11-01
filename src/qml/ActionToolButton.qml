/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.sonicde.terminal

// A KXmlGui toolbar's button for one QAction: its text, icon and state, triggering the
// action or popping up its button's QMenu by the QToolButton popup mode.
QQC2.ToolButton {
    id: button

    required property ChromeAdapter chrome
    // A QAction ("action" is AbstractButton's own, final).
    required property QtObject qAction
    property string iconName
    // QToolButton::ToolButtonPopupMode: 0 delayed, 1 menu button, 2 instant; -1 no menu.
    property int popupMode: -1
    // Qt::ToolButtonStyle
    property int buttonStyle: Qt.ToolButtonTextBesideIcon
    property int iconSize: Kirigami.Units.iconSizes.smallMedium

    // A menu button's arrow part, under the pointer.
    readonly property bool overArrow: popupMode === 1 && hover.point.position.x >= width - Kirigami.Units.gridUnit
    property bool arrowPressed: false

    // The action can go first when KXmlGui rebuilds or the window closes.
    text: qAction?.iconText ?? ""
    icon.name: iconName
    icon.width: iconSize
    icon.height: iconSize
    display: {
        switch (buttonStyle) {
        case Qt.ToolButtonIconOnly:
            return QQC2.AbstractButton.IconOnly;
        case Qt.ToolButtonTextOnly:
            return QQC2.AbstractButton.TextOnly;
        case Qt.ToolButtonTextUnderIcon:
            return QQC2.AbstractButton.TextUnderIcon;
        default:
            return QQC2.AbstractButton.TextBesideIcon;
        }
    }
    // A QToolBar button doesn't take the focus from the terminal.
    focusPolicy: Qt.NoFocus
    enabled: qAction?.enabled ?? false
    checkable: qAction?.checkable ?? false
    checked: qAction?.checked ?? false
    Accessible.role: popupMode >= 0 ? Accessible.ButtonMenu : Accessible.Button

    QQC2.ToolTip.text: qAction?.toolTip ?? ""
    QQC2.ToolTip.visible: hovered && QQC2.ToolTip.text !== "" && QQC2.ToolTip.text !== text
    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

    onPressed: {
        arrowPressed = overArrow;
        if (popupMode === 2 || arrowPressed) {
            chrome.popupButtonMenu(qAction, button);
        }
    }
    onClicked: {
        if (popupMode !== 2 && !arrowPressed) {
            chrome.trigger(qAction);
        }
        // The action owns the checked state: clicking mustn't toggle the button by itself.
        checked = Qt.binding(() => button.qAction?.checked ?? false);
    }
    onPressAndHold: {
        if (popupMode === 0) {
            chrome.popupButtonMenu(qAction, button);
        }
    }

    HoverHandler {
        id: hover
    }

    // After the action's own handlers (KHamburgerMenu opens its menu there).
    Connections {
        target: button.qAction
        function onTriggered() {
            button.chrome.actionTriggered(button.qAction, button);
        }
    }
    Connections {
        target: button.chrome
        function onButtonMenuRequested(requested) {
            if (requested === button.qAction && button.visible) {
                button.chrome.moveOpenMenu(button);
            }
        }
    }
}
