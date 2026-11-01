/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QString>
#include <QVariantMap>

#include <functional>

class QObject;
class QQmlEngine;
class QQuickWindow;
class QWidget;

namespace Konsole {
/**
 * The QML side of the application's windows and dialogs: one engine, with the desktop
 * style and translations, and the dialogs of the org.sonicde.terminal module opened as
 * windows of their own.
 */
namespace QmlDialogs {
    KONSOLEPRIVATE_EXPORT QQmlEngine* engine();

    /**
     * Creates the dialog @p type (a DialogWindow) with @p properties, transient for the
     * window @p opener is shown in (a compat window's QML window, or the widget window of a
     * KPart host), with @p modality, and shows it. It is deleted once closed; it emits
     * accepted() or rejected() as a QDialog would. Null if the component didn't load.
     */
    KONSOLEPRIVATE_EXPORT QQuickWindow* open(const QString& type, QWidget* opener, const QVariantMap& properties = {},
        Qt::WindowModality modality = Qt::ApplicationModal);

    // connect(dialog, &QDialog::accepted, context, callback) for a QML dialog.
    KONSOLEPRIVATE_EXPORT void onAccepted(QQuickWindow* dialog, QObject* context, const std::function<void()>& callback);
}
}
