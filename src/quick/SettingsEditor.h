/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class KConfigSkeletonItem;
class KCoreConfigSkeleton;

namespace Konsole {
/**
 * A settings dialog's edits of KonsoleSettings, as KConfigDialogManager and
 * ConfigDialogButtonGroupManager keep them for a widget dialog: values change only on
 * apply(); hasChanged and isDefault cover the settings the dialog shows (names).
 */
class KONSOLEPRIVATE_EXPORT SettingsEditor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList names READ names WRITE setNames NOTIFY namesChanged)
    Q_PROPERTY(bool hasChanged READ hasChanged NOTIFY changed)
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY changed)

public:
    explicit SettingsEditor(QObject* parent = nullptr);
    SettingsEditor(KCoreConfigSkeleton* config, QObject* parent);

    QStringList names() const;
    void setNames(const QStringList& names);

    bool hasChanged() const;
    bool isDefault() const;

    // The shown value: the edited one, else the stored one.
    Q_INVOKABLE QVariant value(const QString& name) const;
    // A string sets an Url setting from a path, as KUrlRequester does.
    Q_INVOKABLE void setValue(const QString& name, const QVariant& value);
    // An enum setting by choice name (a radio button's object name in the widget dialog).
    Q_INVOKABLE QString choice(const QString& name) const;
    Q_INVOKABLE void setChoice(const QString& name, const QString& choice);
    // An Url setting as KUrlRequester shows it.
    Q_INVOKABLE QString path(const QString& name) const;
    // The setting's tooltip, which KConfigDialogManager gives a widget without one.
    Q_INVOKABLE QString toolTip(const QString& name) const;

    // OK and Apply: stores and saves the edits.
    Q_INVOKABLE void apply();
    // Defaults: the shown settings edited to their defaults.
    Q_INVOKABLE void defaults();
    // Cancel: the edits dropped.
    Q_INVOKABLE void reset();

Q_SIGNALS:
    void namesChanged();
    void changed();

private:
    KConfigSkeletonItem* item(const QString& name) const;

    KCoreConfigSkeleton* _config;
    QStringList _names;
    QVariantMap _edits;
};
}
