/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The QML Configure dialog does what the widget one did: every setting a kcfg_ widget
// edited has a control that stores it on Apply, Defaults, Cancel and OK behave as
// ConfigurationDialog's, one dialog per window, and the Profiles page's buttons act on
// the selected profile. The KPart's settings dialog is the same pages' QML one.

#include "TestTheme.h"

#include "../MainWindow.h"
#include "../profile/ProfileManager.h"
#include "../quick/ProfileEditor.h"
#include "../quick/ProfileListModel.h"
#include "../quick/WindowController.h"
#include "../settings/GeneralSettings.h"
#include "../settings/MemorySettings.h"
#include "../settings/TabBarSettings.h"
#include "../settings/TemporaryFilesSettings.h"
#include "../settings/ThumbnailsSettings.h"
#include "KonsoleSettings.h"

#include <KActionCollection>
#include <KParts/Part>
#include <KPluginFactory>
#include <KXMLGUIFactory>

#include <QApplication>
#include <QButtonGroup>
#include <QDir>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QVBoxLayout>

#include <memory>

using namespace Konsole;

class ConfigureDialogTest : public QObject {
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
    void testEverySettingStored();
    void testDefaultsAndCancel();
    void testOk();
    void testTabBarRules();
    void testOneDialogPerWindow();
    void testProfilesPage();
    void testPartDialog();
    void testEditCurrentProfile();

private:
    void trigger(const QString& action);
    QQuickWindow* openConfigure();
    static QQuickWindow* dialogTitled(const QString& title);
    static QList<QQuickWindow*> dialogsTitled(const QString& title);
    static QQuickItem* item(QQuickWindow* dialog, const QString& objectName, const QString& setting = QString());
    static QQuickItem* button(QQuickWindow* dialog, const QString& text);

    QPointer<MainWindow> _window;
    QPointer<WindowController> _controller;
};

void ConfigureDialogTest::initTestCase()
{
    QCoreApplication::setApplicationName(QStringLiteral("konsole"));
    // konsolepart from the build tree (TerminalInterfaceTest's search path).
    QString buildPath = QCoreApplication::libraryPaths().constLast();
    if (buildPath.endsWith(QStringLiteral("/autotests"))) {
        buildPath.chop(10);
    }
    QCoreApplication::addLibraryPath(buildPath);
}

void ConfigureDialogTest::init()
{
    KonsoleSettings::self()->setDefaults();
    KonsoleSettings::self()->save();
    _window = new MainWindow();
    _controller = new WindowController(_window);
    QVERIFY(_controller->qmlWindow());
    _window->createSession(ProfileManager::instance()->defaultProfile(), QDir::tempPath());
    _window->resize(800, 500);
    _window->show();
    QVERIFY(QTest::qWaitForWindowExposed(_controller->qmlWindow()));
}

void ConfigureDialogTest::cleanup()
{
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* window : windows) {
        if (window->isVisible() && qobject_cast<QQuickWindow*>(window) && window != _controller->qmlWindow()) {
            window->close();
        }
    }
    delete _window;
    KonsoleSettings::self()->setDefaults();
    KonsoleSettings::self()->save();
}

void ConfigureDialogTest::trigger(const QString& action)
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

QQuickWindow* ConfigureDialogTest::openConfigure()
{
    trigger(QStringLiteral("options_configure"));
    QQuickWindow* dialog = nullptr;
    [&] {
        QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Configure"))));
    }();
    return dialog;
}

QList<QQuickWindow*> ConfigureDialogTest::dialogsTitled(const QString& title)
{
    QList<QQuickWindow*> found;
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* window : windows) {
        if (auto* dialog = qobject_cast<QQuickWindow*>(window); dialog && dialog->isVisible() && dialog->title() == title) {
            found.append(dialog);
        }
    }
    return found;
}

QQuickWindow* ConfigureDialogTest::dialogTitled(const QString& title)
{
    return dialogsTitled(title).value(0);
}

QQuickItem* ConfigureDialogTest::item(QQuickWindow* dialog, const QString& objectName, const QString& setting)
{
    const auto items = dialog->findChildren<QQuickItem*>(objectName);
    for (QQuickItem* candidate : items) {
        if (setting.isEmpty() || candidate->property("setting").toString() == setting) {
            return candidate;
        }
    }
    return nullptr;
}

