/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../quick/ShortcutsEditorModel.h"

#include <KActionCategory>
#include <KActionCollection>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QAction>
#include <QStandardPaths>
#include <QTest>

using namespace Konsole;

class ShortcutsEditorModelTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testRows();
    void testFilter();
    void testEditAndUndo();
    void testAllDefault();
    void testSave();
    void testCollapse();

private:
    int rowOf(const ShortcutsEditorModel& model, const QString& name) const;
    QVariant value(const ShortcutsEditorModel& model, int row, int role) const;

    KActionCollection* _main = nullptr;
    KActionCollection* _session = nullptr;
};

void ShortcutsEditorModelTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void ShortcutsEditorModelTest::init()
{
    _main = new KActionCollection(this, QStringLiteral("konsole"));
    _main->setComponentDisplayName(QStringLiteral("Konsole"));
    _session = new KActionCollection(this, QStringLiteral("konsole"));
    _session->setComponentDisplayName(QStringLiteral("Konsole"));
    const auto add = [](KActionCollection* collection, const QString& id, const QString& text, const QString& shortcut) {
        QAction* action = collection->addAction(id);
        action->setText(text);
        collection->setDefaultShortcut(action, QKeySequence(shortcut));
        return action;
    };
    add(_main, QStringLiteral("new-window"), QStringLiteral("New &Window"), QStringLiteral("Ctrl+Shift+N"));
    add(_main, QStringLiteral("item10"), QStringLiteral("Item 10"), QString());
    add(_main, QStringLiteral("item9"), QStringLiteral("Item 9"), QString());
    _main->addAction(QStringLiteral("unnamed-1"))->setText(QStringLiteral("Hidden"));
    QAction* locked = add(_main, QStringLiteral("locked"), QStringLiteral("Locked"), QStringLiteral("Ctrl+L"));
    KActionCollection::setShortcutsConfigurable(locked, false);
    add(_session, QStringLiteral("clear-history"), QStringLiteral("Clear Scrollback"), QStringLiteral("Ctrl+Shift+K"));
    auto* category = new KActionCategory(QStringLiteral("Tabs"), _session);
    QAction* inCategory = category->addAction(QStringLiteral("next-tab"));
    inCategory->setText(QStringLiteral("Next Tab"));
}

void ShortcutsEditorModelTest::cleanup()
{
    delete _main;
    delete _session;
    KSharedConfig::openConfig(QStringLiteral("konsolerc"))->deleteGroup(QStringLiteral("ActionShortcuts"));
}

int ShortcutsEditorModelTest::rowOf(const ShortcutsEditorModel& model, const QString& name) const
{
    for (int row = 0; row < model.rowCount(); ++row) {
        if (value(model, row, ShortcutsEditorModel::NameRole).toString() == name) {
            return row;
        }
    }
    return -1;
}

QVariant ShortcutsEditorModelTest::value(const ShortcutsEditorModel& model, int row, int role) const
{
    return model.data(model.index(row), role);
}

void ShortcutsEditorModelTest::testRows()
{
    ShortcutsEditorModel model({_main, _session});

    // One top item for both collections (same display name); the category; the actions
    // sorted numerically; unnamed and unconfigurable actions left out.
    QStringList names;
    QList<int> depths;
    for (int row = 0; row < model.rowCount(); ++row) {
        names.append(value(model, row, ShortcutsEditorModel::NameRole).toString());
        depths.append(value(model, row, ShortcutsEditorModel::DepthRole).toInt());
    }
    QCOMPARE(names,
        (QStringList{QStringLiteral("Konsole"),
            QStringLiteral("Clear Scrollback"),
            QStringLiteral("Item 9"),
            QStringLiteral("Item 10"),
            QStringLiteral("New Window"),
            QStringLiteral("Tabs"),
            QStringLiteral("Next Tab")}));
    QCOMPARE(depths, (QList<int>{0, 1, 1, 1, 1, 1, 2}));
    QCOMPARE(value(model, 0, ShortcutsEditorModel::KindRole).toInt(), int(ShortcutsEditorModel::TopItem));
    QCOMPARE(value(model, 5, ShortcutsEditorModel::KindRole).toInt(), int(ShortcutsEditorModel::CategoryItem));
    const int newWindow = rowOf(model, QStringLiteral("New Window"));
    QCOMPARE(value(model, newWindow, ShortcutsEditorModel::PrimaryRole).toString(), QKeySequence(QStringLiteral("Ctrl+Shift+N")).toString(QKeySequence::NativeText));
    QCOMPARE(value(model, newWindow, ShortcutsEditorModel::DefaultPrimaryRole).toString(), value(model, newWindow, ShortcutsEditorModel::PrimaryRole).toString());
}

