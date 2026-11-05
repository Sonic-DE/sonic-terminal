/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ShortcutStore.h"

#include <Sonic/Logging/Log>

#include <KActionCollection>
#include <KConfigGroup>
#include <KXMLGUIClient>
#include <KXMLGUIFactory>

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QKeySequence>

using namespace Konsole;

namespace {
const QString GroupName = QStringLiteral("ActionShortcuts");
const QString MigratedKey = QStringLiteral("Migrated");

// As KShortcutsEditor lists them: named and not marked unconfigurable.
bool configurable(const QAction* action)
{
    const QString name = action->objectName();
    if (name.isEmpty() || name.startsWith(QLatin1String("unnamed-"))) {
        return false;
    }
    const QVariant value = action->property("isShortcutConfigurable");
    return !value.isValid() || value.toBool();
}

QList<QKeySequence> defaultShortcuts(const QAction* action)
{
    return action->property("defaultShortcuts").value<QList<QKeySequence>>();
}

// listFromString("") is one empty sequence, not none.
QList<QKeySequence> parse(const QString& value)
{
    QList<QKeySequence> sequences = QKeySequence::listFromString(value, QKeySequence::PortableText);
    sequences.removeIf([](const QKeySequence& sequence) {
        return sequence.isEmpty();
    });
    return sequences;
}

QString key(const QString& client, const QString& action)
{
    return client + QLatin1Char('/') + action;
}
}

QString ShortcutStore::clientName(const KActionCollection* collection)
{
    // The document's <gui name>: "konsole" and "session" for konsoleui.rc and sessionui.rc.
    const KXMLGUIClient* client = collection->parentGUIClient();
    const QString name = client ? client->domDocument().documentElement().attribute(QStringLiteral("name")) : QString();
    return name.isEmpty() ? collection->componentName() : name;
}

ShortcutStore::ShortcutStore(KSharedConfigPtr config, QObject* parent)
    : QObject(parent)
    , _config(std::move(config))
{
}

ShortcutStore::~ShortcutStore() = default;

ShortcutStore* ShortcutStore::instance()
{
    // By name: in a host, its own config isn't the terminal's.
    static auto* store = new ShortcutStore(KSharedConfig::openConfig(QStringLiteral("konsolerc")), QCoreApplication::instance());
    return store;
}

void ShortcutStore::migrate(const QString& rcDir, const QStringList& clients)
{
    KConfigGroup group(_config, GroupName);
    if (group.readEntry(MigratedKey, false)) {
        return;
    }
    for (const QString& client : clients) {
        QFile file(QDir(rcDir).filePath(client + QStringLiteral("ui.rc")));
        if (!file.exists()) {
            // As KActionCollection::readSettings(): the config's [Shortcuts] when there's no .rc.
            const KConfigGroup old(_config, QStringLiteral("Shortcuts"));
            const QMap<QString, QString> entries = old.entryMap();
            for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
                group.writeEntry(key(client, it.key()), it.value());
                sonicInfo() << "shortcut migrated from [Shortcuts]:" << client << it.key() << it.value();
            }
            continue;
        }
        QDomDocument document;
        if (!file.open(QIODevice::ReadOnly) || !document.setContent(&file)) {
            sonicWarning() << "shortcuts not migrated, unreadable:" << file.fileName();
            continue;
        }
        for (QDomElement properties = document.documentElement().firstChildElement(QStringLiteral("ActionProperties")); !properties.isNull();
            properties = properties.nextSiblingElement(QStringLiteral("ActionProperties"))) {
            for (QDomElement action = properties.firstChildElement(QStringLiteral("Action")); !action.isNull();
                action = action.nextSiblingElement(QStringLiteral("Action"))) {
                const QString name = action.attribute(QStringLiteral("name"));
                if (name.isEmpty() || !action.hasAttribute(QStringLiteral("shortcut"))) {
                    continue;
                }
                group.writeEntry(key(client, name), action.attribute(QStringLiteral("shortcut")));
                sonicInfo() << "shortcut migrated from" << file.fileName() << ":" << name << action.attribute(QStringLiteral("shortcut"));
            }
        }
    }
    group.writeEntry(MigratedKey, true);
    _config->sync();
}

void ShortcutStore::apply(KActionCollection* collection) const
{
    if (!collection) {
        return;
    }
    const KConfigGroup group(_config, GroupName);
    const QString client = clientName(collection);
    int applied = 0;
    const auto actions = collection->actions();
    for (QAction* action : actions) {
        if (!configurable(action)) {
            continue;
        }
        const QString entry = key(client, action->objectName());
        QList<QKeySequence> wanted;
        if (group.hasKey(entry)) {
            wanted = parse(group.readEntry(entry, QString()));
        } else if (action->property("defaultShortcuts").isValid()) {
            wanted = defaultShortcuts(action);
        } else {
            // Set in code without a default: nothing to restore.
            continue;
        }
        if (action->shortcuts() != wanted) {
            action->setShortcuts(wanted);
            ++applied;
        }
    }
    sonicDebug() << "shortcuts applied to" << client << ":" << applied << "changed";
}

void ShortcutStore::attach(KXMLGUIFactory* factory)
{
    // KXmlGui sets the defaults and the .rc's values before clientAdded.
    connect(factory, &KXMLGUIFactory::clientAdded, this, [this](KXMLGUIClient* client) {
        apply(client->actionCollection());
    });
    connect(this, &ShortcutStore::changed, factory, [this, factory] {
        const auto clients = factory->clients();
        for (KXMLGUIClient* client : clients) {
            apply(client->actionCollection());
        }
    });
    const auto clients = factory->clients();
    for (KXMLGUIClient* client : clients) {
        apply(client->actionCollection());
    }
}

void ShortcutStore::save(const QList<KActionCollection*>& collections)
{
    KConfigGroup group(_config, GroupName);
    for (KActionCollection* collection : collections) {
        const QString client = clientName(collection);
        const auto actions = collection->actions();
        for (QAction* action : actions) {
            if (!configurable(action)) {
                continue;
            }
            const QString entry = key(client, action->objectName());
            if (action->shortcuts() != defaultShortcuts(action)) {
                group.writeEntry(entry, QKeySequence::listToString(action->shortcuts(), QKeySequence::PortableText));
                sonicInfo() << "shortcut saved:" << entry << action->shortcuts();
            } else if (group.hasKey(entry)) {
                group.deleteEntry(entry);
                sonicInfo() << "shortcut back to its default:" << entry;
            }
        }
    }
    _config->sync();
    Q_EMIT changed();
}

void ShortcutStore::reapply()
{
    Q_EMIT changed();
}

#include "moc_ShortcutStore.cpp"