QQuickItem* ConfigureDialogTest::button(QQuickWindow* dialog, const QString& text)
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

void ConfigureDialogTest::testEverySettingStored()
{
    // The kcfg_ widgets and radio button groups of the widget pages (MainWindow's).
    QWidget pages;
    new GeneralSettings(&pages);
    new MemorySettings(&pages);
    new TabBarSettings(&pages);
    new TemporaryFilesSettings(&pages);
    new ThumbnailsSettings(&pages);
    QStringList widgets;
    for (QWidget* widget : pages.findChildren<QWidget*>()) {
        if (widget->objectName().startsWith(QStringLiteral("kcfg_"))) {
            widgets.append(widget->objectName().mid(5));
        }
    }
    QHash<QString, QStringList> groups;
    for (QButtonGroup* group : pages.findChildren<QButtonGroup*>()) {
        for (QAbstractButton* choice : group->buttons()) {
            groups[group->objectName().mid(5)].append(choice->objectName());
        }
    }
    QVERIFY(widgets.size() > 20);
    QCOMPARE(groups.size(), 7);

    QQuickWindow* dialog = openConfigure();
    QVERIFY(dialog);
    QHash<QString, QVariant> before;
    for (KConfigSkeletonItem* setting : KonsoleSettings::self()->items()) {
        before.insert(setting->name(), setting->property());
    }

    // Each control changed as a user would (signals the user's input emits).
    QStringList changed;
    for (const QString& name : std::as_const(widgets)) {
        QQuickItem* control = item(dialog, QStringLiteral("kcfg_") + name);
        QVERIFY2(control, qPrintable(QStringLiteral("no control for ") + name));
        if (name.startsWith(QStringLiteral("scrollbackUse")) && control->inherits("QQuickRadioButton")) {
            continue;
        }
        if (control->inherits("QQuickCheckBox")) {
            QMetaObject::invokeMethod(control, "click");
        } else if (control->inherits("QQuickSpinBox")) {
            control->setProperty("value", control->property("value").toInt() + 1);
            QMetaObject::invokeMethod(control, "valueModified");
        } else if (control->inherits("QQuickTextField")) {
            control->setProperty("text", QStringLiteral("/tmp/configure-test"));
            QMetaObject::invokeMethod(control, "textEdited");
        } else {
            QFAIL(qPrintable(QStringLiteral("unexpected control for ") + name));
        }
        changed.append(name);
    }
    // The three location settings are one choice.
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("kcfg_scrollbackUseCacheLocation")), "click");
    changed += {QStringLiteral("scrollbackUseSystemLocation"), QStringLiteral("scrollbackUseCacheLocation")};
    // Last: the tab bar hidden disables other choices.
    QStringList enums = groups.keys();
    enums.removeOne(QStringLiteral("TabBarVisibility"));
    enums.append(QStringLiteral("TabBarVisibility"));
    for (const QString& name : std::as_const(enums)) {
        QQuickItem* other = nullptr;
        for (const QString& choice : groups.value(name)) {
            QQuickItem* radio = item(dialog, choice, name);
            QVERIFY2(radio, qPrintable(QStringLiteral("no choice %1 of %2").arg(choice, name)));
            if (!other && !radio->property("checked").toBool()) {
                other = radio;
            }
        }
        QVERIFY(other);
        QMetaObject::invokeMethod(other, "click");
        changed.append(name);
    }

    QQuickItem* apply = button(dialog, QStringLiteral("Apply"));
    QVERIFY(apply && apply->isEnabled());
    // Nothing is stored before Apply.
    for (const QString& name : std::as_const(changed)) {
        QCOMPARE(KonsoleSettings::self()->findItem(name)->property(), before.value(name));
    }
    QMetaObject::invokeMethod(apply, "click");
    for (const QString& name : std::as_const(changed)) {
        QVERIFY2(KonsoleSettings::self()->findItem(name)->property() != before.value(name), qPrintable(name));
    }
    QVERIFY(KonsoleSettings::scrollbackUseCacheLocation());
    QVERIFY(!KonsoleSettings::scrollbackUseSystemLocation());
    QCOMPARE(KonsoleSettings::scrollbackUseSpecifiedLocationDirectory(), QUrl::fromLocalFile(QStringLiteral("/tmp/configure-test")));
    QVERIFY(!apply->isEnabled());
}

