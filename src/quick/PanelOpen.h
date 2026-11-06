/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleapp_export.h"

#include <QString>
#include <QStringList>

class QCommandLineParser;

// A started terminal asks the Terminal Panel widget for a tab instead of opening a window.
namespace Konsole::PanelOpen {
// Raise on any incompatible change.
inline constexpr quint16 protocolVersion = 1;
inline constexpr qint64 maxPayload = 64 * 1024;

enum MessageType : quint32 {
    // The working directory in UTF-8, or empty for the profile's.
    OpenTab = 1,
    // No payload.
    Opened = 2,
};

KONSOLEAPP_EXPORT QString socketPath();

// Whether these arguments ask only for a terminal, which a panel's tab can be.
KONSOLEAPP_EXPORT bool canUsePanel(const QCommandLineParser& parser, const QStringList& customCommand);

// False when no panel took it, for any reason: the caller opens a window.
KONSOLEAPP_EXPORT bool openTab(const QString& directory);
}
