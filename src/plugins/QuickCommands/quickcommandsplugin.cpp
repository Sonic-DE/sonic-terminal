// This file was part of the KDE libraries
// SPDX-FileCopyrightText: 2022 Tao Guo <guotao945@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quickcommandsplugin.h"
#include "quickcommanddata.h"
#include "quickcommandsbackend.h"
#include "quickcommandsmodel.h"

#include <KActionCollection>
#include <KCommandBar>
#include <QSettings>

#include "MainWindow.h"
#include "quick/FrameworkDialogScope.h"
#include "quick/WindowController.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include <KLocalizedString>
#include <KMessageBox>

K_PLUGIN_CLASS_WITH_JSON(QuickCommandsPlugin, "konsole_quickcommands.json")

struct QuickCommandsPlugin::Private {
    QuickCommandsModel model;
    QAction *showQuickAccess = nullptr;
    QMap<Konsole::MainWindow*, QuickCommandsBackend*> backendForWindow;
    QMap<Konsole::MainWindow*, QAction*> toggleForWindow;
};

QuickCommandsPlugin::QuickCommandsPlugin(QObject *object, const QVariantList &args)
    : Konsole::IKonsolePlugin(object, args)
    , priv(std::make_unique<Private>())
{
    priv->showQuickAccess = new QAction();
    setName(QStringLiteral("QuickCommands"));
}

QuickCommandsPlugin::~QuickCommandsPlugin() = default;

void QuickCommandsPlugin::createWidgetsForMainWindow(Konsole::MainWindow *mainWindow)
{
    Q_UNUSED(mainWindow)
}

QList<Konsole::QmlPanel> QuickCommandsPlugin::panels(Konsole::MainWindow* mainWindow)
{
    auto* backend = new QuickCommandsBackend(&priv->model, mainWindow, mainWindow);
    connect(backend, &QuickCommandsBackend::quickAccessShortcutChanged, this, [this, mainWindow](const QKeySequence& s) {
        mainWindow->actionCollection()->setDefaultShortcut(priv->showQuickAccess, s);

        QString sequenceText = s.toString();
        QSettings settings;
        settings.beginGroup(QStringLiteral("plugins"));
        settings.beginGroup(QStringLiteral("quickcommands"));
        settings.setValue(QStringLiteral("shortcut"), sequenceText);
        settings.sync();
    });
    priv->backendForWindow[mainWindow] = backend;

    auto* toggleVisibilityAction = new QAction(i18n("Show Quick Commands"), mainWindow);
    toggleVisibilityAction->setCheckable(true);
    mainWindow->actionCollection()->setDefaultShortcut(toggleVisibilityAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F1));
    priv->toggleForWindow[mainWindow] = toggleVisibilityAction;

    return {{i18n("Quick Commands"),
        QStringLiteral("QuickCommandsDock"),
        QUrl(QStringLiteral("qrc:/org/sonicde/terminal/plugins/quickcommands/QuickCommandsPanel.qml")),
        {{QStringLiteral("backend"), QVariant::fromValue<QObject*>(backend)}},
        toggleVisibilityAction}};
}

void QuickCommandsPlugin::activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow)
{
    if (mainWindow == nullptr || controller == nullptr) {
        return;
    }

    priv->showQuickAccess->deleteLater();
    priv->showQuickAccess = new QAction(i18n("Show Quick Access"));

    QSettings settings;
    settings.beginGroup(QStringLiteral("plugins"));
    settings.beginGroup(QStringLiteral("quickcommands"));

    const QKeySequence def(Qt::CTRL | Qt::ALT | Qt::Key_G);
    const QString defText = def.toString();
    const QString entry = settings.value(QStringLiteral("shortcut"), defText).toString();
    const QKeySequence shortcutEntry(entry);

    mainWindow->actionCollection()->setDefaultShortcut(priv->showQuickAccess, shortcutEntry);

    // controller may exist while its view is being torn down
    QPointer<Konsole::TerminalDisplay> terminalDisplay = controller->view();
    if (terminalDisplay == nullptr) {
        return;
    }
    terminalDisplay->addAction(priv->showQuickAccess);

    QPointer<Konsole::SessionController> controllerPtr(controller);
    QAction* quickAccess = priv->showQuickAccess;
    connect(priv->showQuickAccess, &QAction::triggered, this, [this, quickAccess, mainWindow, controllerPtr] {
        QList<QAction *> actions;
        for (int i = 0; i < priv->model.rowCount(); i++) {
            QModelIndex folder = priv->model.index(i, 0);
            for (int e = 0; e < priv->model.rowCount(folder); e++) {
                QModelIndex idx = priv->model.index(e, 0, folder);
                QAction* act = new QAction(idx.data().toString(), quickAccess);
                connect(act, &QAction::triggered, this, [this, idx, controllerPtr] {
                    if (controllerPtr == nullptr) {
                        return;
                    }
                    const auto item = priv->model.itemFromIndex(idx);
                    const auto data = item->data(QuickCommandsModel::QuickCommandRole).value<QuickCommandData>();
                    if (controllerPtr->session() != nullptr) {
                        controllerPtr->session()->sendTextToTerminal(data.command, QLatin1Char('\r'));
                    }
                });
                actions.append(act);
            }
        }

        if (actions.isEmpty()) // no quick commands found, must give feedback to the user about that
        {
            const QString feedbackMessage = i18n("No quick commands found. You can add one on Plugins -> Quick Commands");
            const QString feedbackTitle = i18n("Plugins - Quick Commands");
            Konsole::FrameworkDialogScope scope(QStringLiteral("no quick commands"));
            KMessageBox::error(mainWindow, feedbackMessage, feedbackTitle);
            return;
        }

        if (auto* windowController = Konsole::WindowController::controllerOf(mainWindow)) {
            windowController->openCommandBar({KCommandBar::ActionGroup{i18n("Quick Commands"), actions}});
        }
    });

    priv->backendForWindow[mainWindow]->setCurrentController(controller);
}

QList<QAction *> QuickCommandsPlugin::menuBarActions(Konsole::MainWindow *mainWindow) const
{
    return {priv->toggleForWindow[mainWindow]};
}

#include "moc_quickcommandsplugin.cpp"
#include "quickcommandsplugin.moc"
