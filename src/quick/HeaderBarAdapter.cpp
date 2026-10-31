/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "HeaderBarAdapter.h"

#include "CompatWindows.h"
#include "TerminalView.h"
#include "ViewProperties.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "widgets/TerminalHeaderBar.h"

#include <QApplication>
#include <QDrag>
#include <QEvent>
#include <QLabel>
#include <QMimeData>
#include <QStyle>
#include <QToolButton>

namespace Konsole {
HeaderBarAdapter::HeaderBarAdapter(QObject* parent)
    : WidgetMirror(parent)
{
    connect(CompatWindowsNotifier::instance(), &CompatWindowsNotifier::activeChanged, this, &HeaderBarAdapter::changed);
}

TerminalView* HeaderBarAdapter::view() const
{
    return _view;
}

void HeaderBarAdapter::setView(TerminalView* view)
{
    if (_view == view) {
        return;
    }
    if (_headerBar) {
        _headerBar->m_toggleExpandedMode->disconnect(this);
        if (_view && _view->display()) {
            _view->display()->disconnect(this);
        }
        _headerBar->removeEventFilter(this);
        for (QWidget* child : _headerBar->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
            child->removeEventFilter(this);
        }
    }
    if (_view && _view->sessionController()) {
        _view->sessionController()->disconnect(this);
    }
    _view = view;
    _headerBar = view && view->display() ? view->display()->headerBar() : nullptr;
    mirror(_headerBar, view ? view->display() : nullptr);
    if (_headerBar) {
        // Layout, visibility, palette and tooltip changes all arrive as events.
        _headerBar->installEventFilter(this);
        for (QWidget* child : _headerBar->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
            child->installEventFilter(this);
        }
        connect(_headerBar->m_toggleExpandedMode, &QAbstractButton::toggled, this, &HeaderBarAdapter::changed);
        // Queued: the signal comes before the header bar's setFocusIndicatorState().
        connect(view->display(), &TerminalDisplay::compositeFocusChanged, this, &HeaderBarAdapter::changed, Qt::QueuedConnection);
    }
    if (ViewProperties* properties = view ? view->sessionController() : nullptr) {
        // The header bar's own connections come first, so its labels are already updated.
        connect(properties, &ViewProperties::titleChanged, this, &HeaderBarAdapter::changed);
        connect(properties, &ViewProperties::iconChanged, this, &HeaderBarAdapter::changed);
        connect(properties, &ViewProperties::notificationChanged, this, &HeaderBarAdapter::changed);
        connect(properties, &ViewProperties::readOnlyChanged, this, &HeaderBarAdapter::changed);
        connect(properties, &ViewProperties::copyInputChanged, this, &HeaderBarAdapter::changed);
    }
    Q_EMIT viewChanged();
    Q_EMIT changed();
}

bool HeaderBarAdapter::eventFilter(QObject* watched, QEvent* event)
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
    case QEvent::UpdateRequest:
        Q_EMIT changed();
        break;
    default:
        break;
    }
    return WidgetMirror::eventFilter(watched, event);
}

QRect HeaderBarAdapter::childGeometry(const QWidget* child) const
{
    return child->geometry();
}

QColor HeaderBarAdapter::backgroundColor() const
{
    return _headerBar ? CompatWindows::palette(_headerBar).color(_headerBar->backgroundRole()) : QColor();
}

QColor HeaderBarAdapter::textColor() const
{
    return _headerBar ? CompatWindows::palette(_headerBar).color(QPalette::WindowText) : QColor();
}

QPalette HeaderBarAdapter::palette() const
{
    return _headerBar ? CompatWindows::palette(_headerBar) : QPalette();
}

bool HeaderBarAdapter::focused() const
{
    return _headerBar && _headerBar->m_terminalIsFocused;
}

QColor HeaderBarAdapter::shadowColor() const
{
    // TerminalHeaderBar::paintEvent()'s unfocused shade.
    QColor color = qApp->palette().color(QPalette::Shadow);
    color.setAlphaF(qreal(0.2) * color.alphaF());
    return color;
}

