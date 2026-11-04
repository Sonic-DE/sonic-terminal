/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../ShortcutStore.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KXMLGUIClient>
#include <KXMLGUIFactory>
#include <KXmlGuiWindow>

#include <QAction>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Konsole;

class ShortcutStoreTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void testMigrateFromRcFiles();
    void testMigrateFallsBackToShortcutsGroup();
    void testMigrateOnce();
    void testApply();
    void testSave();
    void testAppliedAfterAddClient();

private:
    void writeRc(const QString& client, const QString& actions);
    KConfigGroup group() const;
    static QString key(const KActionCollection& collection, const QString& action);

    std::unique_ptr<QTemporaryDir> _dir;
    KSharedConfigPtr _config;
};

void ShortcutStoreTest::init()
{
    _dir = std::make_unique<QTemporaryDir>();
    QVERIFY(_dir->isValid());
    _config = KSharedConfig::openConfig(_dir->filePath(QStringLiteral("konsolerc")), KConfig::SimpleConfig);
}

void ShortcutStoreTest::writeRc(const QString& client, const QString& actions)
{
    QFile file(_dir->filePath(client + QStringLiteral("ui.rc")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QStringLiteral("<!DOCTYPE gui SYSTEM \"kpartgui.dtd\">\n<gui name=\"%1\" version=\"1\">\n<MenuBar/>\n"
                              "<ActionProperties scheme=\"Default\">\n%2\n</ActionProperties>\n</gui>\n")
            .arg(client, actions)
            .toUtf8());
}

KConfigGroup ShortcutStoreTest::group() const
{
    return KConfigGroup(_config, QStringLiteral("ActionShortcuts"));
}

QString ShortcutStoreTest::key(const KActionCollection& collection, const QString& action)
{
    return ShortcutStore::clientName(&collection) + QLatin1Char('/') + action;
}

void ShortcutStoreTest::testMigrateFromRcFiles()
{
    writeRc(QStringLiteral("konsole"), QStringLiteral("<Action name=\"new-window\" shortcut=\"Ctrl+Alt+N\"/><Action name=\"no-key\" shortcut=\"\"/>"
                                                      "<Action name=\"icon-only\" icon=\"x\"/>"
                                                      "<Action name=\"split-view-left-right\" shortcut=\"Ctrl+Alt+(\"/>"));
    writeRc(QStringLiteral("session"),
        QStringLiteral("<Action name=\"clear-history\" shortcut=\"Ctrl+Alt+K; Ctrl+Alt+L\"/><Action name=\"split-view-left-right\" shortcut=\"F3\"/>"));
    // Ignored: both clients have their .rc.
    KConfigGroup(_config, QStringLiteral("Shortcuts")).writeEntry("from-group", "Ctrl+G");

    ShortcutStore(_config).migrate(_dir->path());

    QCOMPARE(group().readEntry("konsole/new-window", QString()), QStringLiteral("Ctrl+Alt+N"));
    QVERIFY(group().hasKey("konsole/no-key"));
    QCOMPARE(group().readEntry("konsole/no-key", QStringLiteral("unset")), QString());
    QVERIFY(!group().hasKey("konsole/icon-only"));
    QCOMPARE(group().readEntry("session/clear-history", QString()), QStringLiteral("Ctrl+Alt+K; Ctrl+Alt+L"));
    // The same name in both clients stays apart.
    QCOMPARE(group().readEntry("konsole/split-view-left-right", QString()), QStringLiteral("Ctrl+Alt+("));
    QCOMPARE(group().readEntry("session/split-view-left-right", QString()), QStringLiteral("F3"));
    QVERIFY(!group().hasKey("konsole/from-group"));
    QVERIFY(!group().hasKey("session/from-group"));
    QVERIFY(group().readEntry("Migrated", false));
}

void ShortcutStoreTest::testMigrateFallsBackToShortcutsGroup()
{
    writeRc(QStringLiteral("konsole"), QStringLiteral("<Action name=\"new-window\" shortcut=\"Ctrl+Alt+N\"/>"));
    KConfigGroup(_config, QStringLiteral("Shortcuts")).writeEntry("clear-history", "Ctrl+Alt+K");

    ShortcutStore(_config).migrate(_dir->path());

    QCOMPARE(group().readEntry("konsole/new-window", QString()), QStringLiteral("Ctrl+Alt+N"));
    QVERIFY(!group().hasKey("konsole/clear-history"));
    QCOMPARE(group().readEntry("session/clear-history", QString()), QStringLiteral("Ctrl+Alt+K"));
}

void ShortcutStoreTest::testMigrateOnce()
{
    writeRc(QStringLiteral("konsole"), QStringLiteral("<Action name=\"new-window\" shortcut=\"Ctrl+Alt+N\"/>"));
    ShortcutStore store(_config);
    store.migrate(_dir->path());
    group().writeEntry("konsole/new-window", "Ctrl+Alt+W");
    writeRc(QStringLiteral("konsole"), QStringLiteral("<Action name=\"new-window\" shortcut=\"Ctrl+Alt+X\"/><Action name=\"other\" shortcut=\"F2\"/>"));

    store.migrate(_dir->path());

    QCOMPARE(group().readEntry("konsole/new-window", QString()), QStringLiteral("Ctrl+Alt+W"));
    QVERIFY(!group().hasKey("konsole/other"));
}

void ShortcutStoreTest::testApply()
{
    KActionCollection collection(this);
    QAction* stored = collection.addAction(QStringLiteral("stored"));
    collection.setDefaultShortcut(stored, QKeySequence(QStringLiteral("Ctrl+A")));
    QAction* defaulted = collection.addAction(QStringLiteral("defaulted"));
    collection.setDefaultShortcut(defaulted, QKeySequence(QStringLiteral("Ctrl+C")));
    defaulted->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    QAction* noDefault = collection.addAction(QStringLiteral("no-default"));
    noDefault->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    QAction* cleared = collection.addAction(QStringLiteral("cleared"));
    collection.setDefaultShortcut(cleared, QKeySequence(QStringLiteral("Ctrl+F")));
    QAction* locked = collection.addAction(QStringLiteral("locked"));
    collection.setDefaultShortcut(locked, QKeySequence(QStringLiteral("Ctrl+G")));
    locked->setProperty("isShortcutConfigurable", false);
    QAction* otherClient = collection.addAction(QStringLiteral("other-client"));
    collection.setDefaultShortcut(otherClient, QKeySequence(QStringLiteral("Ctrl+O")));
    group().writeEntry(key(collection, QStringLiteral("stored")), "Ctrl+B; Ctrl+Shift+B");
    group().writeEntry(key(collection, QStringLiteral("cleared")), "");
    group().writeEntry(key(collection, QStringLiteral("locked")), "Ctrl+H");
    group().writeEntry("another-client/other-client", "Ctrl+P");

    ShortcutStore(_config).apply(&collection);

    QCOMPARE(stored->shortcuts(), (QList<QKeySequence>{QKeySequence(QStringLiteral("Ctrl+B")), QKeySequence(QStringLiteral("Ctrl+Shift+B"))}));
    QCOMPARE(defaulted->shortcut(), QKeySequence(QStringLiteral("Ctrl+C")));
    QCOMPARE(noDefault->shortcut(), QKeySequence(QStringLiteral("Ctrl+E")));
    QVERIFY(cleared->shortcuts().isEmpty());
    QCOMPARE(locked->shortcut(), QKeySequence(QStringLiteral("Ctrl+G")));
    QCOMPARE(otherClient->shortcut(), QKeySequence(QStringLiteral("Ctrl+O")));
}

void ShortcutStoreTest::testSave()
{
    KActionCollection collection(this);
    QAction* changed = collection.addAction(QStringLiteral("changed"));
    collection.setDefaultShortcut(changed, QKeySequence(QStringLiteral("Ctrl+A")));
    changed->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+A")));
    QAction* reset = collection.addAction(QStringLiteral("reset"));
    collection.setDefaultShortcut(reset, QKeySequence(QStringLiteral("Ctrl+R")));
    QAction* cleared = collection.addAction(QStringLiteral("cleared"));
    collection.setDefaultShortcut(cleared, QKeySequence(QStringLiteral("Ctrl+C")));
    cleared->setShortcuts(QList<QKeySequence>());
    group().writeEntry(key(collection, QStringLiteral("reset")), "Ctrl+Alt+R");
    ShortcutStore store(_config);
    QSignalSpy spy(&store, &ShortcutStore::changed);

    store.save({&collection});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(group().readEntry(key(collection, QStringLiteral("changed")), QString()), QStringLiteral("Ctrl+Alt+A"));
    QVERIFY(!group().hasKey(key(collection, QStringLiteral("reset"))));
    QVERIFY(group().hasKey(key(collection, QStringLiteral("cleared"))));
    QCOMPARE(group().readEntry(key(collection, QStringLiteral("cleared")), QStringLiteral("unset")), QString());
    // On disk, for the next start.
    KConfig reread(_dir->filePath(QStringLiteral("konsolerc")), KConfig::SimpleConfig);
    QCOMPARE(reread.group(QStringLiteral("ActionShortcuts")).readEntry(key(collection, QStringLiteral("changed")), QString()), QStringLiteral("Ctrl+Alt+A"));
}

void ShortcutStoreTest::testAppliedAfterAddClient()
{
    // KXmlGui sets the .rc's value on each addClient(); the store's wins, also after a save.
    class Client : public KXMLGUIClient {
    public:
        Client()
        {
            setXML(QStringLiteral("<!DOCTYPE gui SYSTEM \"kpartgui.dtd\"><gui name=\"test\" version=\"1\"><MenuBar/>"
                                  "<ActionProperties><Action name=\"act\" shortcut=\"Ctrl+Alt+R\"/></ActionProperties></gui>"));
        }
    };
    KXmlGuiWindow window;
    KXMLGUIFactory factory(&window);
    Client client;
    QAction* action = client.actionCollection()->addAction(QStringLiteral("act"));
    client.actionCollection()->setDefaultShortcut(action, QKeySequence(QStringLiteral("Ctrl+D")));
    group().writeEntry(key(*client.actionCollection(), QStringLiteral("act")), "Ctrl+Alt+S");
    ShortcutStore store(_config);
    store.attach(&factory);

    factory.addClient(&client);
    QCOMPARE(action->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+S")));

    factory.removeClient(&client);
    factory.addClient(&client);
    QCOMPARE(action->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+S")));

    // Back to the default in one window: every attached factory applies it.
    action->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    store.save({client.actionCollection()});
    action->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+R")));
    Q_EMIT store.changed();
    QCOMPARE(action->shortcut(), QKeySequence(QStringLiteral("Ctrl+D")));
}

QTEST_MAIN(ShortcutStoreTest)

#include "ShortcutStoreTest.moc"
