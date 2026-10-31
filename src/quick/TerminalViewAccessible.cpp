/*
    SPDX-FileCopyrightText: 2012 Frederik Gladhorn <gladhorn@kde.org>
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "TerminalViewAccessible.h"

#include "Screen.h"
#include "ScreenWindow.h"
#include "TerminalView.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "terminalDisplay/TerminalFonts.h"

#include <QQuickWindow>

namespace Konsole {
TerminalViewAccessible::TerminalViewAccessible(TerminalView* view)
    : QAccessibleObject(view)
{
}

TerminalView* TerminalViewAccessible::view() const
{
    return static_cast<TerminalView*>(object());
}

TerminalDisplay* TerminalViewAccessible::display() const
{
    return view() ? view()->display() : nullptr;
}

int TerminalViewAccessible::positionToOffset(int column, int line) const
{
    return line * display()->_usedColumns + column;
}

int TerminalViewAccessible::lineForOffset(int offset) const
{
    return offset / display()->_usedColumns;
}

int TerminalViewAccessible::columnForOffset(int offset) const
{
    return offset % display()->_usedColumns;
}

QAccessibleInterface* TerminalViewAccessible::parent() const
{
    // QAccessibleQuickItem::parent(): Qt Quick only answers for accessible items.
    for (QQuickItem* item = view()->parentItem(); item; item = item->parentItem()) {
        if (QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(item)) {
            return iface;
        }
    }
    return view()->window() ? QAccessible::queryAccessibleInterface(view()->window()) : nullptr;
}

QAccessibleInterface* TerminalViewAccessible::child(int index) const
{
    Q_UNUSED(index)
    return nullptr;
}

int TerminalViewAccessible::childCount() const
{
    return 0;
}

int TerminalViewAccessible::indexOfChild(const QAccessibleInterface* child) const
{
    Q_UNUSED(child)
    return -1;
}

QWindow* TerminalViewAccessible::window() const
{
    return view()->window();
}

QRect TerminalViewAccessible::rect() const
{
    if (!view()->window()) {
        return {};
    }
    const QRectF scene = view()->mapRectToScene(QRectF(QPointF(0, 0), view()->size()));
    return QRect(view()->window()->mapToGlobal(scene.topLeft().toPoint()), scene.size().toSize());
}

QAccessible::Role TerminalViewAccessible::role() const
{
    return QAccessible::Terminal;
}

QAccessible::State TerminalViewAccessible::state() const
{
    QAccessible::State state;
    state.focusable = true;
    state.focused = view()->hasActiveFocus();
    state.invisible = !view()->isVisible();
    state.multiLine = true;
    return state;
}

QString TerminalViewAccessible::text(QAccessible::Text t) const
{
    switch (t) {
    case QAccessible::Value:
        if (display()->screenWindow() == nullptr) {
            return QString();
        }
        return display()->screenWindow()->screen()->text(0, display()->_usedColumns * display()->_usedLines, Screen::PreserveLineBreaks);
    case QAccessible::Name:
        // TerminalDisplayAccessible's name: the session's title when the view was made.
        return display()->sessionController() ? display()->sessionController()->userTitle() : QString();
    default:
        return QString();
    }
}

void* TerminalViewAccessible::interface_cast(QAccessible::InterfaceType type)
{
    if (type == QAccessible::TextInterface) {
        return static_cast<QAccessibleTextInterface*>(this);
    }
    return QAccessibleObject::interface_cast(type);
}

// The text interface is TerminalDisplayAccessible's, on the facade's state.

QString TerminalViewAccessible::text(int startOffset, int endOffset) const
{
    if (display()->screenWindow() == nullptr) {
        return QString();
    }
    const auto maxIndex = display()->_usedColumns * display()->_usedLines;
    startOffset = qBound(0, startOffset, maxIndex);
    endOffset = qBound(startOffset, endOffset, maxIndex);
    return display()->screenWindow()->screen()->text(startOffset, endOffset, Screen::PreserveLineBreaks);
}

int TerminalViewAccessible::characterCount() const
{
    return display()->_usedLines * display()->_usedColumns;
}

int TerminalViewAccessible::selectionCount() const
{
    if (display()->screenWindow() == nullptr) {
        return 0;
    }
    int startLine = 0;
    int startColumn = 0;
    int endLine = 0;
    int endColumn = 0;
    display()->screenWindow()->getSelectionStart(startColumn, startLine);
    display()->screenWindow()->getSelectionEnd(endColumn, endLine);
    return ((startLine == endLine) && (startColumn == endColumn)) ? 0 : 1;
}

void TerminalViewAccessible::selection(int selectionIndex, int* startOffset, int* endOffset) const
{
    *startOffset = 0;
    *endOffset = 0;
    if ((display()->screenWindow() == nullptr) || (selectionIndex != 0)) {
        return;
    }
    int startLine = 0;
    int startColumn = 0;
    int endLine = 0;
    int endColumn = 0;
    display()->screenWindow()->getSelectionStart(startColumn, startLine);
    display()->screenWindow()->getSelectionEnd(endColumn, endLine);
    if ((startLine == endLine) && (startColumn == endColumn)) {
        return;
    }
    *startOffset = positionToOffset(startColumn, startLine);
    *endOffset = positionToOffset(endColumn, endLine);
}

void TerminalViewAccessible::addSelection(int startOffset, int endOffset)
{
    if (display()->screenWindow() == nullptr) {
        return;
    }
    display()->screenWindow()->setSelectionStart(columnForOffset(startOffset), lineForOffset(startOffset), false);
    display()->screenWindow()->setSelectionEnd(columnForOffset(endOffset), lineForOffset(endOffset), display()->trimTrailingWhitespace());
}

void TerminalViewAccessible::setSelection(int selectionIndex, int startOffset, int endOffset)
{
    if (selectionIndex != 0) {
        return;
    }
    addSelection(startOffset, endOffset);
}

void TerminalViewAccessible::removeSelection(int selectionIndex)
{
    if ((display()->screenWindow() == nullptr) || (selectionIndex != 0)) {
        return;
    }
    display()->screenWindow()->clearSelection();
}

QRect TerminalViewAccessible::characterRect(int offset) const
{
    const int row = offset / display()->_usedColumns;
    const int col = offset - row * display()->_usedColumns;
    const QPoint position(col * display()->terminalFont()->fontWidth(), row * display()->terminalFont()->fontHeight());
    return QRect(position, QSize(display()->terminalFont()->fontWidth(), display()->terminalFont()->fontHeight()));
}

int TerminalViewAccessible::offsetAtPoint(const QPoint& point) const
{
    Q_UNUSED(point)
    return 0;
}

void TerminalViewAccessible::scrollToSubstring(int startIndex, int endIndex)
{
    Q_UNUSED(startIndex)
    Q_UNUSED(endIndex)
}

QString TerminalViewAccessible::attributes(int offset, int* startOffset, int* endOffset) const
{
    Q_UNUSED(offset)
    *startOffset = 0;
    *endOffset = characterCount();
    return QString();
}

int TerminalViewAccessible::cursorPosition() const
{
    if (display()->screenWindow() == nullptr) {
        return 0;
    }
    const int offset = display()->_usedColumns * display()->screenWindow()->screen()->getCursorY();
    return offset + display()->screenWindow()->screen()->getCursorX();
}

void TerminalViewAccessible::setCursorPosition(int position)
{
    if (display()->screenWindow() == nullptr) {
        return;
    }
    display()->screenWindow()->screen()->setCursorYX(lineForOffset(position), columnForOffset(position));
}
}