void ConfigureDialogTest::testDefaultsAndCancel()
{
    QQuickWindow* dialog = openConfigure();
    QVERIFY(dialog);
    QQuickItem* apply = button(dialog, QStringLiteral("Apply"));
    QQuickItem* defaults = button(dialog, QStringLiteral("Defaults"));
    QVERIFY(apply && defaults);
    // Stored defaults: nothing to apply or reset.
    QVERIFY(!apply->isEnabled());
    QVERIFY(!defaults->isEnabled());

    QQuickItem* size = item(dialog, QStringLiteral("kcfg_ThumbnailSize"));
    size->setProperty("value", 400);
    QMetaObject::invokeMethod(size, "valueModified");
    QVERIFY(apply->isEnabled());
    QVERIFY(defaults->isEnabled());
    QMetaObject::invokeMethod(apply, "click");
    QCOMPARE(KonsoleSettings::thumbnailSize(), 400);

    // Defaults edits the controls; Cancel drops it.
    QMetaObject::invokeMethod(defaults, "click");
    QCOMPARE(size->property("value").toInt(), 250);
    QVERIFY(apply->isEnabled());
    QVERIFY(!defaults->isEnabled());
    QPointer<QQuickWindow> guard(dialog);
    QMetaObject::invokeMethod(button(dialog, QStringLiteral("Cancel")), "click");
    QTRY_VERIFY(!guard);
    QCOMPARE(KonsoleSettings::thumbnailSize(), 400);
}

void ConfigureDialogTest::testOk()
{
    QQuickWindow* dialog = openConfigure();
    QVERIFY(dialog);
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("kcfg_SearchNoWrap")), "click");
    QPointer<QQuickWindow> guard(dialog);
    QMetaObject::invokeMethod(button(dialog, QStringLiteral("OK")), "click");
    QTRY_VERIFY(!guard);
    QVERIFY(KonsoleSettings::searchNoWrap());
}

void ConfigureDialogTest::testTabBarRules()
{
    // TabBarSettings.ui: the tab bar's options are disabled while it is never shown,
    // except the New Tab button.
    QQuickWindow* dialog = openConfigure();
    QVERIFY(dialog);
    QQuickItem* top = item(dialog, QStringLiteral("Top"), QStringLiteral("TabBarPosition"));
    QQuickItem* middleClick = item(dialog, QStringLiteral("kcfg_CloseTabOnMiddleMouseButton"));
    QQuickItem* newTab = item(dialog, QStringLiteral("kcfg_NewTabButton"));
    QVERIFY(top && middleClick && newTab);
    QVERIFY(top->isEnabled());
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("AlwaysHideTabBar"), QStringLiteral("TabBarVisibility")), "click");
    QVERIFY(!top->isEnabled());
    QVERIFY(!middleClick->isEnabled());
    QVERIFY(newTab->isEnabled());
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("AlwaysShowTabBar"), QStringLiteral("TabBarVisibility")), "click");
    QVERIFY(top->isEnabled());
}

void ConfigureDialogTest::testOneDialogPerWindow()
{
    // Larger than the dialog, so centring it needs no clamping to the screen.
    _controller->qmlWindow()->setGeometry(100, 50, 1200, 900);
    QQuickWindow* dialog = openConfigure();
    QVERIFY(dialog);
    QCOMPARE(dialog->transientParent(), _controller->qmlWindow());
    QCOMPARE(dialog->modality(), Qt::NonModal);
    // Centred on the window as a QDialog (a non-modal one isn't placed by the window manager).
    QVERIFY(QLineF(dialog->geometry().center(), _controller->qmlWindow()->frameGeometry().center()).length() <= 1);
    QCOMPARE(dialog->property("currentIndex").toInt(), 0);
    // Manage Profiles shows the same dialog at its Profiles page.
    trigger(QStringLiteral("manage-profiles"));
    QCOMPARE(dialogsTitled(QStringLiteral("Configure")), QList<QQuickWindow*>{dialog});
    QCOMPARE(dialog->property("currentPage").value<QQuickItem*>()->property("title").toString(), QStringLiteral("Profiles"));
    trigger(QStringLiteral("options_configure"));
    QCOMPARE(dialogsTitled(QStringLiteral("Configure")), QList<QQuickWindow*>{dialog});

    // Closed, the next one is a new dialog.
    QPointer<QQuickWindow> guard(dialog);
    dialog->close();
    QTRY_VERIFY(!guard);
    QVERIFY(openConfigure());
}

