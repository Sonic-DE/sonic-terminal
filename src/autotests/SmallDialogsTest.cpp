/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The QML Rename Tab, History Size, Copy Input and text input dialogs do what the widget
// dialogs did: opened from their actions and buttons, transient for the window they were
// opened from, their results reaching the session or profile.

#include "TestTheme.h"

#include "../Enumeration.h"
#include "../MainWindow.h"
#include "../ViewManager.h"
#include "../history/HistoryType.h"
#include "../history/HistoryTypeNone.h"
#include "../history/compact/CompactHistoryType.h"
#include "../profile/ProfileManager.h"
#include "../quick/ProfileEditor.h"
#include "../quick/WindowController.h"
#include "../session/Session.h"
#include "../session/SessionController.h"

#include <KActionCollection>
#include <KXMLGUIFactory>

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

using namespace Konsole;

class SmallDialogsTest : public QObject {
    Q_OBJECT

public:
    static void initMain()
    {
        TestTheme::setUp();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QCoreApplication::setApplicationName(QStringLiteral("konsole"));
    }
    void init();
    void cleanup();
    void testRenameTab();
    void testRenameTabCancelled();
    void testTitleFormatMenu();
    void testHistorySize();
    void testCopyInput();
    void testEnvironmentEditor();
    void testCustomTextEditorCommand();

private:
    void trigger(const QString& action);
    static QQuickWindow* dialogTitled(const QString& title);
    static QQuickItem* item(QQuickWindow* dialog, const QString& objectName);
    static void clickButton(QQuickWindow* dialog, const QString& text);

    QPointer<MainWindow> _window;
    QPointer<WindowController> _controller;
};

void SmallDialogsTest::init()
{
    _window = new MainWindow();
    _controller = new WindowController(_window);
    QVERIFY(_controller->qmlWindow());
    _window->createSession(ProfileManager::instance()->defaultProfile(), QDir::tempPath());
    _window->resize(800, 500);
    _window->show();
    QVERIFY(QTest::qWaitForWindowExposed(_controller->qmlWindow()));
    _controller->qmlWindow()->requestActivate();
    QTRY_VERIFY(_controller->qmlWindow()->isActive());
}

void SmallDialogsTest::cleanup()
{
    // A failed test's dialog would stay modal over the next test's window.
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* window : windows) {
        if (window->isVisible() && qobject_cast<QQuickWindow*>(window) && window != _controller->qmlWindow()) {
            window->close();
        }
    }
    delete _window;
}

void SmallDialogsTest::trigger(const QString& action)
{
    const auto clients = _window->guiFactory()->clients();
    for (KXMLGUIClient* client : clients) {
        if (QAction* found = client->actionCollection()->action(action)) {
            found->trigger();
            return;
        }
    }
    QFAIL(qPrintable(QStringLiteral("no action ") + action));
}

QQuickWindow* SmallDialogsTest::dialogTitled(const QString& title)
{
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* window : windows) {
        if (auto* dialog = qobject_cast<QQuickWindow*>(window); dialog && dialog->isVisible() && dialog->title() == title) {
            return dialog;
        }
    }
    return nullptr;
}

QQuickItem* SmallDialogsTest::item(QQuickWindow* dialog, const QString& objectName)
{
    return dialog->findChild<QQuickItem*>(objectName);
}

void SmallDialogsTest::clickButton(QQuickWindow* dialog, const QString& text)
{
    QList<QQuickItem*> items = {dialog->contentItem()};
    while (!items.isEmpty()) {
        QQuickItem* candidate = items.takeFirst();
        if (candidate->inherits("QQuickAbstractButton") && candidate->property("text").toString().remove(u'&') == text) {
            QMetaObject::invokeMethod(candidate, "click");
            return;
        }
        items += candidate->childItems();
    }
    QFAIL(qPrintable(QStringLiteral("no button ") + text));
}

