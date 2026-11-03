/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// How a dialog lays out what a user sees, as facts comparable between a widget dialog and the
// QML dialog replacing it (DialogLayoutTest): whether a label's text ends just before the
// control to its right, whether a control is stretched wider than its size hint, and how much
// of the page a word-wrapped label spans. Keys are "page|label text" and "page|object name"
// (the QML dialogs keep the .ui's object names); a dialog without pages has an empty page.

#include "DialogContent.h"

#include <KPageDialog>
#include <KPageView>
#include <KPageWidgetModel>

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QHash>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRadioButton>
#include <QSlider>
#include <QStyle>
#include <QTabWidget>
#include <QTest>
#include <QTextEdit>

#include <algorithm>

namespace DialogLayout {
// A label's text ending this close to the control on its right reads as that control's label
// (form layouts put a few pixels between them; a left-aligned label in a wide column doesn't).
constexpr int labelGap = 24;
// Wider than this beyond its size hint, a control was stretched by its layout.
constexpr int stretchSlack = 4;
// A page's layout margin is at most this.
constexpr int edgeSlack = 16;

struct Facts {
    QHash<QString, bool> labelBesideControl;
    QHash<QString, bool> stretched;
    QHash<QString, qreal> wrappedShare;
};

struct Placed {
    QRectF rect;
    bool isLabel = false;
    QString key;
    bool wrapped = false;
    // A control's object name (empty when it isn't compared) and whether its layout made it
    // wider than its size hint.
    QString name;
    bool grown = false;
};

inline void addFacts(Facts& facts, const QList<Placed>& placed, qreal pageWidth, const QString& page)
{
    // A control spanning to the page's right edge (inside its layout margin) is stretched
    // however it got there: the widest control sets the dialog's width and so sits at its own
    // size hint.
    for (const Placed& control : placed) {
        if (!control.isLabel && !control.name.isEmpty()) {
            facts.stretched.insert(page + u'|' + control.name, control.grown || control.rect.right() >= pageWidth - edgeSlack);
        }
    }
    for (const Placed& label : placed) {
        if (!label.isLabel) {
            continue;
        }
        const QString key = page + u'|' + label.key;
        if (label.wrapped) {
            if (pageWidth > 0) {
                facts.wrappedShare.insert(key, label.rect.width() / pageWidth);
            }
            continue;
        }
        // The nearest control to the right whose height covers the label's middle.
        const qreal middle = label.rect.center().y();
        qreal nearest = -1;
        for (const Placed& control : placed) {
            if (control.isLabel || control.rect.top() > middle || control.rect.bottom() < middle || control.rect.left() < label.rect.right() - 2) {
                continue;
            }
            if (nearest < 0 || control.rect.left() < nearest) {
                nearest = control.rect.left();
            }
        }
        if (nearest >= 0) {
            facts.labelBesideControl.insert(key, nearest - label.rect.right() <= labelGap);
        }
    }
}

inline bool isWidgetControl(const QWidget* widget)
{
    return qobject_cast<const QAbstractButton*>(widget) || qobject_cast<const QComboBox*>(widget) || qobject_cast<const QAbstractSpinBox*>(widget)
        || qobject_cast<const QLineEdit*>(widget) || qobject_cast<const QSlider*>(widget) || qobject_cast<const QKeySequenceEdit*>(widget)
        || qobject_cast<const QAbstractItemView*>(widget) || qobject_cast<const QPlainTextEdit*>(widget) || qobject_cast<const QTextEdit*>(widget);
}

// Controls whose drawn width is their width. A check box's or radio button's extra width is
// empty space; a view's width isn't comparable with a QML view's implicit width.
inline bool isWidgetStretchable(const QWidget* widget)
{
    if (qobject_cast<const QCheckBox*>(widget) || qobject_cast<const QRadioButton*>(widget)) {
        return false;
    }
    return qobject_cast<const QAbstractButton*>(widget) || qobject_cast<const QComboBox*>(widget) || qobject_cast<const QAbstractSpinBox*>(widget)
        || qobject_cast<const QLineEdit*>(widget) || qobject_cast<const QSlider*>(widget) || qobject_cast<const QKeySequenceEdit*>(widget);
}

// A control's own parts (a combo box's line edit, a spin box's buttons) aren't controls.
inline bool insideWidgetControl(const QWidget* widget, const QWidget* page)
{
    for (const QWidget* parent = widget->parentWidget(); parent && parent != page; parent = parent->parentWidget()) {
        if (isWidgetControl(parent)) {
            return true;
        }
    }
    return false;
}

// The widgets shown on the page now (other pages and tabs are hidden).
inline void fromWidgets(QWidget* page, Facts& facts, const QString& pageName)
{
    QList<Placed> placed;
    const QList<QWidget*> widgets = page->findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (!widget->isVisibleTo(page) || insideWidgetControl(widget, page)) {
            continue;
        }
        const QPointF origin = widget->mapTo(page, QPoint(0, 0));
        if (auto* label = qobject_cast<QLabel*>(widget)) {
            const QString key = DialogContent::clean(label->text());
            if (key.isEmpty()) {
                continue;
            }
            // Where the text is drawn: its width aligned in the label as the label aligns it.
            const QRect contents = label->contentsRect();
            const int textWidth = label->wordWrap() ? contents.width() : std::min(contents.width(), label->sizeHint().width() - 2 * label->margin());
            const QRect text = QStyle::alignedRect(label->layoutDirection(),
                label->alignment() & Qt::AlignHorizontal_Mask,
                QSize(textWidth, contents.height()),
                contents);
            placed.append({QRectF(text).translated(origin), true, key, label->wordWrap()});
        } else if (isWidgetControl(widget)) {
            const bool compared = isWidgetStretchable(widget) && !widget->objectName().isEmpty() && !widget->objectName().startsWith(QLatin1String("qt_"));
            // A size policy without GrowFlag (Fixed, Maximum) is never stretched by its layout.
            const bool canGrow = widget->sizePolicy().horizontalPolicy() & QSizePolicy::GrowFlag;
            placed.append({QRectF(origin, widget->size()),
                false,
                QString(),
                false,
                compared ? widget->objectName() : QString(),
                canGrow && widget->width() > widget->sizeHint().width() + stretchSlack});
        }
    }
    addFacts(facts, placed, page->width(), pageName);
}

