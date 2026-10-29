/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

namespace Konsole {
class TerminalDisplay;
class TerminalView;

// Pairs each TerminalDisplay facade with the TerminalView that implements it; the facade's
// declaration is frozen for plugin ABI, so the link can't be a member.
namespace TerminalViewRegistry {
    KONSOLEPRIVATE_EXPORT void add(const TerminalDisplay* display, TerminalView* view);
    KONSOLEPRIVATE_EXPORT void remove(const TerminalDisplay* display);
    KONSOLEPRIVATE_EXPORT TerminalView* view(const TerminalDisplay* display);
    KONSOLEPRIVATE_EXPORT TerminalDisplay* display(const TerminalView* view);
}
}