void SmallDialogsTest::testRenameTab()
{
    Session* session = _window->viewManager()->activeViewController()->session();
    session->setTabTitleFormat(Session::LocalTabTitle, QStringLiteral("local %n"));
    session->setTabTitleFormat(Session::RemoteTabTitle, QStringLiteral("remote %h"));
    trigger(QStringLiteral("rename-session"));
    QQuickWindow* dialog = nullptr;
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Tab Properties"))));
    QCOMPARE(dialog->transientParent(), _controller->qmlWindow());
    QCOMPARE(dialog->modality(), Qt::ApplicationModal);
    QQuickItem* local = item(dialog, QStringLiteral("tabTitleEdit"));
    QQuickItem* remote = item(dialog, QStringLiteral("remoteTabTitleEdit"));
    QVERIFY(local && remote);
    QCOMPARE(local->property("text").toString(), QStringLiteral("local %n"));
    QCOMPARE(remote->property("text").toString(), QStringLiteral("remote %h"));
    // A local session: the local format has the focus. No window manager activates the dialog here.
    dialog->requestActivate();
    QTRY_VERIFY(local->hasActiveFocus());

    local->setProperty("text", QStringLiteral("renamed %d"));
    remote->setProperty("text", QStringLiteral("remote %u"));
    // KColorCombo's red: after "Custom…", "None" and the 12 colours before it.
    QQuickItem* color = item(dialog, QStringLiteral("tabColorCombo"));
    QQuickItem* activity = item(dialog, QStringLiteral("tabActivityColorCombo"));
    QVERIFY(color && activity);
    color->setProperty("currentIndex", 14);
    QMetaObject::invokeMethod(color, "activated", Q_ARG(int, 14));
    activity->setProperty("currentIndex", 19);
    QMetaObject::invokeMethod(activity, "activated", Q_ARG(int, 19));
    QSignalSpy renamed(_window->viewManager()->activeViewController(), &SessionController::tabRenamedByUser);
    clickButton(dialog, QStringLiteral("OK"));

    QCOMPARE(session->tabTitleFormat(Session::LocalTabTitle), QStringLiteral("renamed %d"));
    QCOMPARE(session->tabTitleFormat(Session::RemoteTabTitle), QStringLiteral("remote %u"));
    QCOMPARE(session->color(), QColor(255, 0, 0));
    QCOMPARE(session->activityColor(), QColor(0, 0, 255));
    QCOMPARE(renamed.count(), 2);

    // Opened again: the colours chosen are the entries shown.
    trigger(QStringLiteral("rename-session"));
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Tab Properties"))));
    QCOMPARE(item(dialog, QStringLiteral("tabColorCombo"))->property("currentIndex").toInt(), 14);
    QCOMPARE(item(dialog, QStringLiteral("tabActivityColorCombo"))->property("currentIndex").toInt(), 19);
    clickButton(dialog, QStringLiteral("Cancel"));
}

void SmallDialogsTest::testRenameTabCancelled()
{
    Session* session = _window->viewManager()->activeViewController()->session();
    const QString before = session->tabTitleFormat(Session::LocalTabTitle);
    trigger(QStringLiteral("rename-session"));
    QQuickWindow* dialog = nullptr;
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Tab Properties"))));
    item(dialog, QStringLiteral("tabTitleEdit"))->setProperty("text", QStringLiteral("not kept"));
    clickButton(dialog, QStringLiteral("Cancel"));
    QCOMPARE(session->tabTitleFormat(Session::LocalTabTitle), before);
    // No colour chosen: "None" (the widget's transparent base colour), as before.
    trigger(QStringLiteral("rename-session"));
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Tab Properties"))));
    QCOMPARE(item(dialog, QStringLiteral("tabColorCombo"))->property("currentIndex").toInt(), 1);
    clickButton(dialog, QStringLiteral("OK"));
    QCOMPARE(session->color().alpha(), 0);
}