// TextField and TextArea aren't QQuickControls.
inline bool isQmlControl(const QQuickItem* item)
{
    return item->inherits("QQuickControl") || item->inherits("QQuickFlickable") || item->inherits("QQuickTextInput") || item->inherits("QQuickTextEdit");
}

// As isWidgetStretchable().
inline bool isQmlStretchable(const QQuickItem* item)
{
    if (item->inherits("QQuickCheckBox") || item->inherits("QQuickRadioButton") || item->inherits("QQuickSwitch") || item->inherits("QQuickTabButton")
        || item->inherits("QQuickItemDelegate")) {
        return false;
    }
    return item->inherits("QQuickAbstractButton") || item->inherits("QQuickComboBox") || item->inherits("QQuickSpinBox")
        || item->inherits("QQuickTextInput") || item->inherits("QQuickSlider");
}

// Positions in the page's coordinates, as the widget side's.
inline void placeQml(QQuickItem* item, QQuickItem* page, QList<Placed>& placed)
{
    if (!item->isVisible() || item->opacity() <= 0) {
        return;
    }
    if (isQmlControl(item)) {
        const bool compared = isQmlStretchable(item) && !item->objectName().isEmpty();
        // The layout's size hint: Layout.preferredWidth when set, else the implicit width.
        const QQmlProperty preferred(item, QStringLiteral("Layout.preferredWidth"), qmlContext(item));
        const qreal hint = preferred.isValid() && preferred.read().toReal() >= 0 ? preferred.read().toReal() : item->implicitWidth();
        placed.append({QRectF(item->mapToItem(page, QPointF(0, 0)), item->size()),
            false,
            QString(),
            false,
            compared ? item->objectName() : QString(),
            item->width() > hint + stretchSlack});
        return;
    }
    if (item->inherits("QQuickText")) {
        const QString key = DialogContent::clean(item->property("text").toString());
        if (!key.isEmpty()) {
            const bool wrapped = item->property("wrapMode").toInt() != 0;
            const qreal width = wrapped ? item->width() : std::min(item->width(), item->property("contentWidth").toReal());
            // QQuickText's effective horizontal alignment: 1 left, 2 right, 4 centre.
            const int alignment = item->property("effectiveHorizontalAlignment").toInt();
            const qreal left = alignment == 2 ? item->width() - width : alignment == 4 ? (item->width() - width) / 2
                                                                                       : 0;
            placed.append({QRectF(item->mapToItem(page, QPointF(left, 0)), QSizeF(width, item->height())), true, key, wrapped});
        }
        return;
    }
    const QList<QQuickItem*> children = item->childItems();
    for (QQuickItem* child : children) {
        placeQml(child, page, placed);
    }
}

// The items shown on the page now.
inline void fromQml(QQuickItem* page, Facts& facts, const QString& pageName)
{
    QList<Placed> placed;
    placeQml(page, page, placed);
    addFacts(facts, placed, page->width(), pageName);
}

// Qt Quick lays out on the next polish.
inline void settle()
{
    QTest::qWait(100);
}

