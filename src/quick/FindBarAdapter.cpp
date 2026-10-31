/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "FindBarAdapter.h"

#include "CompatWindows.h"
#include "TerminalView.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "widgets/IncrementalSearchBar.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSizeGrip>
#include <QStyle>
#include <QStyleOptionFrame>
#include <QToolButton>
#include <QWindow>

namespace Konsole {
namespace {
    // QLineEditPrivate::horizontalMargin.
    constexpr int LineEditHorizontalMargin = 2;
}

FindBarAdapter::FindBarAdapter(QObject* parent)
    : WidgetMirror(parent)
{
    connect(CompatWindowsNotifier::instance(), &CompatWindowsNotifier::activeChanged, this, &FindBarAdapter::changed);
}

TerminalView* FindBarAdapter::view() const
{
    return _view;
}

void FindBarAdapter::setView(TerminalView* view)
{
    if (_view == view) {
        return;
    }
    if (_searchBar) {
        _searchBar->_searchEdit->disconnect(this);
        for (QWidget* widget : _searchBar->findChildren<QWidget*>()) {
            widget->removeEventFilter(this);
        }
        _searchBar->removeEventFilter(this);
    }
    if (_window) {
        _window->disconnect(this);
    }
    _view = view;
    _searchBar = view && view->display() ? view->display()->searchBar() : nullptr;
    mirror(_searchBar, view ? view->display() : nullptr);
    if (_searchBar) {
        watch(_searchBar);
        for (QWidget* widget : _searchBar->findChildren<QWidget*>()) {
            watch(widget);
        }
        connect(_searchBar->_searchEdit, &QLineEdit::textChanged, this, &FindBarAdapter::textChanged);
        for (QAction* action : {_searchBar->_regExpression, _searchBar->_highlightMatches, _searchBar->_reverseSearch, _searchBar->_noWrap}) {
            connect(action, &QAction::changed, this, &FindBarAdapter::changed);
        }
        connect(_searchBar->_caseSensitiveButton, &QAbstractButton::toggled, this, &FindBarAdapter::changed);
        connectWindow();
    }
    Q_EMIT viewChanged();
    Q_EMIT changed();
    Q_EMIT textChanged();
}

void FindBarAdapter::watch(QWidget* widget)
{
    widget->installEventFilter(this);
}

void FindBarAdapter::connectWindow()
{
    // The facade's window is never active, so setFocus() emits focusObjectChanged there.
    QWindow* window = _searchBar && _searchBar->window() ? _searchBar->window()->windowHandle() : nullptr;
    if (window == _window) {
        return;
    }
    if (_window) {
        _window->disconnect(this);
    }
    _window = window;
    if (_window) {
        connect(_window, &QWindow::focusObjectChanged, this, [this](QObject* object) {
            if (_searchBar && object == _searchBar->_searchEdit) {
                Q_EMIT focusRequested();
            }
        });
    }
}

bool FindBarAdapter::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::ShowToParent:
    case QEvent::HideToParent:
    case QEvent::PaletteChange:
    case QEvent::FontChange:
    case QEvent::EnabledChange:
    case QEvent::ToolTipChange:
    case QEvent::StyleChange:
    case QEvent::ActionChanged:
        Q_EMIT changed();
        break;
    case QEvent::ParentChange:
        connectWindow();
        break;
    default:
        break;
    }
    if (watched == _searchBar && event->type() == QEvent::Show) {
        // setVisible(true) focuses the line edit, which is already the window's focus
        // object when it was before.
        Q_EMIT focusRequested();
    }
    return WidgetMirror::eventFilter(watched, event);
}

QColor FindBarAdapter::backgroundColor() const
{
    return _searchBar ? CompatWindows::palette(_searchBar).color(_searchBar->backgroundRole()) : QColor();
}

QColor FindBarAdapter::textColor() const
{
    return _searchBar ? CompatWindows::palette(_searchBar).color(QPalette::WindowText) : QColor();
}

