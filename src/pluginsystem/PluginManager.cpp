/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "PluginManager.h"

#include "IKonsolePlugin.h"
#include "IKonsoleQmlPlugin.h"
#include "MainWindow.h"
#include "quick/DockAdapter.h"

#include <Sonic/Logging/Log>

#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <QAction>
#include <QDockWidget>

namespace Konsole
{
struct PluginManagerPrivate {
    std::vector<IKonsolePlugin *> plugins;
};

PluginManager::PluginManager()
    : d(std::make_unique<PluginManagerPrivate>())
{
}

PluginManager::~PluginManager()
{
    qDeleteAll(d->plugins);
}

void PluginManager::loadAllPlugins()
{
    QVector<KPluginMetaData> pluginMetaData = KPluginMetaData::findPlugins(QStringLiteral("konsoleplugins"), [](const KPluginMetaData &data) {
        // Compare RELEASE_SERVICE_VERSION MAJOR and MINOR only: XX.YY
        auto plugin_version = QString(data.version()).left(5);
        auto release_version = QLatin1String(RELEASE_SERVICE_VERSION).left(5);
        if (plugin_version == release_version) {
            return true;
        } else {
            sonicWarning() << "Ignoring" << data.name() << "plugin version (" << plugin_version << ") doesn't match release version (" << release_version
                           << ")";
            return false;
        }
    });
    for (const auto &metaData : std::as_const(pluginMetaData)) {
        const KPluginFactory::Result result = KPluginFactory::instantiatePlugin<IKonsolePlugin>(metaData);
        if (!result) {
            sonicWarning() << "plugin not loaded:" << metaData.name() << result.errorString;
            continue;
        }
        sonicInfo() << "plugin loaded:" << metaData.name() << (qobject_cast<IKonsoleQmlPlugin*>(result.plugin) ? "(QML interface)" : "(widget interface)");

        d->plugins.push_back(result.plugin);
    }
}

void PluginManager::registerMainWindow(Konsole::MainWindow *window)
{
    QList<QAction *> internalPluginSubmenus;
    for (auto *plugin : d->plugins) {
        plugin->addMainWindow(window);
        if (auto* qmlPlugin = qobject_cast<IKonsoleQmlPlugin*>(plugin)) {
            const QList<QmlPanel> panels = qmlPlugin->panels(window);
            for (const QmlPanel& panel : panels) {
                QDockWidget* dock = DockAdapter::addQmlPanel(window, panel.title, panel.objectName, panel.source, panel.properties);
                if (panel.toggleAction) {
                    connect(panel.toggleAction, &QAction::triggered, dock, &QDockWidget::setVisible);
                    connect(dock, &QDockWidget::visibilityChanged, panel.toggleAction, &QAction::setChecked);
                }
            }
        }
        internalPluginSubmenus.append(plugin->menuBarActions(window));
        window->addPlugin(plugin);
    }

    if (internalPluginSubmenus.isEmpty()) {
        auto *emptyMenuAct = new QAction(i18n("No plugins available"), this);
        emptyMenuAct->setEnabled(false);
        internalPluginSubmenus.append(emptyMenuAct);
    }

    window->setPluginsActions(internalPluginSubmenus);
}

std::vector<IKonsolePlugin *> PluginManager::plugins() const
{
    return d->plugins;
}

}

#include "moc_PluginManager.cpp"
