/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshmanagerplugin.h"

#include "sshconfigurationdata.h"
#include "sshmanagerbackend.h"
#include "sshmanagermodel.h"

#include "ProcessInfo.h"
#include "konsoledebug.h"
#include "quick/WindowController.h"
#include "session/Session.h"
#include "session/SessionController.h"

#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTimer>

#include <KActionCollection>
#include <KCommandBar>
#include <KCrash>
#include <KLocalizedString>
#include <KMessageBox>
#include <QSettings>
#include <kcommandbar.h>

#include "MainWindow.h"
#include "quick/FrameworkDialogScope.h"
#include "terminalDisplay/TerminalDisplay.h"

K_PLUGIN_CLASS_WITH_JSON(SSHManagerPlugin, "konsole_sshmanager.json")

struct SSHManagerPluginPrivate {
    SSHManagerModel model;

    QMap<Konsole::MainWindow*, SSHManagerBackend*> backendForWindow;
    QMap<Konsole::MainWindow*, QAction*> toggleForWindow;
    QAction *showQuickAccess = nullptr;
};

SSHManagerPlugin::SSHManagerPlugin(QObject *object, const QVariantList &args)
    : Konsole::IKonsolePlugin(object, args)
    , d(std::make_unique<SSHManagerPluginPrivate>())
{
    d->showQuickAccess = new QAction();

    setName(QStringLiteral("SshManager"));
    KCrash::initialize();
}

SSHManagerPlugin::~SSHManagerPlugin()
{
}

void SSHManagerPlugin::createWidgetsForMainWindow(Konsole::MainWindow *mainWindow)
{
    Q_UNUSED(mainWindow)
}

QList<Konsole::QmlPanel> SSHManagerPlugin::panels(Konsole::MainWindow* mainWindow)
{
    auto* backend = new SSHManagerBackend(&d->model, mainWindow, mainWindow);
    d->backendForWindow[mainWindow] = backend;

    connect(backend, &SSHManagerBackend::quickAccessShortcutChanged, this, [this, mainWindow](const QKeySequence& s) {
        mainWindow->actionCollection()->setDefaultShortcut(d->showQuickAccess, s);

        QString sequenceText = s.toString();
        QSettings settings;
        settings.beginGroup(QStringLiteral("plugins"));
        settings.beginGroup(QStringLiteral("sshplugin"));
        settings.setValue(QStringLiteral("ssh_shortcut"), sequenceText);
        settings.sync();
    });

    QAction *toggleVisibilityAction = new QAction(i18n("Show SSH Manager"), mainWindow);
    toggleVisibilityAction->setCheckable(true);
    mainWindow->actionCollection()->setDefaultShortcut(toggleVisibilityAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F2));
    d->toggleForWindow[mainWindow] = toggleVisibilityAction;

    return {{i18n("SSH Manager"),
        QStringLiteral("SSHManagerDock"),
        QUrl(QStringLiteral("qrc:/org/sonicde/terminal/plugins/sshmanager/SSHManagerPanel.qml")),
        {{QStringLiteral("backend"), QVariant::fromValue<QObject*>(backend)}},
        toggleVisibilityAction}};
}

QList<QAction*> SSHManagerPlugin::menuBarActions(Konsole::MainWindow* mainWindow) const
{
    return {d->toggleForWindow[mainWindow]};
}

#include <iostream>

