/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "TerminalViewRegistry.h"

#include <QHash>

namespace Konsole::TerminalViewRegistry {
namespace {
    QHash<const TerminalDisplay*, TerminalView*>& views()
    {
        static QHash<const TerminalDisplay*, TerminalView*> map;
        return map;
    }
}

void add(const TerminalDisplay* display, TerminalView* view)
{
    views().insert(display, view);
}

void remove(const TerminalDisplay* display)
{
    views().remove(display);
}

TerminalView* view(const TerminalDisplay* display)
{
    return views().value(display);
}

TerminalDisplay* display(const TerminalView* view)
{
    return const_cast<TerminalDisplay*>(views().key(const_cast<TerminalView*>(view)));
}
}