void SmallDialogsTest::testTitleFormatMenu()
{
    trigger(QStringLiteral("rename-session"));
    QQuickWindow* dialog = nullptr;
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Tab Properties"))));
    dialog->requestActivate();
    QTRY_VERIFY(dialog->isActive());
    QQuickItem* local = item(dialog, QStringLiteral("tabTitleEdit"));
    QQuickItem* remote = item(dialog, QStringLiteral("remoteTabTitleEdit"));
    local->setProperty("text", QStringLiteral("a"));
    remote->setProperty("text", QStringLiteral("b"));
    // TabTitleFormatButton's menus, below their buttons: the local and the remote elements.
    const QList<std::tuple<QString, QQuickItem*, QString, QString>> cases = {
        {QStringLiteral("tabTitleFormatButton"), local, QStringLiteral("Program Name: %n"), QStringLiteral("a%n")},
        {QStringLiteral("remoteTabTitleFormatButton"), remote, QStringLiteral("User Name: %u"), QStringLiteral("b%u")},
    };
    for (const auto& [button, edit, first, result] : cases) {
        QQuickItem* insert = item(dialog, button);
        QVERIFY(insert);
        edit->setProperty("cursorPosition", 1);
        // The menu runs its own event loop until closed, so it's checked and chosen from a timer.
        int top = -1;
        QString firstText;
        QTimer poll;
        connect(&poll, &QTimer::timeout, this, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu || !menu->isVisible()) {
                return;
            }
            poll.stop();
            top = menu->geometry().top();
            firstText = menu->actions().constFirst()->text();
            menu->setActiveAction(menu->actions().constFirst());
            QTest::keyClick(menu, Qt::Key_Return);
        });
        poll.start(50);
        QMetaObject::invokeMethod(insert, "click");
        QCOMPARE(top, qRound(insert->mapToGlobal(QPointF(0, insert->height())).y()));
        QCOMPARE(firstText, first);
        QTRY_COMPARE(edit->property("text").toString(), result);
        QTRY_VERIFY(edit->hasActiveFocus());
    }
    clickButton(dialog, QStringLiteral("Cancel"));
}

void SmallDialogsTest::testHistorySize()
{
    Session* session = _window->viewManager()->activeViewController()->session();
    // From no history: a compact history replacing a compact one keeps reporting the old size.
    session->setHistoryType(HistoryTypeNone());
    session->setHistoryType(CompactHistoryType(2000));
    QCOMPARE(session->historyType().maximumLineCount(), 2000);
    trigger(QStringLiteral("adjust-history"));
    QQuickWindow* dialog = nullptr;
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Adjust Scrollback"))));
    QCOMPARE(dialog->transientParent(), _controller->qmlWindow());
    QCOMPARE(dialog->modality(), Qt::ApplicationModal);
    QQuickItem* fixed = item(dialog, QStringLiteral("fixedSizeHistoryButton"));
    QQuickItem* spinner = item(dialog, QStringLiteral("historyLineSpinner"));
    QVERIFY(fixed && spinner);
    QVERIFY(fixed->property("checked").toBool());
    QCOMPARE(spinner->property("value").toInt(), 2000);
    QCOMPARE(spinner->property("stepSize").toInt(), 200);
    QVERIFY(spinner->isEnabled());

    // Unlimited: the line count can't be edited.
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("unlimitedHistoryButton")), "click");
    QVERIFY(!spinner->isEnabled());
    clickButton(dialog, QStringLiteral("OK"));
    QVERIFY(session->historyType().isUnlimited());

    trigger(QStringLiteral("adjust-history"));
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Adjust Scrollback"))));
    QVERIFY(item(dialog, QStringLiteral("unlimitedHistoryButton"))->property("checked").toBool());
    dialog->requestActivate();
    QTRY_VERIFY(dialog->isActive());
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("fixedSizeHistoryButton")), "click");
    spinner = item(dialog, QStringLiteral("historyLineSpinner"));
    QVERIFY(spinner->isEnabled());
    // HistorySizeWidget's focus proxy: choosing a fixed size edits the line count.
    QTRY_VERIFY(spinner->hasActiveFocus());
    spinner->setProperty("value", 500);
    clickButton(dialog, QStringLiteral("OK"));
    QVERIFY(session->historyType().isEnabled() && !session->historyType().isUnlimited());
    QCOMPARE(session->historyType().maximumLineCount(), 500);

    trigger(QStringLiteral("adjust-history"));
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Adjust Scrollback"))));
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("noHistoryButton")), "click");
    clickButton(dialog, QStringLiteral("OK"));
    QVERIFY(!session->historyType().isEnabled());
}