QRect FindBarAdapter::gripGeometry() const
{
    const auto grip = _searchBar ? _searchBar->findChild<QSizeGrip*>() : nullptr;
    return grip ? grip->geometry() : QRect();
}

QRect FindBarAdapter::editGeometry() const
{
    return _searchBar ? _searchBar->_searchEdit->geometry() : QRect();
}

QRect FindBarAdapter::textGeometry() const
{
    if (!_searchBar) {
        return {};
    }
    // QLineEdit::initStyleOption() and paintEvent(); metrics are the line edit's own.
    const QLineEdit* edit = _searchBar->_searchEdit;
    QStyle* style = edit->style();
    QStyleOptionFrame panel;
    panel.initFrom(edit);
    panel.rect = edit->contentsRect();
    panel.lineWidth = editFrameWidth();
    panel.midLineWidth = 0;
    panel.state |= QStyle::State_Sunken;
    QMargins margins = edit->textMargins();
    if (clearButtonShown()) {
        // QLineEditPrivate::sideWidgetParameters(); a fading-out button gets no space.
        margins.setRight(margins.right() + style->pixelMetric(QStyle::PM_LineEditIconMargin, nullptr, nullptr)
            + style->pixelMetric(QStyle::PM_LineEditIconSize, nullptr, nullptr) + 6);
    }
    const QRect contents = style->subElementRect(QStyle::SE_LineEditContents, &panel, edit).marginsRemoved(margins);
    const QFontMetrics metrics = edit->fontMetrics();
    const QString shown = edit->text().isEmpty() ? edit->placeholderText() : edit->text();
    const int height = qMax(metrics.boundingRect(shown).height(), metrics.height());
    return {contents.x() + LineEditHorizontalMargin, contents.y() + (contents.height() - height + 1) / 2, contents.width() - 2 * LineEditHorizontalMargin, height};
}

int FindBarAdapter::editFrameWidth() const
{
    if (!_searchBar || !_searchBar->_searchEdit->hasFrame()) {
        return 0;
    }
    // Styles answer this metric per widget type (Silver: 0 without a widget).
    const QLineEdit* edit = _searchBar->_searchEdit;
    QStyleOptionFrame panel;
    panel.initFrom(edit);
    return edit->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &panel, edit);
}

QString FindBarAdapter::text() const
{
    return _searchBar ? _searchBar->_searchEdit->text() : QString();
}

QString FindBarAdapter::placeholderText() const
{
    return _searchBar ? _searchBar->_searchEdit->placeholderText() : QString();
}

QString FindBarAdapter::editToolTip() const
{
    return _searchBar ? _searchBar->_searchEdit->toolTip() : QString();
}

QFont FindBarAdapter::editFont() const
{
    return _searchBar ? _searchBar->_searchEdit->font() : QFont();
}

QColor FindBarAdapter::editTextColor() const
{
    return _searchBar ? CompatWindows::palette(_searchBar->_searchEdit).color(QPalette::Text) : QColor();
}

QPalette FindBarAdapter::editPalette() const
{
    if (!_searchBar) {
        return {};
    }
    QPalette palette = CompatWindows::palette(_searchBar->_searchEdit);
    if (hasMatchColor()) {
        palette.setColor(QPalette::Base, matchColor());
    }
    return palette;
}

QColor FindBarAdapter::placeholderColor() const
{
    return _searchBar ? CompatWindows::palette(_searchBar->_searchEdit).color(QPalette::PlaceholderText) : QColor();
}

QColor FindBarAdapter::selectionColor() const
{
    return _searchBar ? CompatWindows::palette(_searchBar->_searchEdit).color(QPalette::Highlight) : QColor();
}

QColor FindBarAdapter::selectedTextColor() const
{
    return _searchBar ? CompatWindows::palette(_searchBar->_searchEdit).color(QPalette::HighlightedText) : QColor();
}