QVariantList HeaderBarAdapter::pixmaps() const
{
    QVariantList pixmaps;
    if (!_headerBar) {
        return pixmaps;
    }
    const QLabel* labels[] = {_headerBar->m_terminalIcon,
        _headerBar->m_statusIconReadOnly,
        _headerBar->m_statusIconCopyInput,
        _headerBar->m_statusIconSilence,
        _headerBar->m_statusIconActivity,
        _headerBar->m_statusIconBell};
    for (const QLabel* label : labels) {
        if (label->isVisibleTo(_headerBar)) {
            pixmaps.append(QVariantMap{{QStringLiteral("geometry"), childGeometry(label)},
                {QStringLiteral("image"), label->pixmap().toImage()},
                {QStringLiteral("alignment"), int(label->alignment())}});
        }
    }
    return pixmaps;
}

QRect HeaderBarAdapter::titleGeometry() const
{
    return _headerBar ? childGeometry(_headerBar->m_terminalTitle) : QRect();
}

QString HeaderBarAdapter::title() const
{
    return _headerBar ? _headerBar->m_terminalTitle->text() : QString();
}

QFont HeaderBarAdapter::titleFont() const
{
    return _headerBar ? _headerBar->m_terminalTitle->font() : QFont();
}

QColor HeaderBarAdapter::titleColor() const
{
    return _headerBar ? CompatWindows::palette(_headerBar->m_terminalTitle).color(_headerBar->m_terminalTitle->foregroundRole()) : QColor();
}

QList<QAbstractButton*> HeaderBarAdapter::shownButtons() const
{
    QList<QAbstractButton*> shown;
    if (_headerBar) {
        for (QToolButton* button : {_headerBar->m_toggleExpandedMode, _headerBar->m_moveToNewTab, _headerBar->m_closeBtn}) {
            if (button->isVisibleTo(_headerBar)) {
                shown.append(button);
            }
        }
    }
    return shown;
}

QVariantList HeaderBarAdapter::buttons() const
{
    QVariantList buttons;
    for (const QAbstractButton* button : shownButtons()) {
        buttons.append(QVariantMap{{QStringLiteral("geometry"), childGeometry(button)},
            {QStringLiteral("iconName"), button->icon().name()},
            {QStringLiteral("iconWidth"), button->iconSize().width()},
            {QStringLiteral("iconHeight"), button->iconSize().height()},
            {QStringLiteral("enabled"), button->isEnabled()},
            {QStringLiteral("checked"), button->isChecked()},
            {QStringLiteral("toolTip"), button->toolTip()}});
    }
    return buttons;
}

int HeaderBarAdapter::toolTipDelay() const
{
    return QApplication::style()->styleHint(QStyle::SH_ToolTip_WakeUpDelay);
}

void HeaderBarAdapter::clickButton(int index)
{
    const QList<QAbstractButton*> shown = shownButtons();
    if (index >= 0 && index < shown.size() && shown.at(index)->isEnabled()) {
        shown.at(index)->click();
    }
}

void HeaderBarAdapter::press(QPointF position)
{
    if (_headerBar) {
        _headerBar->m_startDrag = position.toPoint();
    }
}

void HeaderBarAdapter::move(QPointF position)
{
    // TerminalHeaderBar::mouseMoveEvent().
    if (!_headerBar || _headerBar->m_toggleExpandedMode->isChecked()) {
        return;
    }
    if ((position.toPoint() - _headerBar->m_startDrag).manhattanLength() > 10) {
        auto drag = new QDrag(_headerBar->parent());
        auto mimeData = new QMimeData();
        QByteArray payload;
        payload.setNum(qApp->applicationPid());
        mimeData->setData(QStringLiteral("konsole/terminal_display"), payload);
        drag->setMimeData(mimeData);
        drag->exec();
    }
}

void HeaderBarAdapter::doubleClick(int button)
{
    if (_headerBar && button == Qt::LeftButton) {
        _headerBar->m_toggleExpandedMode->click();
    }
}
}

#include "moc_HeaderBarAdapter.cpp"
