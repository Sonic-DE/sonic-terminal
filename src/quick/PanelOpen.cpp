/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "PanelOpen.h"

#include <Sonic/Ipc/Client>
#include <Sonic/Ipc/Server>
#include <Sonic/Logging/Log>

#include <QCommandLineParser>

#include <algorithm>

namespace Konsole::PanelOpen {

QString socketPath()
{
    return Sonic::Ipc::Server::userSocketPath(QStringLiteral("terminal-panel.sock"));
}

bool canUsePanel(const QCommandLineParser& parser, const QStringList& customCommand)
{
    if (!customCommand.isEmpty() || !parser.positionalArguments().isEmpty()) {
        return false;
    }
    const QStringList options = parser.optionNames();
    return std::all_of(options.cbegin(), options.cend(), [](const QString& option) {
        return option == QLatin1String("workdir") || option == QLatin1String("new-tab");
    });
}

bool openTab(const QString& directory)
{
    const QString path = socketPath();
    if (path.isEmpty()) {
        return false;
    }
    QString error;
    const auto reply = Sonic::Ipc::Client::request(path, OpenTab, directory.toUtf8(), protocolVersion, 1000, &error, maxPayload);
    if (!reply) {
        sonicDebug() << "no panel takes the terminal:" << error;
        return false;
    }
    return reply->type == Opened;
}
}
