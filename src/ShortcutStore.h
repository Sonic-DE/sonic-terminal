/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "konsoleprivate_export.h"

#include <KSharedConfig>

#include <QList>
#include <QObject>

class KActionCollection;
class KXMLGUIFactory;

namespace Konsole {
/**
 * The user's action shortcuts, in konsolerc [ActionShortcuts] (<client>/<action name>=<key
 * sequences>, empty for none; the main window and the session clients share action names).
 * An action without an entry has its default. They replace the shortcuts KXmlGui keeps in
 * the clients' local .rc files, which migrate() copies once.
 */
class KONSOLEPRIVATE_EXPORT ShortcutStore : public QObject {
    Q_OBJECT

public:
    explicit ShortcutStore(KSharedConfigPtr config, QObject* parent = nullptr);
    ~ShortcutStore() override;

    // The application's, on konsolerc.
    static ShortcutStore* instance();

    // Once (marker Migrated=true): each client's shortcuts from `rcDir`/<client>ui.rc, or
    // from [Shortcuts] when that file doesn't exist (as KActionCollection reads them).
    void migrate(const QString& rcDir, const QStringList& clients = {QStringLiteral("konsole"), QStringLiteral("session")});

    void apply(KActionCollection* collection) const;
    // Applies to every client the factory has and adds, and again after each save.
    void attach(KXMLGUIFactory* factory);
    // Stores the collections' shortcuts that differ from their defaults; removes the rest.
    void save(const QList<KActionCollection*>& collections);
    // Applies the stored shortcuts again to every attached factory (after a scheme change).
    void reapply();

    // A collection's client in the keys: its GUI document's name ("konsole", "session",
    // as their .rc files), else its component name.
    static QString clientName(const KActionCollection* collection);

Q_SIGNALS:
    void changed();

private:
    KSharedConfigPtr _config;
};
}