QColor FindBarAdapter::matchColor() const
{
    if (!_searchBar) {
        return {};
    }
    // The colour setFoundMatch() puts into the line edit's style sheet.
    static const QRegularExpression background(QStringLiteral("background-color:\\s*(#[0-9a-fA-F]{6})"));
    const QRegularExpressionMatch match = background.match(_searchBar->_searchEdit->styleSheet());
    return match.hasMatch() ? QColor(match.captured(1)) : QColor();
}

bool FindBarAdapter::hasMatchColor() const
{
    return matchColor().isValid();
}

QToolButton* FindBarAdapter::clearButton() const
{
    // QLineEdit's clear button is its only tool button (a QLineEditIconButton).
    return _searchBar ? _searchBar->_searchEdit->findChild<QToolButton*>() : nullptr;
}

QRect FindBarAdapter::clearButtonGeometry() const
{
    const QToolButton* button = clearButton();
    return button ? button->geometry() : QRect();
}

QImage FindBarAdapter::clearButtonImage() const
{
    const QToolButton* button = clearButton();
    if (!button) {
        return {};
    }
    const int size = button->style()->pixelMetric(QStyle::PM_LineEditIconSize, nullptr, nullptr);
    const qreal ratio = _view && _view->window() ? _view->window()->effectiveDevicePixelRatio() : qApp->devicePixelRatio();
    return button->icon().pixmap(QSize(size, size), ratio, button->isEnabled() ? QIcon::Normal : QIcon::Disabled).toImage();
}

bool FindBarAdapter::clearButtonShown() const
{
    // The button fades in with text and out without (QLineEditPrivate::textChanged()).
    const QToolButton* button = clearButton();
    return button && button->isEnabled() && !text().isEmpty();
}

QList<QToolButton*> FindBarAdapter::shownButtons() const
{
    QList<QToolButton*> shown;
    if (_searchBar) {
        for (QToolButton* button : _searchBar->findChildren<QToolButton*>(Qt::FindDirectChildrenOnly)) {
            if (button->isVisibleTo(_searchBar)) {
                shown.append(button);
            }
        }
        std::sort(shown.begin(), shown.end(), [](const QToolButton* a, const QToolButton* b) {
            return a->x() < b->x();
        });
    }
    return shown;
}

QVariantList FindBarAdapter::buttons() const
{
    QVariantList buttons;
    for (const QToolButton* button : shownButtons()) {
        buttons.append(QVariantMap{{QStringLiteral("geometry"), button->geometry()},
            {QStringLiteral("iconName"), button->icon().name()},
            {QStringLiteral("iconWidth"), button->iconSize().width()},
            {QStringLiteral("iconHeight"), button->iconSize().height()},
            {QStringLiteral("enabled"), button->isEnabled()},
            {QStringLiteral("checked"), button->isChecked()},
            {QStringLiteral("toolTip"), button->toolTip()},
            {QStringLiteral("menu"), button->menu() != nullptr}});
    }
    return buttons;
}

void FindBarAdapter::setText(const QString& text)
{
    if (_searchBar && text != _searchBar->_searchEdit->text()) {
        _searchBar->_searchEdit->setText(text);
    }
}

void FindBarAdapter::clear()
{
    if (_searchBar) {
        _searchBar->_searchEdit->clear();
    }
}

void FindBarAdapter::clickButton(int index, QPointF menuPosition)
{
    const QList<QToolButton*> shown = shownButtons();
    if (index < 0 || index >= shown.size() || !shown.at(index)->isEnabled()) {
        return;
    }
    QToolButton* button = shown.at(index);
    if (button->menu() && button->popupMode() == QToolButton::InstantPopup) {
        // The hidden button would open its menu at its own (off-screen) position.
        button->menu()->popup(menuPosition.toPoint());
    } else {
        button->click();
    }
}

