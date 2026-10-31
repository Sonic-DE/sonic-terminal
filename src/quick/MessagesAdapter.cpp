/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "MessagesAdapter.h"

#include "CompatWindows.h"
#include "TerminalView.h"
#include "WidgetMirror.h"
#include "terminalDisplay/TerminalDisplay.h"

#include <KMessageWidget>

#include <QEvent>
#include <QLabel>
#include <QTimeLine>
#include <QToolButton>

namespace Konsole {
namespace {
    // KMessageWidget's two labels: KMessageWidget::text() is its text label's text.
    QLabel* textLabel(const KMessageWidget* widget)
    {
        const QList<QLabel*> labels = widget->findChildren<QLabel*>(Qt::FindDirectChildrenOnly);
        for (QLabel* label : labels) {
            if (label->text() == widget->text() && label->pixmap().isNull()) {
                return label;
            }
        }
        return nullptr;
    }

    QLabel* iconLabel(const KMessageWidget* widget)
    {
        const QLabel* text = textLabel(widget);
        for (QLabel* label : widget->findChildren<QLabel*>(Qt::FindDirectChildrenOnly)) {
            if (label != text) {
                return label;
            }
        }
        return nullptr;
    }

    QList<QToolButton*> shownButtons(const KMessageWidget* widget)
    {
        QList<QToolButton*> buttons;
        for (QToolButton* button : widget->findChildren<QToolButton*>(Qt::FindDirectChildrenOnly)) {
            if (button->isVisibleTo(widget)) {
                buttons.append(button);
            }
        }
        std::sort(buttons.begin(), buttons.end(), [](const QToolButton* a, const QToolButton* b) {
            return a->x() < b->x();
        });
        return buttons;
    }
}

MessagesAdapter::MessagesAdapter(QObject* parent)
    : QObject(parent)
{
    connect(CompatWindowsNotifier::instance(), &CompatWindowsNotifier::activeChanged, this, &MessagesAdapter::changed);
    connect(StackingNotifier::instance(), &StackingNotifier::changed, this, [this](QWidget* facade) {
        if (_view && facade == _view->display()) {
            Q_EMIT changed();
        }
    });
}

TerminalView* MessagesAdapter::view() const
{
    return _view;
}

void MessagesAdapter::setView(TerminalView* view)
{
    if (_view == view) {
        return;
    }
    if (_view && _view->display()) {
        _view->display()->removeEventFilter(this);
    }
    _view = view;
    if (_view && _view->display()) {
        // The messages are created when first needed.
        _view->display()->installEventFilter(this);
        for (KMessageWidget* widget : widgets()) {
            watch(widget);
        }
    }
    Q_EMIT viewChanged();
    Q_EMIT changed();
}

QList<KMessageWidget*> MessagesAdapter::widgets() const
{
    if (!_view || !_view->display()) {
        return {};
    }
    QList<KMessageWidget*> widgets = _view->display()->findChildren<KMessageWidget*>(Qt::FindDirectChildrenOnly);
    std::sort(widgets.begin(), widgets.end(), [](const KMessageWidget* a, const KMessageWidget* b) {
        return a->y() < b->y();
    });
    return widgets;
}

void MessagesAdapter::watch(KMessageWidget* widget)
{
    if (_watched.contains(widget)) {
        return;
    }
    _watched.append(widget);
    widget->installEventFilter(this);
    for (QWidget* child : widget->findChildren<QWidget*>()) {
        child->installEventFilter(this);
    }
    if (QTimeLine* timeLine = widget->findChild<QTimeLine*>()) {
        connect(timeLine, &QTimeLine::valueChanged, this, &MessagesAdapter::changed);
        connect(timeLine, &QTimeLine::finished, this, &MessagesAdapter::changed);
    }
}

bool MessagesAdapter::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::ChildPolished:
        if (_view && watched == _view->display()) {
            if (auto* widget = qobject_cast<KMessageWidget*>(static_cast<QChildEvent*>(event)->child())) {
                watch(widget);
                Q_EMIT changed();
            }
        }
        break;
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::ShowToParent:
    case QEvent::HideToParent:
    case QEvent::PaletteChange:
    case QEvent::FontChange:
    case QEvent::ToolTipChange:
    case QEvent::ActionChanged:
    case QEvent::ZOrderChange:
        if (_view && watched != _view->display()) {
            Q_EMIT changed();
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

QVariantList MessagesAdapter::messages() const
{
    QVariantList messages;
    TerminalDisplay* display = _view ? _view->display() : nullptr;
    for (const KMessageWidget* widget : widgets()) {
        const QTimeLine* timeLine = widget->findChild<QTimeLine*>();
        const qreal value = timeLine && timeLine->state() == QTimeLine::Running ? timeLine->currentValue() : 1;
        QVariantMap message{{QStringLiteral("geometry"), QRect(widget->mapTo(display, QPoint(0, 0)), widget->size())},
            {QStringLiteral("shown"), widget->isVisibleTo(display)},
            {QStringLiteral("stackOrder"), WidgetMirror::stackOrderOf(widget)},
            {QStringLiteral("color"), CompatWindows::palette(widget).color(QPalette::Window)},
            {QStringLiteral("parentColor"), CompatWindows::palette(display).color(QPalette::Window)},
            {QStringLiteral("opacity"), value * value}};
        const QLabel* icon = iconLabel(widget);
        const bool iconShown = icon && icon->isVisibleTo(widget);
        message.insert(QStringLiteral("icon"),
            QVariantMap{{QStringLiteral("shown"), iconShown},
                {QStringLiteral("geometry"), iconShown ? icon->geometry() : QRect()},
                {QStringLiteral("image"), iconShown ? icon->pixmap().toImage() : QImage()}});
        if (const QLabel* text = textLabel(widget)) {
            // QLabel's AutoText rule; a label without a frame lays out in its contents.
            const bool rich = text->textFormat() == Qt::RichText || (text->textFormat() == Qt::AutoText && Qt::mightBeRichText(text->text()));
            message.insert(QStringLiteral("text"),
                QVariantMap{{QStringLiteral("geometry"), text->contentsRect().translated(text->pos())},
                    {QStringLiteral("text"), text->text()},
                    {QStringLiteral("rich"), rich},
                    {QStringLiteral("wordWrap"), text->wordWrap()},
                    {QStringLiteral("color"), CompatWindows::palette(text).color(text->foregroundRole())},
                    {QStringLiteral("linkColor"), CompatWindows::palette(text).color(QPalette::Link)},
                    {QStringLiteral("font"), text->font()}});
        }
        QVariantList buttons;
        for (const QToolButton* button : shownButtons(widget)) {
            buttons.append(QVariantMap{{QStringLiteral("geometry"), button->geometry()},
                {QStringLiteral("iconName"), button->icon().name()},
                {QStringLiteral("iconColor"), CompatWindows::palette(button).color(QPalette::WindowText)},
                {QStringLiteral("iconWidth"), button->iconSize().width()},
                {QStringLiteral("iconHeight"), button->iconSize().height()},
                {QStringLiteral("autoRaise"), button->autoRaise()},
                {QStringLiteral("toolTip"), button->toolTip()}});
        }
        message.insert(QStringLiteral("buttons"), buttons);
        messages.append(message);
    }
    return messages;
}

void MessagesAdapter::clickButton(int message, int button)
{
    const QList<KMessageWidget*> all = widgets();
    if (message < 0 || message >= all.size()) {
        return;
    }
    const QList<QToolButton*> buttons = shownButtons(all.at(message));
    if (button >= 0 && button < buttons.size()) {
        buttons.at(button)->click();
    }
}

void MessagesAdapter::activateLink(int message, const QString& link)
{
    const QList<KMessageWidget*> all = widgets();
    if (message >= 0 && message < all.size()) {
        Q_EMIT all.at(message)->linkActivated(link);
    }
}

void MessagesAdapter::hoverLink(int message, const QString& link)
{
    const QList<KMessageWidget*> all = widgets();
    if (message >= 0 && message < all.size()) {
        Q_EMIT all.at(message)->linkHovered(link);
    }
}
}

#include "moc_MessagesAdapter.cpp"
