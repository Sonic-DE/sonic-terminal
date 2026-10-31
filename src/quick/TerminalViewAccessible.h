/*
    SPDX-FileCopyrightText: 2012 Frederik Gladhorn <gladhorn@kde.org>
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAccessibleObject>
#include <QAccessibleTextInterface>
#include <QPointer>

namespace Konsole {
class TerminalDisplay;
class TerminalView;

/**
 * TerminalDisplayAccessible for a view in a QML scene: the same terminal role, name and
 * text interface, read from the view's facade; its place in the accessibility tree is the
 * item's (QAccessibleQuickItem's rules: the nearest accessible ancestor, else the window).
 */
class TerminalViewAccessible : public QAccessibleObject, public QAccessibleTextInterface {
public:
    explicit TerminalViewAccessible(TerminalView* view);

    QAccessibleInterface* parent() const override;
    QAccessibleInterface* child(int index) const override;
    int childCount() const override;
    int indexOfChild(const QAccessibleInterface* child) const override;
    QWindow* window() const override;
    QRect rect() const override;
    QAccessible::Role role() const override;
    QAccessible::State state() const override;
    QString text(QAccessible::Text t) const override;
    void* interface_cast(QAccessible::InterfaceType type) override;

    QString text(int startOffset, int endOffset) const override;
    int characterCount() const override;
    int selectionCount() const override;
    void selection(int selectionIndex, int* startOffset, int* endOffset) const override;
    void addSelection(int startOffset, int endOffset) override;
    void setSelection(int selectionIndex, int startOffset, int endOffset) override;
    void removeSelection(int selectionIndex) override;
    QRect characterRect(int offset) const override;
    int offsetAtPoint(const QPoint& point) const override;
    void scrollToSubstring(int startIndex, int endIndex) override;
    QString attributes(int offset, int* startOffset, int* endOffset) const override;
    int cursorPosition() const override;
    void setCursorPosition(int position) override;

private:
    TerminalView* view() const;
    TerminalDisplay* display() const;
    int positionToOffset(int column, int line) const;
    int lineForOffset(int offset) const;
    int columnForOffset(int offset) const;
};
}
