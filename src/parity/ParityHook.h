// SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QCommandLineOption>
#include <QString>

#include "konsoleapp_export.h"

namespace Konsole {
// Drives the UI from a script for the parity tests (tools/parity). Built only with
// KONSOLE_PARITY_HOOKS; one JSON object per line, see tools/parity/README.md.
namespace ParityHook {
    KONSOLEAPP_EXPORT QCommandLineOption commandLineOption();
    KONSOLEAPP_EXPORT void start(const QString& scriptPath);
}
}
