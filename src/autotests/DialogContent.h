/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// What a user sees in a dialog, as comparable entries ("check:Show progress in taskbar",
// "tooltip:…"), collected from a widget dialog and from a QML dialog: the QML dialogs keep
// all the content of the widget ones (DialogContentTest).

#include <KPageWidgetModel>

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRadioButton>
#include <QSet>
#include <QSpinBox>
#include <QTabWidget>

namespace DialogContent {
inline QString clean(QString text)
{
    // "&&" is a literal ampersand; a single one marks the accelerator.
    text.replace(QLatin1String("&&"), QStringLiteral("\x01"));
    text.remove(u'&');
    text.replace(u'\x01', u'&');
    return text.simplified();
}

inline void add(QSet<QString>& entries, const char* kind, const QString& text)
{
    const QString cleaned = clean(text);
    if (!cleaned.isEmpty()) {
        entries.insert(QLatin1String(kind) + u':' + cleaned);
    }
}

inline void addRange(QSet<QString>& entries, const QVariant& from, const QVariant& to)
{
    entries.insert(QStringLiteral("range:%1-%2").arg(from.toString(), to.toString()));
}

// Widgets a user can see once the dialog is shown (the ones not hidden on purpose).
inline QSet<QString> fromWidgets(QWidget* dialog)
{
    QSet<QString> entries;
    const QList<KPageWidgetItem*> pages = dialog->findChildren<KPageWidgetItem*>();
    for (KPageWidgetItem* page : pages) {
        add(entries, "page", page->name());
    }
    const QList<QWidget*> widgets = dialog->findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (!widget->isVisibleTo(dialog)) {
            continue;
        }
        add(entries, "tooltip", widget->toolTip());
        if (auto* box = qobject_cast<QCheckBox*>(widget)) {
            add(entries, "check", box->text());
        } else if (auto* radio = qobject_cast<QRadioButton*>(widget)) {
            add(entries, "radio", radio->text());
        } else if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
            add(entries, "button", button->text());
        } else if (auto* label = qobject_cast<QLabel*>(widget)) {
            add(entries, "label", label->text());
        } else if (auto* group = qobject_cast<QGroupBox*>(widget)) {
            add(entries, "group", group->title());
        } else if (auto* tabs = qobject_cast<QTabWidget*>(widget)) {
            for (int i = 0; i < tabs->count(); ++i) {
                add(entries, "tab", tabs->tabText(i));
            }
        } else if (auto* combo = qobject_cast<QComboBox*>(widget)) {
            for (int i = 0; i < combo->count(); ++i) {
                add(entries, "choice", combo->itemText(i));
            }
        } else if (auto* spin = qobject_cast<QSpinBox*>(widget)) {
            addRange(entries, spin->minimum(), spin->maximum());
            add(entries, "suffix", spin->suffix());
        } else if (auto* doubleSpin = qobject_cast<QDoubleSpinBox*>(widget)) {
            addRange(entries, doubleSpin->minimum(), doubleSpin->maximum());
            add(entries, "suffix", doubleSpin->suffix());
        }
    }
    return entries;
}

inline void fromItem(QQuickItem* item, QSet<QString>& entries)
{
    // The attached property by the name the item's file imports QtQuick.Controls under.
    for (const QString& name : {QStringLiteral("ToolTip.text"), QStringLiteral("QQC2.ToolTip.text")}) {
        const QQmlProperty toolTip(item, name, qmlContext(item));
        if (toolTip.isValid()) {
            add(entries, "tooltip", toolTip.read().toString());
        }
    }
    const QString text = item->property("text").toString();
    if (QByteArray(item->metaObject()->className()).startsWith("DialogPage_QMLTYPE")) {
        add(entries, "page", item->property("title").toString());
    }
    // Controls: their own entries, not those of the items they're drawn with.
    if (item->inherits("QQuickCheckBox")) {
        add(entries, "check", text);
        return;
    }
    if (item->inherits("QQuickRadioButton")) {
        add(entries, "radio", text);
        return;
    }
    if (item->inherits("QQuickTabButton")) {
        add(entries, "tab", text);
        return;
    }
    if (item->inherits("QQuickAbstractButton")) {
        add(entries, "button", text);
        return;
    }
    if (item->inherits("QQuickComboBox")) {
        const int count = item->property("count").toInt();
        for (int i = 0; i < count; ++i) {
            QString choice;
            QMetaObject::invokeMethod(item, "textAt", Q_RETURN_ARG(QString, choice), Q_ARG(int, i));
            add(entries, "choice", choice);
        }
        return;
    }
    if (item->inherits("QQuickSpinBox")) {
        addRange(entries, item->property("from"), item->property("to"));
        add(entries, "suffix", item->property("suffix").toString());
        return;
    }
    if (item->inherits("QQuickTextInput") || item->inherits("QQuickTextEdit")) {
        // Read-only text (a selectable label, as Kirigami's InlineMessage shows) is a label.
        if (item->property("readOnly").toBool()) {
            add(entries, "label", text);
        }
        return;
    }
    if (item->inherits("QQuickText")) {
        add(entries, "label", text);
        return;
    }
    if (item->inherits("QQuickGroupBox")) {
        add(entries, "group", item->property("title").toString());
    }
    const QList<QQuickItem*> children = item->childItems();
    for (QQuickItem* child : children) {
        fromItem(child, entries);
    }
}

// Every item of the dialog, shown or not: a page or tab not current still counts.
inline QSet<QString> fromQml(QQuickWindow* dialog)
{
    QSet<QString> entries;
    fromItem(dialog->contentItem(), entries);
    return entries;
}

// The widget dialog's entries the QML dialog lacks, minus intended removals.
inline QStringList missing(const QSet<QString>& widget, const QSet<QString>& qml, const QSet<QString>& intendedRemovals = {})
{
    QStringList result;
    for (const QString& entry : widget) {
        if (!qml.contains(entry) && !intendedRemovals.contains(entry)) {
            result.append(entry);
        }
    }
    result.sort();
    return result;
}
}
