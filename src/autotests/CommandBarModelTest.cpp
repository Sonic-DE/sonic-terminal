/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "../quick/CommandBarModel.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QAction>
#include <QMenu>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace Konsole;

class CommandBarModelTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void testRows();
    void testLastUsedFirst();
    void testFilter();
    void testActivateAction();
    void testMenusDescend();
    void testShortcutSegments();

private:
    QStringList rows(const CommandBarModel& model) const;
    QAction* action(const QString& text, const QString& shortcut = QString());

    QList<QAction*> _actions;
};

void CommandBarModelTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void CommandBarModelTest::init()
{
    qDeleteAll(_actions);
    _actions.clear();
    KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("General")).deleteEntry("CommandBarLastUsedActions");
}

QAction* CommandBarModelTest::action(const QString& text, const QString& shortcut)
{
    auto* action = new QAction(text);
    action->setShortcut(QKeySequence(shortcut));
    _actions.append(action);
    return action;
}

QStringList CommandBarModelTest::rows(const CommandBarModel& model) const
{
    QStringList result;
    for (int row = 0; row < model.rowCount(); ++row) {
        result.append(model.data(model.index(row), Qt::DisplayRole).toString());
    }
    return result;
}

void CommandBarModelTest::testRows()
{
    QAction* disabled = action(QStringLiteral("Disabled"));
    disabled->setEnabled(false);
    CommandBarModel model;
    model.setActions({{QStringLiteral("Edit"), {action(QStringLiteral("&Paste")), action(QStringLiteral("Copy")), disabled}},
        {QString(), {action(QStringLiteral("New Tab"))}}});

    // Enabled actions, "Group: Command" without markers, sorted by text; Clear History last group.
    QCOMPARE(rows(model),
        (QStringList{QStringLiteral("Command Bar: Clear History"), QStringLiteral("Edit: Copy"), QStringLiteral("Edit: Paste"), QStringLiteral("New Tab")}));
    QCOMPARE(model.data(model.index(1), CommandBarModel::GroupLengthRole).toInt(), 4);
    QCOMPARE(model.data(model.index(3), CommandBarModel::GroupLengthRole).toInt(), 0);
    QVERIFY(model.placeholderText().isEmpty());
}

void CommandBarModelTest::testLastUsedFirst()
{
    KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("General"))
        .writeEntry("CommandBarLastUsedActions", QStringList{QStringLiteral("New Tab"), QStringLiteral("Copy")});
    CommandBarModel model;
    model.setActions({{QString(), {action(QStringLiteral("Copy")), action(QStringLiteral("Find")), action(QStringLiteral("New Tab"))}}});

    // The most recent first, then the rest by text.
    QCOMPARE(rows(model).mid(0, 3), (QStringList{QStringLiteral("New Tab"), QStringLiteral("Copy"), QStringLiteral("Command Bar: Clear History")}));
}

void CommandBarModelTest::testFilter()
{
    CommandBarModel model;
    model.setActions({{QStringLiteral("View"), {action(QStringLiteral("Split View Left/Right"), QStringLiteral("Ctrl+(")), action(QStringLiteral("Find"))}}});

    model.setFilterText(QStringLiteral("splt"));
    QCOMPARE(rows(model), QStringList{QStringLiteral("View: Split View Left/Right")});
    // Matched ranges are in the display text, after "View: ".
    const QVariantList matches = model.data(model.index(0), CommandBarModel::MatchesRole).toList();
    QVERIFY(!matches.isEmpty());
    QVERIFY(matches.first().toInt() >= 6);
    QCOMPARE(model.data(model.index(0), CommandBarModel::ShortcutRole).toString(), QKeySequence(QStringLiteral("Ctrl+(")).toString());

    model.setFilterText(QStringLiteral("zzzz"));
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.placeholderText(), QStringLiteral("No commands matching the filter"));
}

void CommandBarModelTest::testActivateAction()
{
    QAction* newTab = action(QStringLiteral("New Tab"));
    QSignalSpy triggered(newTab, &QAction::triggered);
    {
        CommandBarModel model;
        model.setActions({{QString(), {newTab}}});
        QVERIFY(!model.activate(rows(model).indexOf(QStringLiteral("New Tab"))));
        // Triggered after the bar closed.
        QCOMPARE(triggered.count(), 0);
        QTRY_COMPARE(triggered.count(), 1);
    }
    // Remembered for the next bar.
    QCOMPARE(KConfigGroup(KSharedConfig::openStateConfig(), QStringLiteral("General")).readEntry("CommandBarLastUsedActions", QStringList()),
        QStringList{QStringLiteral("New Tab")});
}

void CommandBarModelTest::testMenusDescend()
{
    // fillRows(): a menu's actions are rows under its title; one filled on demand is asked to.
    QMenu menu(QStringLiteral("&Profiles"));
    QAction* menuAction = action(QStringLiteral("&Profiles"));
    menuAction->setMenu(&menu);
    connect(&menu, &QMenu::aboutToShow, &menu, [&menu] {
        if (menu.actions().isEmpty()) {
            menu.addAction(QStringLiteral("Default"));
        }
    });
    CommandBarModel model;
    model.setActions({{QString(), {menuAction}}});
    QCOMPARE(rows(model), (QStringList{QStringLiteral("Command Bar: Clear History"), QStringLiteral("Profiles: Default")}));
}

void CommandBarModelTest::testShortcutSegments()
{
    QCOMPARE(CommandBarModel::shortcutSegments(QStringLiteral("Ctrl+A, Ctrl+K")),
        (QStringList{QStringLiteral("Ctrl"), QStringLiteral("+"), QStringLiteral("A"), QStringLiteral(", "), QStringLiteral("Ctrl"), QStringLiteral("+"), QStringLiteral("K")}));
    QCOMPARE(CommandBarModel::shortcutSegments(QStringLiteral("F11")), QStringList{QStringLiteral("F11")});
}

QTEST_MAIN(CommandBarModelTest)

#include "CommandBarModelTest.moc"
