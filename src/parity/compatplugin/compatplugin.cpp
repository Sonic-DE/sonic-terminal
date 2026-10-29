// SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MainWindow.h"
#include "pluginsystem/IKonsolePlugin.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"

#include <KActionCollection>
#include <KPluginFactory>

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QPointer>

class CompatPlugin : public Konsole::IKonsolePlugin {
public:
    CompatPlugin(QObject* parent, const QVariantList& args)
        : Konsole::IKonsolePlugin(parent, args)
    {
        setName(QStringLiteral("CompatPlugin"));
    }

    void createWidgetsForMainWindow(Konsole::MainWindow* mainWindow) override
    {
        auto* dock = new QDockWidget(mainWindow);
        dock->setWindowTitle(QStringLiteral("Compat Plugin"));
        dock->setObjectName(QStringLiteral("CompatPluginDock"));
        dock->setWidget(new QLabel(QStringLiteral("compat plugin panel"), dock));
        mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dock);

        _action = new QAction(QStringLiteral("Compat Plugin Action"), this);
        mainWindow->actionCollection()->addAction(QStringLiteral("compatplugin-action"), _action);
        mainWindow->actionCollection()->setDefaultShortcut(_action, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9));
        connect(_action, &QAction::triggered, this, [this, mainWindow] {
            log(QStringLiteral("action triggered"));
            mainWindow->newTab();
            if (_controller && _controller->session()) {
                _controller->session()->sendTextToTerminal(QStringLiteral("echo from-compat-plugin"), QLatin1Char('\r'));
                if (!_controller->session()->views().isEmpty()) {
                    _controller->session()->views().at(0)->setFocus();
                }
            }
        });
        log(QStringLiteral("createWidgetsForMainWindow"));
    }

    void activeViewChanged(Konsole::SessionController* controller, Konsole::MainWindow*) override
    {
        _controller = controller;
        log(QStringLiteral("activeViewChanged view=%1").arg(controller && controller->view() ? QStringLiteral("yes") : QStringLiteral("no")));
    }

    QList<QAction*> menuBarActions(Konsole::MainWindow*) const override
    {
        return _action ? QList<QAction*>{_action} : QList<QAction*>{};
    }

private:
    static void log(const QString& line)
    {
        QFile file(QDir::home().filePath(QStringLiteral("compatplugin.log")));
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            file.write(line.toUtf8() + '\n');
        }
    }

    QAction* _action = nullptr;
    QPointer<Konsole::SessionController> _controller;
};

K_PLUGIN_CLASS_WITH_JSON(CompatPlugin, "compatplugin.json")

#include "compatplugin.moc"
