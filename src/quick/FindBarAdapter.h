/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "WidgetMirror.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPalette>
#include <QPointF>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QAbstractButton;
class QLineEdit;
class QToolButton;
class QWindow;

namespace Konsole {
class IncrementalSearchBar;
class TerminalView;

/**
 * The facade's (hidden) IncrementalSearchBar as data for the QML find bar: its layout, the
 * line edit's text, colours and text rectangle (QLineEdit::paintEvent()'s rule), the clear
 * button and the tool buttons. Typing edits the real line edit, so the bar's search timer
 * and signals work as before; buttons, the options menu, keys and the size grip act
 * through the bar. Geometry of the parts is relative to the bar.
 */
class KONSOLEPRIVATE_EXPORT FindBarAdapter : public WidgetMirror {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Konsole::TerminalView* view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY changed)
    Q_PROPERTY(QColor textColor READ textColor NOTIFY changed)
    Q_PROPERTY(QRect gripGeometry READ gripGeometry NOTIFY changed)
    Q_PROPERTY(QRect editGeometry READ editGeometry NOTIFY changed)
    Q_PROPERTY(QRect textGeometry READ textGeometry NOTIFY changed)
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText NOTIFY changed)
    Q_PROPERTY(QString editToolTip READ editToolTip NOTIFY changed)
    Q_PROPERTY(QFont editFont READ editFont NOTIFY changed)
    Q_PROPERTY(QColor editTextColor READ editTextColor NOTIFY changed)
    Q_PROPERTY(QPalette editPalette READ editPalette NOTIFY changed)
    Q_PROPERTY(int editFrameWidth READ editFrameWidth NOTIFY changed)
    Q_PROPERTY(QColor placeholderColor READ placeholderColor NOTIFY changed)
    Q_PROPERTY(QColor selectionColor READ selectionColor NOTIFY changed)
    Q_PROPERTY(QColor selectedTextColor READ selectedTextColor NOTIFY changed)
    Q_PROPERTY(QColor matchColor READ matchColor NOTIFY changed)
    Q_PROPERTY(bool hasMatchColor READ hasMatchColor NOTIFY changed)
    Q_PROPERTY(QRect clearButtonGeometry READ clearButtonGeometry NOTIFY changed)
    Q_PROPERTY(QImage clearButtonImage READ clearButtonImage NOTIFY changed)
    Q_PROPERTY(bool clearButtonShown READ clearButtonShown NOTIFY textChanged)
    Q_PROPERTY(QVariantList buttons READ buttons NOTIFY changed)

public:
    explicit FindBarAdapter(QObject* parent = nullptr);

    TerminalView* view() const;
    void setView(TerminalView* view);

    QColor backgroundColor() const;
    QColor textColor() const;
    QRect gripGeometry() const;
    QRect editGeometry() const;
    // The line of text inside the line edit, relative to the line edit.
    QRect textGeometry() const;
    QString text() const;
    QString placeholderText() const;
    QString editToolTip() const;
    QFont editFont() const;
    QColor editTextColor() const;
    // The line edit's palette as its frame is drawn: the bar inherits the terminal's palette
    // (its setPalette(qApp->palette()) sets no role), and a match colour replaces the base.
    QPalette editPalette() const;
    int editFrameWidth() const;
    QColor placeholderColor() const;
    QColor selectionColor() const;
    QColor selectedTextColor() const;
    // The found/not found background (IncrementalSearchBar::setFoundMatch()); invalid if none.
    QColor matchColor() const;
    bool hasMatchColor() const;
    // Relative to the line edit; the image is the icon at the line edit's icon size.
    QRect clearButtonGeometry() const;
    QImage clearButtonImage() const;
    bool clearButtonShown() const;
    // {geometry, iconName, iconWidth, iconHeight, enabled, checked, toolTip, menu}
    QVariantList buttons() const;

    Q_INVOKABLE void setText(const QString& text);
    Q_INVOKABLE void clear();
    // menuPosition: the QML button's global bottom-left, where a menu button's menu opens.
    Q_INVOKABLE void clickButton(int index, QPointF menuPosition);
    // True when the key was the bar's (IncrementalSearchBar's key filter and keyPressEvent()).
    Q_INVOKABLE bool key(int key, int modifiers, const QString& text);
    Q_INVOKABLE void gripPress(QPointF globalPosition);
    Q_INVOKABLE void gripMove(QPointF globalPosition, int buttons);
    Q_INVOKABLE void gripRelease();

Q_SIGNALS:
    void viewChanged();
    void changed();
    void textChanged();
    // The bar focused its line edit (shown, or focusLineEdit()).
    void focusRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void watch(QWidget* widget);
    QList<QToolButton*> shownButtons() const;
    QToolButton* clearButton() const;
    void connectWindow();

    QPointer<TerminalView> _view;
    QPointer<IncrementalSearchBar> _searchBar;
    QPointer<QWindow> _window;
    // QSizeGripPrivate's drag state.
    bool _gripPressed = false;
    QPoint _gripStart;
    QRect _gripGeometry;
    int _gripMaxDx = 0;
    int _gripMaxDy = 0;
};
}
