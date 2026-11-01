/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "SettingsEditor.h"

#include "KonsoleSettings.h"

#include <KCoreConfigSkeleton>
#include <Sonic/Logging/Log>

#include <QUrl>

namespace Konsole {
SettingsEditor::SettingsEditor(QObject* parent)
    : SettingsEditor(KonsoleSettings::self(), parent)
{
}

SettingsEditor::SettingsEditor(KCoreConfigSkeleton* config, QObject* parent)
    : QObject(parent)
    , _config(config)
{
}

QStringList SettingsEditor::names() const
{
    return _names;
}

void SettingsEditor::setNames(const QStringList& names)
{
    if (_names == names) {
        return;
    }
    for (const QString& name : names) {
        if (!item(name)) {
            sonicWarning() << "No setting" << name;
        }
    }
    _names = names;
    Q_EMIT namesChanged();
    Q_EMIT changed();
}

bool SettingsEditor::hasChanged() const
{
    return !_edits.isEmpty();
}

bool SettingsEditor::isDefault() const
{
    for (const QString& name : _names) {
        if (KConfigSkeletonItem* setting = item(name); setting && value(name) != setting->getDefault()) {
            return false;
        }
    }
    return true;
}

QVariant SettingsEditor::value(const QString& name) const
{
    KConfigSkeletonItem* setting = item(name);
    return setting ? _edits.value(name, setting->property()) : QVariant();
}

void SettingsEditor::setValue(const QString& name, const QVariant& value)
{
    KConfigSkeletonItem* setting = item(name);
    if (!setting) {
        sonicWarning() << "No setting" << name;
        return;
    }
    QVariant converted = value;
    if (dynamic_cast<KCoreConfigSkeleton::ItemUrl*>(setting) && value.typeId() == QMetaType::QString) {
        const QString text = value.toString();
        converted = text.isEmpty() ? QUrl() : QUrl::fromUserInput(text, QString(), QUrl::AssumeLocalFile);
    } else {
        converted.convert(setting->property().metaType());
    }
    if (setting->isEqual(converted)) {
        if (_edits.remove(name) == 0) {
            return;
        }
    } else if (_edits.value(name) == converted) {
        return;
    } else {
        _edits.insert(name, converted);
    }
    Q_EMIT changed();
}

QString SettingsEditor::choice(const QString& name) const
{
    auto* setting = dynamic_cast<KCoreConfigSkeleton::ItemEnum*>(item(name));
    if (!setting) {
        return QString();
    }
    const int index = value(name).toInt();
    const auto choices = setting->choices();
    return index >= 0 && index < choices.size() ? choices.at(index).name : QString();
}

void SettingsEditor::setChoice(const QString& name, const QString& choice)
{
    auto* setting = dynamic_cast<KCoreConfigSkeleton::ItemEnum*>(item(name));
    if (!setting) {
        sonicWarning() << "No enum setting" << name;
        return;
    }
    const auto choices = setting->choices();
    for (int i = 0; i < choices.size(); ++i) {
        if (choices.at(i).name == choice) {
            return setValue(name, i);
        }
    }
    sonicWarning() << "No choice" << choice << "for" << name;
}

QString SettingsEditor::path(const QString& name) const
{
    return value(name).toUrl().toDisplayString(QUrl::PreferLocalFile);
}

QString SettingsEditor::toolTip(const QString& name) const
{
    KConfigSkeletonItem* setting = item(name);
    return setting ? setting->toolTip() : QString();
}

void SettingsEditor::apply()
{
    if (_edits.isEmpty()) {
        return;
    }
    for (auto it = _edits.cbegin(); it != _edits.cend(); ++it) {
        item(it.key())->setProperty(it.value());
    }
    _edits.clear();
    _config->save();
    Q_EMIT changed();
}

void SettingsEditor::defaults()
{
    for (const QString& name : std::as_const(_names)) {
        if (KConfigSkeletonItem* setting = item(name)) {
            setValue(name, setting->getDefault());
        }
    }
}

void SettingsEditor::reset()
{
    if (_edits.isEmpty()) {
        return;
    }
    _edits.clear();
    Q_EMIT changed();
}

KConfigSkeletonItem* SettingsEditor::item(const QString& name) const
{
    return _config->findItem(name);
}
}

#include "moc_SettingsEditor.cpp"
