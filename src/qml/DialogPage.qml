/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Layouts

// A page of a PageDialog or TabbedPageDialog (a KPageWidgetItem): its name, icon and content.
ColumnLayout {
    property string title
    property string iconName
}
