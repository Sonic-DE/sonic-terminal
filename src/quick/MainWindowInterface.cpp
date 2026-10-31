/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "MainWindowInterface.h"

#include <KActionCollection>
#include <KXmlGuiWindow>

#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QQuickWindow>

#include <algorithm>

namespace Konsole {
MainWindowInterface::MainWindowInterface(KXmlGuiWindow* compatWindow, QQuickWindow* shownWindow)
    : QDBusAbstractAdaptor(compatWindow)
    , _compatWindow(compatWindow)
    , _shownWindow(shownWindow)
{
}

QStringList MainWindowInterface::actions()
{
    QStringList names;
    const QList<QAction*> actions = _compatWindow->actionCollection()->actions();
    for (QAction* action : actions) {
        const QList<QObject*> objects = action->associatedObjects();
        if (std::any_of(objects.cbegin(), objects.cend(), [](QObject* object) {
                return object->isWidgetType();
            })) {
            names.append(action->objectName());
        }
    }
    return names;
}

bool MainWindowInterface::activateAction(const QString& action)
{
    QAction* found = _compatWindow->actionCollection()->action(action);
    if (found) {
        found->trigger();
    }
    return found;
}

bool MainWindowInterface::disableAction(const QString& action)
{
    QAction* found = _compatWindow->actionCollection()->action(action);
    if (found) {
        found->setEnabled(false);
    }
    return found;
}

bool MainWindowInterface::enableAction(const QString& action)
{
    QAction* found = _compatWindow->actionCollection()->action(action);
    if (found) {
        found->setEnabled(true);
    }
    return found;
}

bool MainWindowInterface::actionIsEnabled(const QString& action)
{
    QAction* found = _compatWindow->actionCollection()->action(action);
    return found && found->isEnabled();
}

QString MainWindowInterface::actionToolTip(const QString& action)
{
    QAction* found = _compatWindow->actionCollection()->action(action);
    return found ? found->toolTip() : QStringLiteral("Error no such object!");
}

qlonglong MainWindowInterface::winId()
{
    return _shownWindow ? qlonglong(_shownWindow->winId()) : 0;
}

void MainWindowInterface::grabWindowToClipBoard()
{
    if (_shownWindow) {
        QGuiApplication::clipboard()->setImage(_shownWindow->grabWindow());
    }
}
}

#include "moc_MainWindowInterface.cpp"
