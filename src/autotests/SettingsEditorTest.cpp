/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// SettingsEditor keeps a settings dialog's edits as KConfigDialogManager does: nothing
// changes before apply(), every setting round-trips through the config file, and Apply
// and Defaults follow ConfigurationDialog::updateButtons().

#include "../quick/SettingsEditor.h"
#include "KonsoleSettings.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QUrl>

using namespace Konsole;

class SettingsEditorTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testRoundTrip();
    void testButtonStates();
    void testDefaultsAndReset();
    void testChoice();
    void testUrlFromPath();

private:
    static QVariant otherValue(KConfigSkeletonItem* item);
};

void SettingsEditorTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void SettingsEditorTest::cleanup()
{
    KonsoleSettings::self()->setDefaults();
    KonsoleSettings::self()->save();
}

// A value the setting doesn't hold, of its own type.
QVariant SettingsEditorTest::otherValue(KConfigSkeletonItem* item)
{
    const QVariant current = item->property();
    if (auto* choices = dynamic_cast<KCoreConfigSkeleton::ItemEnum*>(item)) {
        return (current.toInt() + 1) % choices->choices().size();
    }
    switch (current.typeId()) {
    case QMetaType::Bool:
        return !current.toBool();
    case QMetaType::Int: {
        const QVariant max = item->maxValue();
        return max.isValid() && current.toInt() >= max.toInt() ? current.toInt() - 1 : current.toInt() + 1;
    }
    case QMetaType::QString:
        return QString(current.toString() + QStringLiteral("x"));
    case QMetaType::QUrl:
        return QUrl::fromLocalFile(QStringLiteral("/tmp/settings-editor-test"));
    }
    return QVariant();
}

void SettingsEditorTest::testRoundTrip()
{
    const KConfigSkeletonItem::List items = KonsoleSettings::self()->items();
    QVERIFY(!items.isEmpty());
    SettingsEditor editor(KonsoleSettings::self(), nullptr);
    QHash<QString, QVariant> edited;
    for (KConfigSkeletonItem* item : items) {
        const QVariant value = otherValue(item);
        QVERIFY2(value.isValid(), qPrintable(item->name() + QStringLiteral(": unknown type ") + QString::fromLatin1(item->property().typeName())));
        const QVariant stored = item->property();
        editor.setValue(item->name(), value);
        QCOMPARE(editor.value(item->name()), value);
        QCOMPARE(item->property(), stored);
        edited.insert(item->name(), value);
    }
    QVERIFY(editor.hasChanged());

    editor.apply();
    QVERIFY(!editor.hasChanged());
    // Read back from the file, not the items the editor just set.
    KonsoleSettings::self()->setDefaults();
    KonsoleSettings::self()->read();
    for (KConfigSkeletonItem* item : items) {
        QVERIFY2(item->isEqual(edited.value(item->name())), qPrintable(item->name()));
    }
}

void SettingsEditorTest::testButtonStates()
{
    SettingsEditor editor(KonsoleSettings::self(), nullptr);
    QSignalSpy changed(&editor, &SettingsEditor::changed);
    editor.setNames({QStringLiteral("ShowProgressInTaskBar"), QStringLiteral("ThumbnailSize")});
    QVERIFY(!editor.hasChanged());
    QVERIFY(editor.isDefault());

    editor.setValue(QStringLiteral("ShowProgressInTaskBar"), false);
    QVERIFY(editor.hasChanged());
    QVERIFY(!editor.isDefault());
    // Back to the stored value: nothing to apply.
    editor.setValue(QStringLiteral("ShowProgressInTaskBar"), true);
    QVERIFY(!editor.hasChanged());
    QVERIFY(editor.isDefault());
    QCOMPARE(changed.count(), 3);

    // A setting the dialog doesn't show doesn't count for Defaults.
    KonsoleSettings::self()->setFocusFollowsMouse(true);
    QVERIFY(editor.isDefault());
    editor.setValue(QStringLiteral("ThumbnailSize"), 300);
    QVERIFY(!editor.isDefault());
    editor.apply();
    QCOMPARE(KonsoleSettings::thumbnailSize(), 300);
    QVERIFY(!editor.hasChanged());
    QVERIFY(!editor.isDefault());
}

void SettingsEditorTest::testDefaultsAndReset()
{
    KonsoleSettings::self()->setThumbnailSize(300);
    KonsoleSettings::self()->setEnableThumbnails(false);
    SettingsEditor editor(KonsoleSettings::self(), nullptr);
    editor.setNames({QStringLiteral("ThumbnailSize"), QStringLiteral("EnableThumbnails")});
    QVERIFY(!editor.isDefault());

    // Defaults edits the shown settings; nothing is stored yet.
    editor.defaults();
    QVERIFY(editor.isDefault());
    QVERIFY(editor.hasChanged());
    QCOMPARE(editor.value(QStringLiteral("ThumbnailSize")).toInt(), 250);
    QCOMPARE(KonsoleSettings::thumbnailSize(), 300);

    // Cancel drops the edits.
    editor.reset();
    QVERIFY(!editor.hasChanged());
    QCOMPARE(editor.value(QStringLiteral("ThumbnailSize")).toInt(), 300);
    QCOMPARE(editor.value(QStringLiteral("EnableThumbnails")).toBool(), false);
}

void SettingsEditorTest::testChoice()
{
    SettingsEditor editor(KonsoleSettings::self(), nullptr);
    QCOMPARE(editor.choice(QStringLiteral("TabBarPosition")), QStringLiteral("Top"));
    editor.setChoice(QStringLiteral("TabBarPosition"), QStringLiteral("Left"));
    QCOMPARE(editor.choice(QStringLiteral("TabBarPosition")), QStringLiteral("Left"));
    QCOMPARE(editor.value(QStringLiteral("TabBarPosition")).toInt(), int(KonsoleSettings::EnumTabBarPosition::Left));
    editor.apply();
    QCOMPARE(KonsoleSettings::tabBarPosition(), int(KonsoleSettings::EnumTabBarPosition::Left));
}

void SettingsEditorTest::testUrlFromPath()
{
    // KUrlRequester's text is a path; the setting is an URL.
    SettingsEditor editor(KonsoleSettings::self(), nullptr);
    const QString name = QStringLiteral("scrollbackUseSpecifiedLocationDirectory");
    editor.setValue(name, QStringLiteral("/tmp/scrollback"));
    QCOMPARE(editor.value(name).toUrl(), QUrl::fromLocalFile(QStringLiteral("/tmp/scrollback")));
    QCOMPARE(editor.path(name), QStringLiteral("/tmp/scrollback"));
    editor.setValue(name, QString());
    QVERIFY(!editor.hasChanged());
}

QTEST_GUILESS_MAIN(SettingsEditorTest)

#include "SettingsEditorTest.moc"