void SSHManagerPlugin::activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow)
{
    Q_ASSERT(controller);
    Q_ASSERT(mainWindow);

    auto terminalDisplay = controller->view();

    d->showQuickAccess->deleteLater();
    d->showQuickAccess = new QAction(i18n("Show Quick Access for SSH Actions"));

    QSettings settings;
    settings.beginGroup(QStringLiteral("plugins"));
    settings.beginGroup(QStringLiteral("sshplugin"));

    const QKeySequence def(Qt::CTRL | Qt::ALT | Qt::Key_H);
    const QString defText = def.toString();
    const QString entry = settings.value(QStringLiteral("ssh_shortcut"), defText).toString();
    const QKeySequence shortcutEntry(entry);

    mainWindow->actionCollection()->setDefaultShortcut(d->showQuickAccess, shortcutEntry);
    terminalDisplay->addAction(d->showQuickAccess);

    QAction* quickAccess = d->showQuickAccess;
    connect(d->showQuickAccess, &QAction::triggered, this, [this, quickAccess, mainWindow, controller] {
        QList<QAction *> actions;
        for (int i = 0; i < d->model.rowCount(); i++) {
            QModelIndex folder = d->model.index(i, 0);
            for (int e = 0; e < d->model.rowCount(folder); e++) {
                QModelIndex idx = d->model.index(e, 0, folder);
                QAction* act = new QAction(idx.data().toString(), quickAccess);
                connect(act, &QAction::triggered, this, [this, idx, controller] {
                    requestConnection(nullptr, &d->model, controller, idx);
                });
                actions.append(act);
            }
        }

        if (actions.isEmpty()) // no ssh config found, must give feedback to the user about that
        {
            const QString feedbackMessage = i18n("No saved SSH config found. You can add one on Plugins -> SSH Manager");
            const QString feedbackTitle = i18n("Plugins - SSH Manager");
            Konsole::FrameworkDialogScope scope(QStringLiteral("no ssh config"));
            KMessageBox::error(mainWindow, feedbackMessage, feedbackTitle);
            return;
        }

        if (auto* windowController = Konsole::WindowController::controllerOf(mainWindow)) {
            windowController->openCommandBar({KCommandBar::ActionGroup{i18n("SSH Entries"), actions}});
        }
    });

    if (mainWindow) {
        d->backendForWindow[mainWindow]->setCurrentController(controller);
    }
}

void SSHManagerPlugin::requestConnection(QSortFilterProxyModel *filterModel,
                                         QStandardItemModel *model,
                                         Konsole::SessionController *controller,
                                         const QModelIndex &idx)
{
    if (!controller) {
        return;
    }

    auto sourceIdx = filterModel ? filterModel->mapToSource(idx) : idx;
    if (sourceIdx.parent() == model->invisibleRootItem()->index()) {
        return;
    }

#ifndef Q_OS_WIN
    Konsole::FrameworkDialogScope scope(QStringLiteral("ssh command error"));
    Konsole::ProcessInfo *info = controller->session()->getProcessInfo();
    bool ok = false;
    QString processName = info->name(&ok);
    if (!ok) {
        KMessageBox::error(controller->view(), i18n("Could not get the process name, assume that we can't request a connection"), i18n("Error issuing SSH Command"));
        return;
    }

    if (!QVector<QString>({QStringLiteral("fish"),
                           QStringLiteral("bash"),
                           QStringLiteral("dash"),
                           QStringLiteral("sh"),
                           QStringLiteral("csh"),
                           QStringLiteral("ksh"),
                           QStringLiteral("zsh")})
             .contains(processName)) {
        KMessageBox::error(controller->view(), i18n("Can't issue SSH command outside the shell application (eg, bash, zsh, sh)"), i18n("Error issuing SSH Command"));
        return;
    }
#else
    // FIXME: Can we do this on windows?
#endif

    auto item = model->itemFromIndex(sourceIdx);
    auto data = item->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();

    QString sshCommand = QStringLiteral("ssh ");
    if (data.useSshConfig) {
        // useSshConfig is defined in sshwidget.ui:165 -> meaning whether the ssh config is taken from .ssh/config
        // call that host by data.name (which is the identifier as in sshmanagermodel.cpp:263)
        sshCommand += data.name;
    } else {
        // if (!data.useSshConfig)
        // useSshConfig is false aka not set, so we just a assume a manual entry in terms of an entry that was added manually to the sshmanager 
        if (data.sshKey.length()) {
            sshCommand += QStringLiteral("-i %1 ").arg(data.sshKey);
        }

        if (data.port.length()) {
            sshCommand += QStringLiteral("-p %1 ").arg(data.port);
        }

        if (!data.username.isEmpty()) {
            sshCommand += data.username + QLatin1Char('@');
        }
        
        if (!data.host.isEmpty()) {
            sshCommand += data.host;
        }
    }

    controller->session()->sendTextToTerminal(sshCommand, QLatin1Char('\r'));
    if (controller->session()->views().count()) {
        controller->session()->views().at(0)->setFocus();
    }
}

#include "moc_sshmanagerplugin.cpp"
#include "sshmanagerplugin.moc"
