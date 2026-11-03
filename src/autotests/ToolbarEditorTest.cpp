/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The QML Configure Toolbars dialog edits the toolbars as KEditToolBar does: its lists are
// the toolbar editor's, its buttons change the session toolbar and the change survives
// saving and the rebuild, Defaults resets it once confirmed, and New Tab and Split View
// stay first outside the XML.

#include "TestTheme.h"

#include "../MainWindow.h"
#include "../profile/ProfileManager.h"
#include "../quick/WindowController.h"

#include <KActionCollection>
#include <KToolBar>
#include <KToolBarEditor>
#include <KXMLGUIFactory>

#include <QAbstractItemModel>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QTimer>

using namespace Konsole;

class ToolbarEditorTest : public QObject {
    Q_OBJECT

public:
    static void initMain()
    {
        TestTheme::setUp();
    }

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testListsAreTheEditors();
    void testButtonsEditTheToolBar();
    void testChangeText();
    void testOkAndApply();
    void testDefaults();
    void testOneDialogPerWindow();

private:
    QQuickWindow* openDialog();
    // The toolbar whose actions include edit_copy (sessionui.rc's), chosen in the dialog.
    void chooseSessionToolBar(QQuickWindow* dialog);
    static KToolBarEditor* editorOf(QQuickWindow* dialog);
    static QQuickWindow* dialogTitled(const QString& title);
    static QQuickItem* item(QQuickWindow* dialog, const QString& objectName);
    static QQuickItem* button(QQuickWindow* dialog, const QString& text);
    static QStringList names(const QAbstractItemModel* model);
    static QStringList shownToolBar(MainWindow* window);
    static void click(QQuickItem* control);

    QPointer<MainWindow> _window;
    QPointer<WindowController> _controller;
};

void ToolbarEditorTest::initTestCase()
{
    QCoreApplication::setApplicationName(QStringLiteral("konsole"));
}

void ToolbarEditorTest::init()
{
    _window = new MainWindow();
    _controller = new WindowController(_window);
    QVERIFY(_controller->qmlWindow());
    _window->createSession(ProfileManager::instance()->defaultProfile(), QDir::tempPath());
    _window->resize(800, 500);
    _window->show();
    QVERIFY(QTest::qWaitForWindowExposed(_controller->qmlWindow()));
}

void ToolbarEditorTest::cleanup()
{
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* window : windows) {
        if (window->isVisible() && qobject_cast<QQuickWindow*>(window) && window != _controller->qmlWindow()) {
            window->close();
        }
    }
    // Every test starts from the installed toolbars.
    for (KXMLGUIClient* client : _window->guiFactory()->clients()) {
        if (!client->localXMLFile().isEmpty()) {
            QFile::remove(client->localXMLFile());
        }
    }
    delete _window;
}

QQuickWindow* ToolbarEditorTest::openDialog()
{
    _window->actionCollection()->action(QStringLiteral("options_configure_toolbars"))->trigger();
    QQuickWindow* dialog = nullptr;
    [&] {
        QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Configure Toolbars"))));
    }();
    return dialog;
}

void ToolbarEditorTest::chooseSessionToolBar(QQuickWindow* dialog)
{
    KToolBarEditor* editor = editorOf(dialog);
    for (int i = 0; i < editor->toolBars().size(); ++i) {
        editor->setCurrentToolBar(i);
        if (names(editor->currentActions()).contains(QStringLiteral("edit_copy"))) {
            return;
        }
    }
    QFAIL("no toolbar with edit_copy");
}

KToolBarEditor* ToolbarEditorTest::editorOf(QQuickWindow* dialog)
{
    return dialog->property("editor").value<KToolBarEditor*>();
}

QQuickWindow* ToolbarEditorTest::dialogTitled(const QString& title)
{
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* window : windows) {
        if (auto* dialog = qobject_cast<QQuickWindow*>(window); dialog && dialog->isVisible() && dialog->title() == title) {
            return dialog;
        }
    }
    return nullptr;
}

QQuickItem* ToolbarEditorTest::item(QQuickWindow* dialog, const QString& objectName)
{
    // Delegates and inline components have no QObject parent: walk the items.
    QList<QQuickItem*> items = {dialog->contentItem()};
    while (!items.isEmpty()) {
        QQuickItem* candidate = items.takeFirst();
        if (candidate->objectName() == objectName) {
            return candidate;
        }
        items += candidate->childItems();
    }
    return nullptr;
}