void SmallDialogsTest::testCopyInput()
{
    Session* first = _window->viewManager()->activeViewController()->session();
    _window->createSession(ProfileManager::instance()->defaultProfile(), QDir::tempPath());
    _window->createSession(ProfileManager::instance()->defaultProfile(), QDir::tempPath());
    SessionController* master = _window->viewManager()->activeViewController();
    QVERIFY(master->session() != first);
    first->setTitle(Session::DisplayedTitleRole, QStringLiteral("third"));

    QSignalSpy changed(master, &SessionController::copyInputChanged);
    trigger(QStringLiteral("copy-input-to-selected-tabs"));
    QQuickWindow* dialog = nullptr;
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Copy Input"))));
    QCOMPARE(dialog->transientParent(), _controller->qmlWindow());
    auto* model = dialog->property("sessions").value<QAbstractItemModel*>();
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 3);
    const QHash<int, QByteArray> roles = model->roleNames();
    const int checked = roles.key("checked");
    const int checkable = roles.key("checkable");
    const int title = roles.key("title");
    // The master session: checked, and can't be unchecked.
    int masterRow = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (!model->index(row, 0).data(checkable).toBool()) {
            masterRow = row;
        }
    }
    QVERIFY(masterRow >= 0);
    QVERIFY(model->index(masterRow, 0).data(checked).toBool());

    // The filter, then Select All: only the rows shown.
    QQuickItem* filter = item(dialog, QStringLiteral("filterEdit"));
    QVERIFY(filter);
    filter->setProperty("text", QStringLiteral("THI"));
    QTRY_COMPARE(model->rowCount(), 1);
    QCOMPARE(model->index(0, 0).data(title).toString(), QStringLiteral("third"));
    clickButton(dialog, QStringLiteral("Select All"));
    filter->setProperty("text", QString());
    QTRY_COMPARE(model->rowCount(), 3);
    for (int row = 0; row < 3; ++row) {
        const QString name = model->index(row, 0).data(title).toString();
        QCOMPARE(model->index(row, 0).data(checked).toBool(), row == masterRow || name == QLatin1String("third"));
    }
    // Deselect All leaves the master checked.
    clickButton(dialog, QStringLiteral("Deselect All"));
    QVERIFY(model->index(masterRow, 0).data(checked).toBool());
    clickButton(dialog, QStringLiteral("Select All"));
    clickButton(dialog, QStringLiteral("OK"));
    QCOMPARE(changed.count(), 1);

    // Opened again: the sessions chosen are checked.
    trigger(QStringLiteral("copy-input-to-selected-tabs"));
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Copy Input"))));
    model = dialog->property("sessions").value<QAbstractItemModel*>();
    for (int row = 0; row < model->rowCount(); ++row) {
        QVERIFY(model->index(row, 0).data(checked).toBool());
    }
    clickButton(dialog, QStringLiteral("Cancel"));
    QCOMPARE(changed.count(), 1);
}