void ConfigureDialogTest::testProfilesPage()
{
    Profile::Ptr profile(new Profile(ProfileManager::instance()->builtinProfile()));
    profile->setProperty(Profile::Name, QStringLiteral("ConfigureTest"));
    profile->setProperty(Profile::UntranslatedName, QStringLiteral("ConfigureTest"));
    ProfileManager::instance()->addProfile(profile);
    // Saved, so it is editable and deletable.
    ProfileManager::instance()->changeProfile(profile, {{Profile::Name, QStringLiteral("ConfigureTest")}}, true);
    QVERIFY(profile->isEditable() && profile->isDeletable());
    const Profile::Ptr previousDefault = ProfileManager::instance()->defaultProfile();

    trigger(QStringLiteral("manage-profiles"));
    QQuickWindow* dialog = nullptr;
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Configure"))));
    QQuickItem* list = item(dialog, QStringLiteral("profileListView"));
    QVERIFY(list);
    auto* model = list->property("model").value<ProfileListModel*>();
    QVERIFY(model);
    QQuickItem* newButton = item(dialog, QStringLiteral("newProfileButton"));
    QQuickItem* editButton = item(dialog, QStringLiteral("editProfileButton"));
    QQuickItem* deleteButton = item(dialog, QStringLiteral("deleteProfileButton"));
    QQuickItem* defaultButton = item(dialog, QStringLiteral("setAsDefaultButton"));
    // Nothing selected: only New.
    QCOMPARE(list->property("currentIndex").toInt(), -1);
    QVERIFY(newButton->isEnabled());
    QVERIFY(!editButton->isEnabled() && !deleteButton->isEnabled() && !defaultButton->isEnabled());

    const auto select = [&]() {
        list->setProperty("currentIndex", model->rowOf(QStringLiteral("ConfigureTest")));
    };
    select();
    QVERIFY(editButton->isEnabled() && deleteButton->isEnabled() && defaultButton->isEnabled());

    // Edit and New open the Edit Profile dialog over the window; New with the name selected.
    const auto editor = []() -> QQuickWindow* {
        const auto windows = QGuiApplication::topLevelWindows();
        for (QWindow* window : windows) {
            if (auto* found = qobject_cast<QQuickWindow*>(window); found && found->isVisible() && found->findChild<ProfileEditor*>()) {
                return found;
            }
        }
        return nullptr;
    };
    QMetaObject::invokeMethod(editButton, "click");
    QTRY_VERIFY(editor());
    QCOMPARE(editor()->title(), QStringLiteral("Edit Profile \"ConfigureTest\""));
    QCOMPARE(editor()->transientParent(), _controller->qmlWindow());
    QCOMPARE(editor()->modality(), Qt::ApplicationModal);
    editor()->close();
    QTRY_VERIFY(!editor());
    QMetaObject::invokeMethod(newButton, "click");
    QTRY_VERIFY(editor());
    QCOMPARE(editor()->title(), QStringLiteral("Create New Profile"));
    QQuickItem* name = editor()->findChild<QQuickItem*>(QStringLiteral("profileNameEdit"));
    QVERIFY(!name->property("text").toString().isEmpty());
    QCOMPARE(name->property("selectedText").toString(), name->property("text").toString());
    editor()->close();
    QTRY_VERIFY(!editor());

    // The shortcut column.
    const QKeySequence shortcut(QStringLiteral("Ctrl+Alt+F9"));
    model->setShortcut(model->rowOf(QStringLiteral("ConfigureTest")), shortcut);
    QCOMPARE(ProfileManager::instance()->shortcut(profile), shortcut);
    QCOMPARE(model->index(model->rowOf(QStringLiteral("ConfigureTest")), 0).data(ProfileListModel::ShortcutRole).value<QKeySequence>(), shortcut);

    // Set as Default: the default can't be removed or set again. The change resets the
    // list (ProfileManager reports it as a profile change), which clears the selection.
    QMetaObject::invokeMethod(defaultButton, "click");
    QCOMPARE(ProfileManager::instance()->defaultProfile(), profile);
    QVERIFY(model->index(model->rowOf(QStringLiteral("ConfigureTest")), 0).data(ProfileListModel::NameRole).toString().contains(QStringLiteral("[Default]")));
    select();
    QVERIFY(editButton->isEnabled());
    QVERIFY(!defaultButton->isEnabled() && !deleteButton->isEnabled());
    ProfileManager::instance()->setDefaultProfile(previousDefault);
    select();
    QVERIFY(deleteButton->isEnabled() && defaultButton->isEnabled());

    QMetaObject::invokeMethod(deleteButton, "click");
    QCOMPARE(model->rowOf(QStringLiteral("ConfigureTest")), -1);
    // The list was reset: nothing is selected.
    QCOMPARE(list->property("currentIndex").toInt(), -1);
}