QQuickItem* ToolbarEditorTest::button(QQuickWindow* dialog, const QString& text)
{
    QList<QQuickItem*> items = {dialog->contentItem()};
    while (!items.isEmpty()) {
        QQuickItem* candidate = items.takeFirst();
        if (candidate->inherits("QQuickAbstractButton") && candidate->property("text").toString().remove(u'&') == text) {
            return candidate;
        }
        items += candidate->childItems();
    }
    return nullptr;
}

QStringList ToolbarEditorTest::names(const QAbstractItemModel* model)
{
    QStringList values;
    for (int row = 0; row < model->rowCount(); ++row) {
        values.append(model->index(row, 0).data(KToolBarEditor::NameRole).toString());
    }
    return values;
}

QStringList ToolbarEditorTest::shownToolBar(MainWindow* window)
{
    QStringList shown;
    const auto actions = window->findChild<KToolBar*>(QStringLiteral("mainToolBar"))->actions();
    for (QAction* action : actions) {
        if (!action->objectName().isEmpty()) {
            shown.append(action->objectName());
        }
    }
    return shown;
}

void ToolbarEditorTest::click(QQuickItem* control)
{
    QVERIFY(control);
    QVERIFY(control->isEnabled());
    QMetaObject::invokeMethod(control, "click");
}

void ToolbarEditorTest::testListsAreTheEditors()
{
    QQuickWindow* dialog = openDialog();
    QVERIFY(dialog);
    KToolBarEditor* editor = editorOf(dialog);
    QVERIFY(editor);
    // A new editor of the window's factory lists the same toolbars and actions; New Tab and
    // Split View are outside the XML.
    KToolBarEditor fresh;
    fresh.load(_window->guiFactory());
    QCOMPARE(editor->toolBars(), fresh.toolBars());
    QCOMPARE(item(dialog, QStringLiteral("toolBarCombo"))->property("count").toInt(), fresh.toolBars().size());
    for (int i = 0; i < fresh.toolBars().size(); ++i) {
        editor->setCurrentToolBar(i);
        fresh.setCurrentToolBar(i);
        QCOMPARE(names(editor->currentActions()), names(fresh.currentActions()));
        QVERIFY(!names(editor->currentActions()).contains(QStringLiteral("new-tab")));
        QVERIFY(!names(editor->currentActions()).contains(QStringLiteral("split-view")));
        QCOMPARE(item(dialog, QStringLiteral("currentList"))->property("count").toInt(), fresh.currentActions()->rowCount());
        QCOMPARE(item(dialog, QStringLiteral("availableList"))->property("count").toInt(), fresh.availableActions()->rowCount());
    }
}

void ToolbarEditorTest::testButtonsEditTheToolBar()
{
    QQuickWindow* dialog = openDialog();
    QVERIFY(dialog);
    chooseSessionToolBar(dialog);
    KToolBarEditor* editor = editorOf(dialog);
    QQuickItem* available = item(dialog, QStringLiteral("availableList"));
    QQuickItem* current = item(dialog, QStringLiteral("currentList"));

    // Nothing selected: the arrows are off.
    for (const QString& name : {QStringLiteral("insertButton"), QStringLiteral("removeButton"), QStringLiteral("upButton"), QStringLiteral("downButton")}) {
        QVERIFY2(!item(dialog, name)->isEnabled(), qPrintable(name));
    }

    // Insert after the selected element: it is then selected.
    // The first action after the separator and the spacer.
    const int availableRow = 2;
    QVERIFY(editor->availableActions()->rowCount() > availableRow);
    const QString added = names(editor->availableActions()).at(availableRow);
    available->setProperty("currentIndex", availableRow);
    const int after = int(names(editor->currentActions()).indexOf(QStringLiteral("edit_copy")));
    current->setProperty("currentIndex", after);
    click(item(dialog, QStringLiteral("insertButton")));
    QCOMPARE(names(editor->currentActions()).value(after + 1), added);
    QCOMPARE(current->property("currentIndex").toInt(), after + 1);

    // Up and down move it, still selected.
    click(item(dialog, QStringLiteral("upButton")));
    QCOMPARE(names(editor->currentActions()).value(after), added);
    QCOMPARE(current->property("currentIndex").toInt(), after);
    click(item(dialog, QStringLiteral("downButton")));
    QCOMPARE(current->property("currentIndex").toInt(), after + 1);

    // Applied: the window's toolbar shows it after edit_copy, New Tab and Split View first.
    click(button(dialog, QStringLiteral("Apply")));
    QVERIFY(!button(dialog, QStringLiteral("Apply"))->isEnabled());
    QStringList shown = shownToolBar(_window);
    QCOMPARE(shown.mid(0, 2), (QStringList{QStringLiteral("new-tab"), QStringLiteral("split-view")}));
    QCOMPARE(shown.indexOf(added), shown.indexOf(QStringLiteral("edit_copy")) + 1);

    // Removed again, and gone from the toolbar after Apply.
    chooseSessionToolBar(dialog);
    current->setProperty("currentIndex", int(names(editor->currentActions()).indexOf(added)));
    click(item(dialog, QStringLiteral("removeButton")));
    QVERIFY(!names(editor->currentActions()).contains(added));
    click(button(dialog, QStringLiteral("Apply")));
    shown = shownToolBar(_window);
    QVERIFY(!shown.contains(added));
    QCOMPARE(shown.mid(0, 2), (QStringList{QStringLiteral("new-tab"), QStringLiteral("split-view")}));
}