void SmallDialogsTest::testEnvironmentEditor()
{
    Profile::Ptr profile(new Profile(ProfileManager::instance()->defaultProfile()));
    profile->setProperty(Profile::Name, QStringLiteral("SmallDialogsEnvironment"));
    profile->setProperty(Profile::Environment, QStringList{QStringLiteral("A=1"), QStringLiteral("B=2")});
    ProfileManager::instance()->addProfile(profile);
    QPointer<QQuickWindow> editor = ProfileEditor::open(profile, ProfileEditor::ExistingProfile, nullptr);
    QVERIFY(editor);
    QVERIFY(QTest::qWaitForWindowExposed(editor));

    const auto open = [&editor]() -> QQuickWindow* {
        QMetaObject::invokeMethod(item(editor, QStringLiteral("environmentEditButton")), "click");
        QQuickWindow* dialog = nullptr;
        [&] {
            QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Edit Environment"))));
        }();
        return dialog;
    };
    QQuickWindow* dialog = open();
    QVERIFY(dialog);
    QCOMPARE(dialog->transientParent(), editor.data());
    QCOMPARE(dialog->modality(), Qt::ApplicationModal);
    QCOMPARE(dialog->property("labelText").toString(), QStringLiteral("One environment variable per line"));
    QQuickItem* area = item(dialog, QStringLiteral("textValueArea"));
    QVERIFY(area && area->isVisible());
    QCOMPARE(area->property("text").toString(), QStringLiteral("A=1\nB=2"));
    area->setProperty("text", QStringLiteral("C=3\nD=4"));
    clickButton(dialog, QStringLiteral("OK"));

    // The edited environment, not yet applied, is what the editor shows next.
    dialog = open();
    QVERIFY(dialog);
    QCOMPARE(item(dialog, QStringLiteral("textValueArea"))->property("text").toString(), QStringLiteral("C=3\nD=4"));
    clickButton(dialog, QStringLiteral("Cancel"));
    clickButton(editor, QStringLiteral("Apply"));
    QCOMPARE(profile->environment(), (QStringList{QStringLiteral("C=3"), QStringLiteral("D=4")}));
    editor->close();
}

void SmallDialogsTest::testCustomTextEditorCommand()
{
    Profile::Ptr profile(new Profile(ProfileManager::instance()->defaultProfile()));
    profile->setProperty(Profile::Name, QStringLiteral("SmallDialogsEditor"));
    profile->setProperty(Profile::TextEditorCmd, Enum::CustomTextEditor);
    profile->setProperty(Profile::TextEditorCmdCustom, QStringLiteral("editor PATH:LINE"));
    ProfileManager::instance()->addProfile(profile);
    QPointer<QQuickWindow> editor = ProfileEditor::open(profile, ProfileEditor::ExistingProfile, nullptr);
    QVERIFY(editor);
    QVERIFY(QTest::qWaitForWindowExposed(editor));

    QQuickItem* button = item(editor, QStringLiteral("textEditorCustomBtn"));
    QVERIFY(button && button->isEnabled());
    QMetaObject::invokeMethod(button, "click");
    QQuickWindow* dialog = nullptr;
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Text Editor Custom Command"))));
    QCOMPARE(dialog->transientParent(), editor.data());
    // As the widget dialog: shown, not modal.
    QCOMPARE(dialog->modality(), Qt::NonModal);
    QVERIFY(dialog->property("labelText").toString().startsWith(QLatin1String("The format is e.g. 'editorExec PATH:LINE:COLUMN'")));
    QQuickItem* field = item(dialog, QStringLiteral("textValueEdit"));
    QVERIFY(field && field->isVisible());
    QCOMPARE(field->property("text").toString(), QStringLiteral("editor PATH:LINE"));
    field->setProperty("text", QStringLiteral("other +LINE PATH"));
    // Return in the line edit accepts, as QInputDialog's default button.
    dialog->requestActivate();
    QTRY_VERIFY(field->hasActiveFocus());
    QTest::keyClick(dialog, Qt::Key_Return);
    QTRY_VERIFY(!dialogTitled(QStringLiteral("Text Editor Custom Command")));
    clickButton(editor, QStringLiteral("Apply"));
    QCOMPARE(profile->customTextEditorCmd(), QStringLiteral("other +LINE PATH"));
    editor->close();
}

QTEST_MAIN(SmallDialogsTest)

#include "SmallDialogsTest.moc"
