/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KToolBarEditor>

#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace Konsole {
// sonic-frameworks-xml-gui's toolbar editor in the module; MainWindow::configureToolbars()
// loads one for the Configure Toolbars dialog.
struct ToolBarEditorForeign {
    Q_GADGET
    QML_FOREIGN(KToolBarEditor)
    QML_NAMED_ELEMENT(KToolBarEditor)
    QML_UNCREATABLE("Loaded by the window that opens the dialog")
};
}