bool FindBarAdapter::key(int key, int modifiers, const QString& text)
{
    if (!_searchBar) {
        return false;
    }
    const auto keyModifiers = Qt::KeyboardModifiers(modifiers);
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        // IncrementalSearchBar::eventFilter() for its line edit.
        if (keyModifiers == Qt::NoModifier) {
            _searchBar->_findNextButton->click();
            return true;
        }
        if (keyModifiers == Qt::ShiftModifier) {
            _searchBar->_findPreviousButton->click();
            return true;
        }
        if (keyModifiers == Qt::ControlModifier) {
            _searchBar->_searchFromButton->click();
            return true;
        }
        return false;
    }
    // The line edit leaves these to the bar's keyPressEvent().
    static const QSet<int> movement{Qt::Key_PageUp, Qt::Key_PageDown, Qt::Key_Up, Qt::Key_Down};
    if (key == Qt::Key_Escape || (movement.contains(key) && keyModifiers == Qt::ShiftModifier)) {
        QKeyEvent event(QEvent::KeyPress, key, keyModifiers, text);
        QCoreApplication::sendEvent(_searchBar, &event);
        return true;
    }
    return false;
}

void FindBarAdapter::gripPress(QPointF globalPosition)
{
    // QSizeGrip::mousePressEvent() for a grip in a sub-window, inside its parent.
    const auto grip = _searchBar ? _searchBar->findChild<QSizeGrip*>() : nullptr;
    if (!grip || !_searchBar->parentWidget()) {
        return;
    }
    _gripPressed = true;
    _gripStart = globalPosition.toPoint();
    _gripGeometry = _searchBar->geometry();
    const QRect available = _searchBar->parentWidget()->contentsRect();
    const QPoint gripPosition = grip->mapTo(_searchBar, QPoint(0, 0));
    const bool atBottom = gripPosition.y() >= _searchBar->height() / 2;
    const bool atLeft = gripPosition.x() <= _searchBar->width() / 2;
    _gripMaxDy = atBottom ? available.bottom() - _gripGeometry.bottom() : available.y() - _gripGeometry.y();
    _gripMaxDx = atLeft ? available.x() - _gripGeometry.x() : available.right() - _gripGeometry.right();
}

void FindBarAdapter::gripMove(QPointF globalPosition, int buttons)
{
    const auto grip = _searchBar ? _searchBar->findChild<QSizeGrip*>() : nullptr;
    if (!grip || !_gripPressed || buttons != Qt::LeftButton) {
        return;
    }
    // QSizeGrip::mouseMoveEvent().
    const QPoint position = globalPosition.toPoint();
    const QPoint gripPosition = grip->mapTo(_searchBar, QPoint(0, 0));
    const bool atBottom = gripPosition.y() >= _gripGeometry.height() / 2;
    const bool atLeft = gripPosition.x() <= _gripGeometry.width() / 2;
    QSize size;
    size.rheight() = atBottom ? _gripGeometry.height() + qMin(position.y() - _gripStart.y(), _gripMaxDy)
                              : _gripGeometry.height() - qMax(position.y() - _gripStart.y(), _gripMaxDy);
    size.rwidth() = atLeft ? _gripGeometry.width() - qMax(position.x() - _gripStart.x(), _gripMaxDx)
                           : _gripGeometry.width() + qMin(position.x() - _gripStart.x(), _gripMaxDx);
    size = QLayout::closestAcceptableSize(_searchBar, size);
    QRect geometry(QPoint(), size);
    if (atBottom) {
        atLeft ? geometry.moveTopRight(_gripGeometry.topRight()) : geometry.moveTopLeft(_gripGeometry.topLeft());
    } else {
        atLeft ? geometry.moveBottomRight(_gripGeometry.bottomRight()) : geometry.moveBottomLeft(_gripGeometry.bottomLeft());
    }
    _searchBar->setGeometry(geometry);
}

void FindBarAdapter::gripRelease()
{
    _gripPressed = false;
}
}

#include "moc_FindBarAdapter.cpp"