// Each page of a widget page dialog current in turn, and each tab of its tab widgets; a
// dialog without pages as a whole.
inline Facts fromWidgetDialog(QWidget* dialog)
{
    Facts facts;
    auto* pageDialog = qobject_cast<KPageDialog*>(dialog);
    auto* view = pageDialog ? pageDialog->findChild<KPageView*>() : nullptr;
    auto* model = view ? qobject_cast<KPageWidgetModel*>(view->model()) : nullptr;
    if (!model) {
        fromWidgets(dialog, facts, QString());
        return facts;
    }
    for (int row = 0; row < model->rowCount(); ++row) {
        KPageWidgetItem* page = model->item(model->index(row, 0));
        const QString name = DialogContent::clean(page->name());
        pageDialog->setCurrentPage(page);
        settle();
        fromWidgets(page->widget(), facts, name);
        const QList<QTabWidget*> tabWidgets = page->widget()->findChildren<QTabWidget*>();
        for (QTabWidget* tabs : tabWidgets) {
            for (int i = 0; i < tabs->count(); ++i) {
                tabs->setCurrentIndex(i);
                settle();
                fromWidgets(page->widget(), facts, name);
            }
        }
    }
    return facts;
}

inline QList<QQuickItem*> qmlTabBars(QQuickItem* item)
{
    QList<QQuickItem*> bars;
    if (item->inherits("QQuickTabBar")) {
        bars.append(item);
    }
    const QList<QQuickItem*> children = item->childItems();
    for (QQuickItem* child : children) {
        bars += qmlTabBars(child);
    }
    return bars;
}

// Each page of a QML page dialog current in turn (as DialogContent::fromQmlPages()), and each
// tab of its tab bars; a dialog without pages as a whole.
inline Facts fromQmlDialog(QQuickWindow* dialog)
{
    Facts facts;
    QList<QQuickItem*> pages;
    const QList<QQuickItem*> items = dialog->findChildren<QQuickItem*>();
    for (QQuickItem* item : items) {
        if (DialogContent::isPage(item)) {
            pages.append(item);
        }
    }
    if (pages.isEmpty()) {
        settle();
        fromQml(dialog->contentItem(), facts, QString());
        return facts;
    }
    for (int i = 0; i < pages.size(); ++i) {
        dialog->setProperty("currentIndex", i);
        settle();
        // The current page is the one shown (TabbedPageDialog names no current page).
        const auto shown = std::find_if(pages.cbegin(), pages.cend(), [](QQuickItem* page) {
            return page->isVisible();
        });
        if (shown == pages.cend()) {
            continue;
        }
        QQuickItem* page = *shown;
        const QString name = DialogContent::clean(page->property("title").toString());
        fromQml(page, facts, name);
        const QList<QQuickItem*> bars = qmlTabBars(page);
        for (QQuickItem* bar : bars) {
            const int count = bar->property("count").toInt();
            for (int tab = 0; tab < count; ++tab) {
                bar->setProperty("currentIndex", tab);
                settle();
                fromQml(page, facts, name);
            }
        }
    }
    return facts;
}

// Where the QML dialog's layout differs from the widget dialog's, as messages.
// besideControlPages: pages whose labels all sit just before their control by decision;
// movedControls: controls those decisions moved, whose width isn't compared.
inline QStringList differences(const Facts& widget, const Facts& qml, const QStringList& besideControlPages, const QStringList& movedControls = {})
{
    QStringList result;
    for (auto it = qml.labelBesideControl.cbegin(); it != qml.labelBesideControl.cend(); ++it) {
        const QString page = it.key().section(u'|', 0, 0);
        const bool intended = besideControlPages.contains(page);
        if (!intended && !widget.labelBesideControl.contains(it.key())) {
            continue;
        }
        const bool expected = intended || widget.labelBesideControl.value(it.key());
        if (it.value() != expected) {
            result.append(QStringLiteral("label %1: %2 its control in QML, %3 %4")
                    .arg(it.key(),
                        it.value() ? QStringLiteral("just before") : QStringLiteral("away from"),
                        expected ? QStringLiteral("just before") : QStringLiteral("away from"),
                        intended ? QStringLiteral("by decision") : QStringLiteral("in the original")));
        }
    }
    for (auto it = widget.stretched.cbegin(); it != widget.stretched.cend(); ++it) {
        if (movedControls.contains(it.key())) {
            continue;
        }
        if (qml.stretched.contains(it.key()) && qml.stretched.value(it.key()) != it.value()) {
            result.append(QStringLiteral("control %1: %2 in QML, %3 in the original")
                    .arg(it.key(),
                        qml.stretched.value(it.key()) ? QStringLiteral("stretched") : QStringLiteral("at its size hint"),
                        it.value() ? QStringLiteral("stretched") : QStringLiteral("at its size hint")));
        }
    }
    // A word-wrapped label spans as much of its page as the original's (a tenth less at most).
    for (auto it = widget.wrappedShare.cbegin(); it != widget.wrappedShare.cend(); ++it) {
        if (qml.wrappedShare.contains(it.key()) && qml.wrappedShare.value(it.key()) < it.value() - 0.1) {
            result.append(QStringLiteral("wrapped label %1: %2% of the page in QML, %3% in the original")
                    .arg(it.key())
                    .arg(qRound(qml.wrappedShare.value(it.key()) * 100))
                    .arg(qRound(it.value() * 100)));
        }
    }
    result.sort();
    return result;
}
}