void ShortcutsEditorModelTest::testFilter()
{
    ShortcutsEditorModel model({_main, _session});

    model.setFilterText(QStringLiteral("next"));
    // The match and the items above it.
    QVERIFY(value(model, rowOf(model, QStringLiteral("Next Tab")), ShortcutsEditorModel::VisibleRole).toBool());
    QVERIFY(value(model, rowOf(model, QStringLiteral("Tabs")), ShortcutsEditorModel::VisibleRole).toBool());
    QVERIFY(value(model, rowOf(model, QStringLiteral("Konsole")), ShortcutsEditorModel::VisibleRole).toBool());
    QVERIFY(!value(model, rowOf(model, QStringLiteral("New Window")), ShortcutsEditorModel::VisibleRole).toBool());

    // A shortcut column matches too, in any case.
    model.setFilterText(QKeySequence(QStringLiteral("Ctrl+Shift+K")).toString(QKeySequence::NativeText).toLower());
    QVERIFY(value(model, rowOf(model, QStringLiteral("Clear Scrollback")), ShortcutsEditorModel::VisibleRole).toBool());
    QVERIFY(!value(model, rowOf(model, QStringLiteral("Next Tab")), ShortcutsEditorModel::VisibleRole).toBool());

    model.setFilterText(QString());
    QVERIFY(value(model, rowOf(model, QStringLiteral("New Window")), ShortcutsEditorModel::VisibleRole).toBool());
}

void ShortcutsEditorModelTest::testEditAndUndo()
{
    ShortcutsEditorModel model({_main, _session});
    const int newWindow = rowOf(model, QStringLiteral("New Window"));
    QAction* action = _main->action(QStringLiteral("new-window"));

    QVERIFY(model.setShortcut(newWindow, ShortcutsEditorModel::Alternate, QKeySequence(QStringLiteral("Ctrl+Alt+F7")), nullptr));
    // Applied to the action at once, as KShortcutsEditorItem does.
    QCOMPARE(action->shortcuts(), (QList<QKeySequence>{QKeySequence(QStringLiteral("Ctrl+Shift+N")), QKeySequence(QStringLiteral("Ctrl+Alt+F7"))}));
    QVERIFY(model.isModified());

    model.undo();
    QCOMPARE(action->shortcuts(), (QList<QKeySequence>{QKeySequence(QStringLiteral("Ctrl+Shift+N"))}));
    QVERIFY(!model.isModified());

    // Back to the old value by hand: not modified any more.
    QVERIFY(model.setShortcut(newWindow, ShortcutsEditorModel::Primary, QKeySequence(QStringLiteral("Ctrl+Alt+F8")), nullptr));
    QVERIFY(model.setShortcut(newWindow, ShortcutsEditorModel::Primary, QKeySequence(QStringLiteral("Ctrl+Shift+N")), nullptr));
    QVERIFY(!model.isModified());
}

void ShortcutsEditorModelTest::testAllDefault()
{
    QAction* action = _session->action(QStringLiteral("clear-history"));
    action->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+K")));
    ShortcutsEditorModel model({_main, _session});

    model.allDefault();

    QCOMPARE(action->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+K")));
    QVERIFY(model.isModified());
    model.undo();
    QCOMPARE(action->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+K")));
}

void ShortcutsEditorModelTest::testSave()
{
    ShortcutsEditorModel model({_main, _session});
    QVERIFY(model.setShortcut(rowOf(model, QStringLiteral("New Window")), ShortcutsEditorModel::Primary, QKeySequence(QStringLiteral("Ctrl+Alt+F7")), nullptr));

    model.save();

    QVERIFY(!model.isModified());
    // Kept after save: undo has nothing to restore.
    model.undo();
    QCOMPARE(_main->action(QStringLiteral("new-window"))->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+F7")));
    const KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("konsolerc")), QStringLiteral("ActionShortcuts"));
    QCOMPARE(group.readEntry(QStringLiteral("konsole/new-window"), QString()), QStringLiteral("Ctrl+Alt+F7"));
}

void ShortcutsEditorModelTest::testCollapse()
{
    ShortcutsEditorModel model({_main, _session});
    const int tabs = rowOf(model, QStringLiteral("Tabs"));

    model.toggleCollapsed(tabs);
    QVERIFY(value(model, tabs, ShortcutsEditorModel::VisibleRole).toBool());
    QVERIFY(!value(model, rowOf(model, QStringLiteral("Next Tab")), ShortcutsEditorModel::VisibleRole).toBool());

    model.toggleCollapsed(tabs);
    QVERIFY(value(model, rowOf(model, QStringLiteral("Next Tab")), ShortcutsEditorModel::VisibleRole).toBool());
}

QTEST_MAIN(ShortcutsEditorModelTest)

#include "ShortcutsEditorModelTest.moc"