void ToolbarEditorTest::testChangeText()
{
    QQuickWindow* dialog = openDialog();
    QVERIFY(dialog);
    chooseSessionToolBar(dialog);
    KToolBarEditor* editor = editorOf(dialog);
    QQuickItem* current = item(dialog, QStringLiteral("currentList"));
    QQuickItem* changeText = item(dialog, QStringLiteral("changeTextButton"));
    QVERIFY(!changeText->isEnabled());

    // Only actions: the spacer can't be renamed.
    current->setProperty("currentIndex", int(names(editor->currentActions()).indexOf(QStringLiteral("spacer_0"))));
    QVERIFY(!changeText->isEnabled());
    const int row = int(names(editor->currentActions()).indexOf(QStringLiteral("edit_copy")));
    current->setProperty("currentIndex", row);
    click(changeText);

    QQuickWindow* textDialog = nullptr;
    QTRY_VERIFY((textDialog = dialogTitled(QStringLiteral("Change Text"))));
    QCOMPARE(textDialog->transientParent(), dialog);
    QCOMPARE(textDialog->modality(), Qt::ApplicationModal);
    QQuickItem* edit = item(textDialog, QStringLiteral("iconTextEdit"));
    QCOMPARE(edit->property("text").toString(), editor->currentActions()->index(row, 0).data(Qt::DisplayRole).toString());
    // Never empty.
    edit->setProperty("text", QStringLiteral("   "));
    QVERIFY(!button(textDialog, QStringLiteral("OK"))->isEnabled());
    edit->setProperty("text", QStringLiteral("Duplicate"));
    click(item(textDialog, QStringLiteral("hiddenCheck")));
    click(button(textDialog, QStringLiteral("OK")));
    QTRY_VERIFY(!dialogTitled(QStringLiteral("Change Text")));

    click(button(dialog, QStringLiteral("OK")));
    QTRY_VERIFY(!dialogTitled(QStringLiteral("Configure Toolbars")));
    QAction* copy = nullptr;
    for (KXMLGUIClient* client : _window->guiFactory()->clients()) {
        if ((copy = client->actionCollection()->action(QStringLiteral("edit_copy")))) {
            break;
        }
    }
    QVERIFY(copy);
    QCOMPARE(copy->iconText(), QStringLiteral("Duplicate"));
    QCOMPARE(copy->priority(), QAction::LowPriority);
}