void ConfigureDialogTest::testPartDialog()
{
    const KPluginMetaData metaData(QStringLiteral("konsolepart"));
    QVERIFY(metaData.isValid());
    QWidget host;
    auto result = KPluginFactory::instantiatePlugin<KParts::Part>(metaData, &host);
    QVERIFY(result);
    auto* layout = new QVBoxLayout(&host);
    layout->addWidget(result.plugin->widget());
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));

    QVERIFY(QMetaObject::invokeMethod(result.plugin, "showManageProfilesDialog", Q_ARG(QWidget*, &host)));
    QQuickWindow* dialog = nullptr;
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Configure"))));
    QCOMPARE(dialog->transientParent(), host.windowHandle());
    QVERIFY(item(dialog, QStringLiteral("profileListView")));
    QVERIFY(!button(dialog, QStringLiteral("Apply"))->isEnabled());
    QVERIFY(!button(dialog, QStringLiteral("Defaults"))->isEnabled());
    QVERIFY(button(dialog, QStringLiteral("Help")));
    // One dialog for every user of the part.
    QVERIFY(QMetaObject::invokeMethod(result.plugin, "showManageProfilesDialog", Q_ARG(QWidget*, &host)));
    QCOMPARE(dialogsTitled(QStringLiteral("Configure")), QList<QQuickWindow*>{dialog});
    QPointer<QQuickWindow> guard(dialog);
    QMetaObject::invokeMethod(button(dialog, QStringLiteral("OK")), "click");
    QTRY_VERIFY(!guard);
    delete result.plugin;
}

void ConfigureDialogTest::testEditCurrentProfile()
{
    // The built-in profile isn't editable: the action edits a new copy of it, and once that
    // is saved the session's profile is an editable one.
    QAction* action = nullptr;
    const auto clients = _window->guiFactory()->clients();
    for (KXMLGUIClient* client : clients) {
        action = action ? action : client->actionCollection()->action(QStringLiteral("edit-current-profile"));
    }
    QVERIFY(action);
    QCOMPARE(action->text().remove(u'&'), QStringLiteral("Create New Profile..."));
    action->trigger();
    QQuickWindow* dialog = nullptr;
    QTRY_VERIFY((dialog = dialogTitled(QStringLiteral("Create New Profile"))));
    QCOMPARE(dialog->transientParent(), _controller->qmlWindow());
    QCOMPARE(dialog->modality(), Qt::ApplicationModal);
    auto* editor = dialog->findChild<ProfileEditor*>();
    QVERIFY(editor);
    const Profile::Ptr created = editor->profile();
    QVERIFY(created != ProfileManager::instance()->builtinProfile());
    QPointer<QQuickWindow> guard(dialog);
    QMetaObject::invokeMethod(button(dialog, QStringLiteral("OK")), "click");
    QTRY_VERIFY(!guard);
    QVERIFY(ProfileManager::instance()->allProfiles().contains(created));
    QCOMPARE(action->text().remove(u'&'), QStringLiteral("Edit Current Profile..."));
    ProfileManager::instance()->deleteProfile(created);
}

QTEST_MAIN(ConfigureDialogTest)

#include "ConfigureDialogTest.moc"
