/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDBusAbstractAdaptor>
#include <QPointer>
#include <QStringList>

class KXmlGuiWindow;
class QQuickWindow;

namespace Konsole {
/**
 * KXmlGui's org.kde.KMainWindow adaptor (kmainwindowiface_p.h) for a compat window, with
 * winId() and grabWindowToClipBoard() acting on its QML window. Made a child of the compat
 * window after KXmlGui's, it replaces that one on the same object path (Qt keeps the last
 * adaptor of an interface), so the path's introspection stays the same.
 */
class MainWindowInterface : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KMainWindow")

public:
    MainWindowInterface(KXmlGuiWindow* compatWindow, QQuickWindow* shownWindow);

    // The same slots, in the same order and with the same argument names, as KXmlGui's.
public Q_SLOTS:
    QStringList actions();
    bool activateAction(const QString& action);
    bool disableAction(const QString& action);
    bool enableAction(const QString& action);
    bool actionIsEnabled(const QString& action);
    QString actionToolTip(const QString& action);
    qlonglong winId();
    void grabWindowToClipBoard();

private:
    KXmlGuiWindow* const _compatWindow;
    QPointer<QQuickWindow> _shownWindow;
};
}
