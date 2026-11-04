/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QList>
#include <QString>

class QObject;
class QWidget;

namespace Konsole {
/**
 * Marks a framework widget dialog kept by decision (file, colour, font, print, message boxes,
 * KXmlGui's and KBookmarks' dialogs…) at its call site. The top-level widgets shown or
 * created while a scope is open are recorded with its name, so the QML application's widget
 * check (NoWidgetsTest) can tell them from stray widgets. Place one in the function that opens
 * the dialog, before it.
 */
class KONSOLEPRIVATE_EXPORT FrameworkDialogScope {
public:
    explicit FrameworkDialogScope(const QString& name);
    ~FrameworkDialogScope();

    FrameworkDialogScope(const FrameworkDialogScope&) = delete;
    FrameworkDialogScope& operator=(const FrameworkDialogScope&) = delete;

    // A scope open until lifetime is destroyed: for dialogs a KIO job or a popup menu opens later.
    static void attach(QObject* lifetime, const QString& name);
    // The scope that recorded the widget's top-level, or one of its parents'; empty if none.
    static QString owner(const QWidget* widget);

private:
    QString _name;
    // The top-level widgets when the scope opened.
    QList<QWidget*> _before;
};
}