void ToolbarEditorTest::testOkAndApply()
{
    // OK without a change only closes: nothing saved.
    QQuickWindow* dialog = openDialog();
    QVERIFY(dialog);
    QVERIFY(!button(dialog, QStringLiteral("Apply"))->isEnabled());
    click(button(dialog, QStringLiteral("OK")));
    QTRY_VERIFY(!dialogTitled(QStringLiteral("Configure Toolbars")));
    for (KXMLGUIClient* client : _window->guiFactory()->clients()) {
        QVERIFY2(client->localXMLFile().isEmpty() || !QFile::exists(client->localXMLFile()), qPrintable(client->localXMLFile()));
    }

    // A change turns Apply on; Apply saves and stays open.
    dialog = openDialog();
    QVERIFY(dialog);
    chooseSessionToolBar(dialog);
    KToolBarEditor* editor = editorOf(dialog);
    item(dialog, QStringLiteral("currentList"))->setProperty("currentIndex", 0);
    click(item(dialog, QStringLiteral("removeButton")));
    QVERIFY(button(dialog, QStringLiteral("Apply"))->isEnabled());
    const QStringList edited = names(editor->currentActions());
    click(button(dialog, QStringLiteral("Apply")));
    QVERIFY(dialog->isVisible());
    QVERIFY(!button(dialog, QStringLiteral("Apply"))->isEnabled());

    // Cancel keeps what Apply saved.
    click(button(dialog, QStringLiteral("Cancel")));
    QTRY_VERIFY(!dialogTitled(QStringLiteral("Configure Toolbars")));
    KToolBarEditor reloaded;
    reloaded.load(_window->guiFactory());
    for (int i = 0; i < reloaded.toolBars().size(); ++i) {
        reloaded.setCurrentToolBar(i);
        if (names(reloaded.currentActions()).contains(QStringLiteral("edit_copy"))) {
            QCOMPARE(names(reloaded.currentActions()), edited);
        }
    }
}

void ToolbarEditorTest::testDefaults()
{
    QQuickWindow* dialog = openDialog();
    QVERIFY(dialog);
    chooseSessionToolBar(dialog);
    KToolBarEditor* editor = editorOf(dialog);
    const QStringList installed = names(editor->currentActions());
    item(dialog, QStringLiteral("currentList"))->setProperty("currentIndex", 0);
    click(item(dialog, QStringLiteral("removeButton")));
    click(button(dialog, QStringLiteral("Apply")));

    // Asked first (KMessageBox runs an event loop before it shows the box: polled), transient
    // for the dialog; Cancel keeps the toolbar.
    const auto answer = [this](const QString& choice, QWindow** parent) {
        auto* poll = new QTimer(this);
        connect(poll, &QTimer::timeout, this, [poll, choice, parent] {
            const auto widgets = QApplication::topLevelWidgets();
            for (QWidget* widget : widgets) {
                if (widget->objectName() == QLatin1String("warningYesNo") && widget->isVisible()) {
                    *parent = widget->windowHandle()->transientParent();
                    auto* box = widget->findChild<QDialogButtonBox*>();
                    box->button(choice == QLatin1String("Yes") ? QDialogButtonBox::Yes : QDialogButtonBox::No)->click();
                    poll->deleteLater();
                    return;
                }
            }
        });
        poll->start(50);
    };
    QWindow* parent = nullptr;
    answer(QStringLiteral("No"), &parent);
    QMetaObject::invokeMethod(button(dialog, QStringLiteral("Defaults")), "click");
    QTRY_COMPARE(parent, dialog);
    chooseSessionToolBar(dialog);
    QVERIFY(names(editor->currentActions()) != installed);

    // Reset: the local files are gone and the toolbar is the installed one.
    parent = nullptr;
    answer(QStringLiteral("Yes"), &parent);
    QMetaObject::invokeMethod(button(dialog, QStringLiteral("Defaults")), "click");
    QTRY_COMPARE(parent, dialog);
    for (KXMLGUIClient* client : _window->guiFactory()->clients()) {
        QVERIFY2(client->localXMLFile().isEmpty() || !QFile::exists(client->localXMLFile()), qPrintable(client->localXMLFile()));
    }
    chooseSessionToolBar(dialog);
    QCOMPARE(names(editor->currentActions()), installed);
    QCOMPARE(shownToolBar(_window).mid(0, 2), (QStringList{QStringLiteral("new-tab"), QStringLiteral("split-view")}));
}

void ToolbarEditorTest::testOneDialogPerWindow()
{
    QQuickWindow* dialog = openDialog();
    QVERIFY(dialog);
    QCOMPARE(dialog->transientParent(), _controller->qmlWindow());
    QCOMPARE(dialog->modality(), Qt::NonModal);
    QVERIFY(KToolBar::toolBarsEditable());
    QCOMPARE(openDialog(), dialog);
    QPointer<QQuickWindow> guard(dialog);
    click(button(dialog, QStringLiteral("Cancel")));
    QTRY_VERIFY(guard.isNull());
    QVERIFY(!KToolBar::toolBarsEditable());
}

QTEST_MAIN(ToolbarEditorTest)

#include "ToolbarEditorTest.moc"
