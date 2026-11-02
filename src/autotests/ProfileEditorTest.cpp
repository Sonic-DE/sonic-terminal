/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The QML Edit Profile dialog does what the widget one did: every profile property a
// control of its pages edited is stored by the same control on Apply, previews are undone
// on Cancel, OK is refused with a message for a bad name or an unwritable profile, and the
// default toggle, the Apply button and the dialogs its buttons open behave as before.

#include "TestTheme.h"

#include "../characters/CharacterColor.h"
#include "../colorscheme/ColorSchemeManager.h"
#include "../keyboardtranslator/KeyboardTranslatorManager.h"
#include "../profile/ProfileManager.h"
#include "../quick/ColorSchemeEditorBackend.h"
#include "../quick/KeyBindingEditorBackend.h"
#include "../quick/ProfileEditor.h"
#include "../widgets/EditProfileDialog.h"

#include <KColorButton>
#include <KFontChooser>
#include <KPageView>
#include <KPageWidgetModel>

#include <QAbstractSlider>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRadioButton>
#include <QSet>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>

using namespace Konsole;

namespace {
// Buttons that open a dialog or menu, or act on a list's scheme, instead of editing a
// property (their own tests), and the default toggle (testSetAsDefault).
const QStringList notEditing{
    QStringLiteral("iconSelectButton"),
    QStringLiteral("dirSelectButton"),
    QStringLiteral("environmentEditButton"),
    QStringLiteral("textEditorCustomBtn"),
    QStringLiteral("selectEncodingButton"),
    QStringLiteral("tabTitleFormatButton"),
    QStringLiteral("remoteTabTitleFormatButton"),
    QStringLiteral("fixedSizeHistoryWarningButton"),
    QStringLiteral("unlimitedHistoryWarningButton"),
    QStringLiteral("setAsDefaultButton"),
    QStringLiteral("chooseFontButton"),
    QStringLiteral("emojiFontPreview"),
    QStringLiteral("newColorSchemeButton"),
    QStringLiteral("editColorSchemeButton"),
    QStringLiteral("removeColorSchemeButton"),
    QStringLiteral("resetColorSchemeButton"),
    QStringLiteral("newKeyBindingsButton"),
    QStringLiteral("editKeyBindingsButton"),
    QStringLiteral("removeKeyBindingsButton"),
    QStringLiteral("resetKeyBindingsButton"),
};

QStringList propertyNames()
{
    QStringList names;
    for (const std::string& entry : Profile::propertiesInfoList()) {
        const QString name = QString::fromStdString(entry).section(QStringLiteral(" : "), 0, 0);
        if (name != QLatin1String("Path")) {
            names.append(name);
        }
    }
    return names;
}

// The schemes' names in one of ProfileEditor's lists. (The test has its own copies of the
// static colour scheme and key bindings managers; the editor's are the library's.)
QStringList names(const QVariantList& list)
{
    QStringList found;
    for (const QVariant& item : list) {
        found.append(item.toMap().value(QStringLiteral("name")).toString());
    }
    return found;
}

QVariantMap entry(const QVariantList& list, const QString& name)
{
    for (const QVariant& item : list) {
        if (item.toMap().value(QStringLiteral("name")).toString() == name) {
            return item.toMap();
        }
    }
    return {};
}

QVariantMap snapshot(const Profile::Ptr& profile)
{
    QVariantMap values;
    for (const QString& name : propertyNames()) {
        values.insert(name, profile->property<QVariant>(Profile::lookupByName(name)));
    }
    return values;
}

QStringList changedProperties(const QVariantMap& before, const QVariantMap& after)
{
    QStringList changed;
    for (auto it = before.cbegin(); it != before.cend(); ++it) {
        if (after.value(it.key()) != it.value()) {
            changed.append(it.key());
        }
    }
    return changed;
}

// What a widget control was changed to, for the QML control to get the same input.
struct Edit {
    QString objectName;
    QVariant input;
    bool enabled = true;
    // A check box's, radio or tool button's state as the page showed the profile.
    QVariant checked;
    // The properties the profile shows at once (previews), and those Apply stores.
    QStringList previewed;
    QStringList changed;
};
}

class ProfileEditorTest : public QObject {
    Q_OBJECT

public:
    static void initMain()
    {
        TestTheme::setUp();
        // Without a session, the platform theme's file dialog never shows and never returns.
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        // A system-wide colour scheme file (the bundled ones are resources): one the user's
        // changed copy can be reset to.
        static QTemporaryDir systemData;
        QDir(systemData.path()).mkpath(QStringLiteral("konsole"));
        QFile::copy(QStringLiteral(":/konsole/color-schemes/Linux.colorscheme"), systemData.filePath(QStringLiteral("konsole/ProfileEditorTestSystem.colorscheme")));
        qputenv("XDG_DATA_DIRS", QByteArray(systemData.path().toLocal8Bit() + ':' + qgetenv("XDG_DATA_DIRS")));
    }

private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testEveryPropertyStored();
    void testPreview();
    void testApplyEnabled();
    void testNameChecks();
    void testReadOnlyProfile();
    void testSetAsDefault();
    void testNewProfile();
    void testSubDialogs();
    void testColorSchemeList();
    void testColorSchemeEditor();
    void testFonts();
    void testKeyBindings();

private:
    static QQuickWindow* shownDialog(const QString& title);
    static Profile::Ptr savedProfile(const QString& name);
    static QQuickItem* item(QQuickWindow* dialog, const QString& objectName);
    static QQuickItem* standardButton(QQuickWindow* dialog, const QString& text);
    static QList<QWidget*> editingWidgets(EditProfileDialog* dialog);
    static bool changeWidget(QWidget* widget, Edit& edit);
    static bool changeItem(QQuickItem* item, const Edit& edit);
};

void ProfileEditorTest::initTestCase()
{
    QCoreApplication::setApplicationName(QStringLiteral("konsole"));
}

void ProfileEditorTest::cleanup()
{
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* window : windows) {
        if (qobject_cast<QQuickWindow*>(window) && window->isVisible()) {
            window->close();
        }
    }
    ProfileManager::instance()->setDefaultProfile(ProfileManager::instance()->builtinProfile());
}

Profile::Ptr ProfileEditorTest::savedProfile(const QString& name)
{
    Profile::Ptr profile(new Profile(ProfileManager::instance()->builtinProfile()));
    profile->setProperty(Profile::Name, name);
    profile->setProperty(Profile::UntranslatedName, name);
    // Dependent controls start enabled, or are enabled by an earlier control.
    profile->setProperty(Profile::HistoryMode, Enum::FixedSizeHistory);
    profile->setProperty(Profile::UnderlineLinksEnabled, true);
    profile->setProperty(Profile::AllowEscapedLinks, false);
    profile->setProperty(Profile::DimWhenInactive, true);
    profile->setProperty(Profile::BorderWhenActive, true);
    profile->setProperty(Profile::WordMode, false);
    profile->setProperty(Profile::BidiRenderingEnabled, false);
    profile->setProperty(Profile::UseFontLineCharacters, true);
    ProfileManager::instance()->addProfile(profile);
    ProfileManager::instance()->changeProfile(profile, {{Profile::Name, name}});
    return profile;
}

QQuickItem* ProfileEditorTest::item(QQuickWindow* dialog, const QString& objectName)
{
    return dialog->findChild<QQuickItem*>(objectName);
}

QQuickItem* ProfileEditorTest::standardButton(QQuickWindow* dialog, const QString& text)
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

QList<QWidget*> ProfileEditorTest::editingWidgets(EditProfileDialog* dialog)
{
    QList<QWidget*> found;
    auto* view = dialog->findChild<KPageView*>();
    auto* model = qobject_cast<KPageWidgetModel*>(view->model());
    for (int row = 0; row < model->rowCount(); ++row) {
        KPageWidgetItem* page = model->item(model->index(row, 0));
        // Each page is set up once it is shown.
        dialog->setCurrentPage(page);
        const QList<QWidget*> widgets = page->widget()->findChildren<QWidget*>();
        for (QWidget* widget : widgets) {
            const bool control = qobject_cast<QAbstractButton*>(widget) || qobject_cast<QAbstractSpinBox*>(widget) || qobject_cast<QLineEdit*>(widget)
                || qobject_cast<QComboBox*>(widget) || qobject_cast<QAbstractSlider*>(widget) || qobject_cast<QKeySequenceEdit*>(widget);
            if (!control || widget->objectName().isEmpty() || widget->objectName().startsWith(QLatin1String("qt_"))) {
                continue;
            }
            // A composite control's own parts.
            QWidget* parent = widget->parentWidget();
            bool part = false;
            for (; parent && parent != page->widget(); parent = parent->parentWidget()) {
                part = part || qobject_cast<QAbstractSpinBox*>(parent) || qobject_cast<QComboBox*>(parent) || qobject_cast<QKeySequenceEdit*>(parent)
                    || parent->inherits("KMessageWidget") || parent->inherits("QTabBar");
            }
            if (!part) {
                found.append(widget);
            }
        }
    }
    return found;
}

bool ProfileEditorTest::changeWidget(QWidget* widget, Edit& edit)
{
    if (auto* colorButton = qobject_cast<KColorButton*>(widget)) {
        edit.input = QColor(Qt::red);
        colorButton->setColor(Qt::red);
    } else if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
        if (!button->isCheckable() || (qobject_cast<QRadioButton*>(button) && button->isChecked())) {
            return false;
        }
        button->click();
    } else if (auto* doubleSpin = qobject_cast<QDoubleSpinBox*>(widget)) {
        const double value = doubleSpin->value() < doubleSpin->maximum() ? doubleSpin->value() + 0.5 : doubleSpin->value() - 0.5;
        edit.input = value;
        doubleSpin->setValue(value);
    } else if (auto* spin = qobject_cast<QSpinBox*>(widget)) {
        const int value = spin->value() < spin->maximum() ? spin->value() + 1 : spin->value() - 1;
        edit.input = value;
        spin->setValue(value);
    } else if (auto* line = qobject_cast<QLineEdit*>(widget)) {
        // Typed at the end: the two profiles have different names.
        edit.input = QStringLiteral("x");
        line->setText(line->text() + edit.input.toString());
    } else if (auto* combo = qobject_cast<QComboBox*>(widget)) {
        // A single choice (no containers found) can't change.
        if (combo->count() < 2) {
            return true;
        }
        // Past "Custom…" of a colour combo, which opens a dialog.
        const int index = combo->currentIndex() == 2 ? 3 : 2;
        edit.input = index;
        combo->setCurrentIndex(index);
        Q_EMIT combo->activated(index);
    } else if (auto* slider = qobject_cast<QAbstractSlider*>(widget)) {
        const int value = slider->value() < slider->maximum() ? slider->value() + 10 : slider->value() - 10;
        edit.input = value;
        slider->setValue(value);
    } else if (auto* keys = qobject_cast<QKeySequenceEdit*>(widget)) {
        edit.input = QKeySequence(QStringLiteral("Ctrl+Shift+F12"));
        keys->setKeySequence(edit.input.value<QKeySequence>());
        Q_EMIT keys->editingFinished();
    } else {
        return false;
    }
    return true;
}

bool ProfileEditorTest::changeItem(QQuickItem* item, const Edit& edit)
{
    if (item->metaObject()->indexOfSignal("colorEdited(QColor)") >= 0) {
        item->setProperty("color", edit.input);
        return QMetaObject::invokeMethod(item, "colorEdited", Q_ARG(QColor, edit.input.value<QColor>()));
    }
    if (item->inherits("QQuickAbstractButton")) {
        return QMetaObject::invokeMethod(item, "click");
    }
    if (item->inherits("QQuickSpinBox")) {
        const bool real = item->property("realFrom").isValid();
        item->setProperty("value", real ? qRound(edit.input.toDouble() * 10) : edit.input.toInt());
        return QMetaObject::invokeMethod(item, "valueModified");
    }
    if (item->inherits("QQuickTextInput")) {
        item->setProperty("text", QString(item->property("text").toString() + edit.input.toString()));
        return QMetaObject::invokeMethod(item, "textEdited");
    }
    if (item->inherits("QQuickComboBox")) {
        if (!edit.input.isValid()) {
            return item->property("count").toInt() < 2;
        }
        item->setProperty("currentIndex", edit.input);
        return QMetaObject::invokeMethod(item, "activated", Q_ARG(int, edit.input.toInt()));
    }
    if (item->inherits("QQuickSlider")) {
        item->setProperty("value", edit.input);
        return QMetaObject::invokeMethod(item, "moved");
    }
    if (item->property("keySequence").isValid()) {
        item->setProperty("keySequence", edit.input);
        return QMetaObject::invokeMethod(item, "keySequenceModified");
    }
    return false;
}

void ProfileEditorTest::testEveryPropertyStored()
{
    // The widget dialog: which properties each control's change stores on Apply.
    const Profile::Ptr widgetProfile = savedProfile(QStringLiteral("ProfileEditorTest widget"));
    QList<Edit> edits;
    {
        EditProfileDialog dialog;
        dialog.setProfile(widgetProfile);
        QPushButton* apply = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply);
        const QList<QWidget*> widgets = editingWidgets(&dialog);
        QVERIFY(widgets.size() > 50);
        for (QWidget* widget : widgets) {
            Edit edit{widget->objectName()};
            if (notEditing.contains(edit.objectName)) {
                continue;
            }
            edit.enabled = widget->isEnabled();
            if (auto* button = qobject_cast<QAbstractButton*>(widget); button && button->isCheckable()) {
                edit.checked = button->isChecked();
            }
            const QVariantMap before = snapshot(widgetProfile);
            if (edit.enabled && !changeWidget(widget, edit)) {
                // A checked radio button: its state is still compared.
                if (qobject_cast<QRadioButton*>(widget)) {
                    edits.append(edit);
                    continue;
                }
                QFAIL(qPrintable(QStringLiteral("unexpected control ") + edit.objectName));
            }
            edit.previewed = changedProperties(before, snapshot(widgetProfile));
            apply->click();
            edit.changed = changedProperties(before, snapshot(widgetProfile));
            edits.append(edit);
        }
    }
    QStringList covered;
    for (const Edit& edit : std::as_const(edits)) {
        covered += edit.changed;
    }
    covered.removeDuplicates();
    QVERIFY2(covered.size() > 50, qPrintable(covered.join(u' ')));

    // The QML dialog on a profile with the same values: the same control, changed the same
    // way, stores the same properties.
    const Profile::Ptr qmlProfile = savedProfile(QStringLiteral("ProfileEditorTest qml"));
    QPointer<QQuickWindow> dialog = ProfileEditor::open(qmlProfile, ProfileEditor::ExistingProfile, nullptr);
    QVERIFY(dialog);
    QQuickItem* apply = standardButton(dialog, QStringLiteral("Apply"));
    QVERIFY(apply);
    for (const Edit& edit : std::as_const(edits)) {
        QQuickItem* control = item(dialog, edit.objectName);
        QVERIFY2(control, qPrintable(QStringLiteral("no control ") + edit.objectName));
        QVERIFY2(control->isEnabled() == edit.enabled, qPrintable(edit.objectName));
        if (edit.checked.isValid()) {
            QVERIFY2(control->property("checked") == edit.checked, qPrintable(edit.objectName + QStringLiteral(" shows another state")));
        }
        // A checked radio button isn't changed.
        if (edit.enabled && control->inherits("QQuickRadioButton") && edit.checked.toBool()) {
            continue;
        }
        const QVariantMap before = snapshot(qmlProfile);
        if (edit.enabled) {
            QVERIFY2(changeItem(control, edit), qPrintable(edit.objectName));
        }
        QStringList previewed = changedProperties(before, snapshot(qmlProfile));
        QStringList expectedPreview = edit.previewed;
        previewed.sort();
        expectedPreview.sort();
        QVERIFY2(previewed == expectedPreview,
            qPrintable(QStringLiteral("%1: QML previewed [%2], widget [%3]").arg(edit.objectName, previewed.join(u' '), expectedPreview.join(u' '))));
        QMetaObject::invokeMethod(apply, "click");
        QStringList changed = changedProperties(before, snapshot(qmlProfile));
        QStringList expected = edit.changed;
        changed.sort();
        expected.sort();
        QVERIFY2(changed == expected,
            qPrintable(QStringLiteral("%1: QML stored [%2], widget [%3]").arg(edit.objectName, changed.join(u' '), expected.join(u' '))));
    }
    // Nothing waits for Apply.
    QVERIFY(!apply->isEnabled());
}

void ProfileEditorTest::testPreview()
{
    const Profile::Ptr profile = savedProfile(QStringLiteral("ProfileEditorTest preview"));
    const bool original = profile->property<bool>(Profile::BlinkingTextEnabled);
    {
        // Shown at once, undone on Cancel.
        ProfileEditor editor(profile, ProfileEditor::ExistingProfile);
        editor.setValue(QStringLiteral("BlinkingTextEnabled"), !original, true);
        QCOMPARE(profile->property<bool>(Profile::BlinkingTextEnabled), !original);
        editor.reject();
        QCOMPARE(profile->property<bool>(Profile::BlinkingTextEnabled), original);
    }
    {
        // Kept on OK.
        ProfileEditor editor(profile, ProfileEditor::ExistingProfile);
        editor.setValue(QStringLiteral("BlinkingTextEnabled"), !original, true);
        QVERIFY(editor.accept());
        QCOMPARE(profile->property<bool>(Profile::BlinkingTextEnabled), !original);
    }
    {
        // Not previewed: the profile keeps its value until Apply.
        ProfileEditor editor(profile, ProfileEditor::ExistingProfile);
        editor.setValue(QStringLiteral("BlinkingTextEnabled"), original);
        QCOMPARE(profile->property<bool>(Profile::BlinkingTextEnabled), !original);
        QVERIFY(editor.apply());
        QCOMPARE(profile->property<bool>(Profile::BlinkingTextEnabled), original);
    }
}

void ProfileEditorTest::testApplyEnabled()
{
    const Profile::Ptr profile = savedProfile(QStringLiteral("ProfileEditorTest apply"));
    const bool flow = profile->property<bool>(Profile::FlowControlEnabled);
    ProfileEditor editor(profile, ProfileEditor::ExistingProfile);
    QVERIFY(!editor.applyEnabled());
    editor.setValue(QStringLiteral("FlowControlEnabled"), !flow);
    QVERIFY(editor.applyEnabled());
    // Back to the profile's value: nothing to apply.
    editor.setValue(QStringLiteral("FlowControlEnabled"), flow);
    QVERIFY(!editor.applyEnabled());
    editor.setSetAsDefault(true);
    QVERIFY(editor.applyEnabled());
    editor.setSetAsDefault(false);
    QVERIFY(!editor.applyEnabled());

    // A new profile: not before an edit, and not after a name edit.
    Profile::Ptr created(new Profile(ProfileManager::instance()->builtinProfile()));
    created->setProperty(Profile::Name, QStringLiteral("ProfileEditorTest created"));
    ProfileEditor newEditor(created, ProfileEditor::NewProfile);
    QVERIFY(!newEditor.applyEnabled());
    QCOMPARE(newEditor.title(), QStringLiteral("Create New Profile"));
    newEditor.setValue(QStringLiteral("Name"), QStringLiteral("ProfileEditorTest renamed"));
    QVERIFY(!newEditor.applyEnabled());
    newEditor.setValue(QStringLiteral("FlowControlEnabled"), !flow);
    QVERIFY(newEditor.applyEnabled());
}

void ProfileEditorTest::testNameChecks()
{
    const Profile::Ptr other = savedProfile(QStringLiteral("ProfileEditorTest other"));
    const Profile::Ptr profile = savedProfile(QStringLiteral("ProfileEditorTest names"));
    QPointer<QQuickWindow> dialog = ProfileEditor::open(profile, ProfileEditor::ExistingProfile, nullptr);
    QVERIFY(dialog);
    QCOMPARE(dialog->title(), QStringLiteral("Edit Profile \"ProfileEditorTest names\""));
    QQuickItem* name = item(dialog, QStringLiteral("profileNameEdit"));
    QQuickItem* message = item(dialog, QStringLiteral("generalPageMessageWidget"));
    QQuickItem* ok = standardButton(dialog, QStringLiteral("OK"));
    QVERIFY(name && message && ok);
    QVERIFY(!message->isVisible());

    const struct {
        QString name;
        QString message;
    } refused[] = {
        {QString(), QStringLiteral("Profile Name was empty")},
        {other->name(), QStringLiteral("already exists")},
        {QStringLiteral("bad/name"), QStringLiteral("invalid in profile names")},
    };
    for (const auto& check : refused) {
        name->setProperty("text", check.name);
        QMetaObject::invokeMethod(ok, "click");
        // Refused: the dialog stays, says why and shows the profile's name again, selected.
        QVERIFY(dialog && dialog->isVisible());
        QTRY_VERIFY(message->isVisible());
        QVERIFY2(message->property("text").toString().contains(check.message), qPrintable(message->property("text").toString()));
        QCOMPARE(name->property("text").toString(), profile->name());
        QCOMPARE(name->property("selectedText").toString(), profile->name());
        QCOMPARE(dialog->property("currentIndex").toInt(), 0);
        QCOMPARE(profile->name(), QStringLiteral("ProfileEditorTest names"));
    }

    // The title follows the name; a valid one is stored and closes the dialog.
    name->setProperty("text", QStringLiteral("ProfileEditorTest renamed"));
    QCOMPARE(dialog->title(), QStringLiteral("Edit Profile \"ProfileEditorTest renamed\""));
    QMetaObject::invokeMethod(ok, "click");
    QTRY_VERIFY(!dialog);
    QCOMPARE(profile->name(), QStringLiteral("ProfileEditorTest renamed"));
}

void ProfileEditorTest::testReadOnlyProfile()
{
    const Profile::Ptr profile = savedProfile(QStringLiteral("ProfileEditorTest read-only"));
    QFile file(profile->path());
    QVERIFY(file.exists());
    const QFile::Permissions permissions = file.permissions();
    QVERIFY(file.setPermissions(QFile::ReadOwner));
    const bool flow = profile->property<bool>(Profile::FlowControlEnabled);

    QPointer<QQuickWindow> dialog = ProfileEditor::open(profile, ProfileEditor::ExistingProfile, nullptr);
    QVERIFY(dialog);
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("enableFlowControlButton")), "click");
    QMetaObject::invokeMethod(standardButton(dialog, QStringLiteral("OK")), "click");
    QVERIFY(dialog && dialog->isVisible());
    QQuickItem* message = item(dialog, QStringLiteral("generalPageMessageWidget"));
    QTRY_VERIFY(message->isVisible());
    QVERIFY(message->property("text").toString().contains(QStringLiteral("Insufficient permissions")));
    QCOMPARE(profile->property<bool>(Profile::FlowControlEnabled), flow);
    QMetaObject::invokeMethod(standardButton(dialog, QStringLiteral("Cancel")), "click");
    QTRY_VERIFY(!dialog);
    QVERIFY(file.setPermissions(permissions));
}

void ProfileEditorTest::testSetAsDefault()
{
    const Profile::Ptr profile = savedProfile(QStringLiteral("ProfileEditorTest default"));
    QPointer<QQuickWindow> dialog = ProfileEditor::open(profile, ProfileEditor::ExistingProfile, nullptr);
    QVERIFY(dialog);
    QQuickItem* toggle = item(dialog, QStringLiteral("setAsDefaultButton"));
    QQuickItem* apply = standardButton(dialog, QStringLiteral("Apply"));
    QVERIFY(toggle && apply);
    QCOMPARE(toggle->property("text").toString(), QStringLiteral("Default profile"));
    QVERIFY(!toggle->property("checked").toBool());
    QVERIFY(!apply->isEnabled());
    QMetaObject::invokeMethod(toggle, "click");
    QVERIFY(apply->isEnabled());
    QMetaObject::invokeMethod(apply, "click");
    QCOMPARE(ProfileManager::instance()->defaultProfile(), profile);
    QVERIFY(!apply->isEnabled());
    // Unset: the built-in profile is the default again.
    QMetaObject::invokeMethod(toggle, "click");
    QMetaObject::invokeMethod(apply, "click");
    QCOMPARE(ProfileManager::instance()->defaultProfile(), ProfileManager::instance()->builtinProfile());
}

void ProfileEditorTest::testNewProfile()
{
    Profile::Ptr created(new Profile(ProfileManager::instance()->builtinProfile()));
    created->setProperty(Profile::Name, QStringLiteral("ProfileEditorTest new"));
    created->setProperty(Profile::UntranslatedName, QStringLiteral("ProfileEditorTest new"));
    QPointer<QQuickWindow> dialog = ProfileEditor::open(created, ProfileEditor::NewProfile, nullptr);
    QVERIFY(dialog);
    QCOMPARE(dialog->title(), QStringLiteral("Create New Profile"));
    QVERIFY(!ProfileManager::instance()->allProfiles().contains(created));
    // OK with no edit still saves the new profile.
    QMetaObject::invokeMethod(standardButton(dialog, QStringLiteral("OK")), "click");
    QTRY_VERIFY(!dialog);
    QVERIFY(ProfileManager::instance()->allProfiles().contains(created));
    QVERIFY(QFile::exists(created->path()));
}

void ProfileEditorTest::testSubDialogs()
{
    const Profile::Ptr profile = savedProfile(QStringLiteral("ProfileEditorTest dialogs"));
    QPointer<QQuickWindow> dialog = ProfileEditor::open(profile, ProfileEditor::ExistingProfile, nullptr);
    QVERIFY(dialog);
    QQuickItem* apply = standardButton(dialog, QStringLiteral("Apply"));
    const auto openedDialog = [](const QString& title) -> QQuickWindow* {
        const auto windows = QGuiApplication::topLevelWindows();
        for (QWindow* window : windows) {
            if (auto* found = qobject_cast<QQuickWindow*>(window); found && found->isVisible() && found->title() == title) {
                return found;
            }
        }
        return nullptr;
    };

    // Environment: a QML dialog on top of this one, its text stored as one variable per line.
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("environmentEditButton")), "click");
    QQuickWindow* environment = nullptr;
    QTRY_VERIFY((environment = openedDialog(QStringLiteral("Edit Environment"))));
    QCOMPARE(environment->transientParent(), dialog.data());
    QCOMPARE(environment->modality(), Qt::ApplicationModal);
    QCOMPARE(environment->property("textValue").toString(), profile->environment().join(u'\n'));
    item(environment, QStringLiteral("textValueArea"))->setProperty("text", QStringLiteral("A=1\nB=2"));
    QMetaObject::invokeMethod(environment, "accept");
    QMetaObject::invokeMethod(apply, "click");
    QCOMPARE(profile->environment(), (QStringList{QStringLiteral("A=1"), QStringLiteral("B=2")}));

    // The custom editor command: enabled by the Custom editor, a non-modal dialog.
    QQuickItem* custom = item(dialog, QStringLiteral("textEditorCustomBtn"));
    QVERIFY(!custom->isEnabled() || profile->property<int>(Profile::TextEditorCmd) == Enum::CustomTextEditor);
    QQuickItem* editors = item(dialog, QStringLiteral("textEditorCombo"));
    editors->setProperty("currentIndex", int(Enum::CustomTextEditor));
    QMetaObject::invokeMethod(editors, "activated", Q_ARG(int, Enum::CustomTextEditor));
    QVERIFY(custom->isEnabled());
    QMetaObject::invokeMethod(custom, "click");
    QQuickWindow* command = nullptr;
    QTRY_VERIFY((command = openedDialog(QStringLiteral("Text Editor Custom Command"))));
    QCOMPARE(command->transientParent(), dialog.data());
    QCOMPARE(command->modality(), Qt::NonModal);
    item(command, QStringLiteral("textValueEdit"))->setProperty("text", QStringLiteral("myeditor +LINE PATH"));
    QMetaObject::invokeMethod(command, "accept");
    QMetaObject::invokeMethod(apply, "click");
    QCOMPARE(profile->customTextEditorCmd(), QStringLiteral("myeditor +LINE PATH"));
    QCOMPARE(profile->property<int>(Profile::TextEditorCmd), int(Enum::CustomTextEditor));

    // The kept framework dialogs are transient for the QML dialog.
    for (const QString& opener : {QStringLiteral("iconSelectButton"), QStringLiteral("dirSelectButton")}) {
        QPointer<QWidget> modal;
        QWindow* parent = nullptr;
        const auto shownDialog = []() -> QDialog* {
            const auto widgets = QApplication::topLevelWidgets();
            for (QWidget* widget : widgets) {
                if (auto* found = qobject_cast<QDialog*>(widget); found && found->isVisible()) {
                    return found;
                }
            }
            return nullptr;
        };
        QTimer::singleShot(0, this, [&] {
            QTRY_VERIFY((modal = shownDialog()));
            parent = modal->windowHandle()->transientParent();
            modal->close();
        });
        QMetaObject::invokeMethod(item(dialog, opener), "click");
        QVERIFY2(modal.isNull() || !modal->isVisible(), qPrintable(opener));
        QCOMPARE(parent, dialog.data());
    }

    // The encoding menu below its button; the chosen codec is the button's text.
    QQuickItem* encoding = item(dialog, QStringLiteral("selectEncodingButton"));
    const QString before = encoding->property("text").toString();
    QString chosen;
    QTimer::singleShot(0, this, [&] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        QVERIFY(menu);
        // The first codec of the first script.
        for (QAction* script : menu->actions()) {
            if (script->menu() && !script->menu()->actions().isEmpty()) {
                QAction* codec = script->menu()->actions().constFirst();
                chosen = codec->text();
                codec->trigger();
                break;
            }
        }
        menu->close();
    });
    QMetaObject::invokeMethod(encoding, "click");
    QVERIFY(!chosen.isEmpty());
    QVERIFY(encoding->property("text").toString() != before);
    QMetaObject::invokeMethod(apply, "click");
    QCOMPARE(profile->defaultEncoding(), encoding->property("text").toString());
}

QQuickWindow* ProfileEditorTest::shownDialog(const QString& title)
{
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* window : windows) {
        if (auto* found = qobject_cast<QQuickWindow*>(window); found && found->isVisible() && found->title() == title) {
            return found;
        }
    }
    return nullptr;
}

void ProfileEditorTest::testColorSchemeList()
{
    const Profile::Ptr profile = savedProfile(QStringLiteral("ProfileEditorTest schemes"));
    const QString original = profile->colorScheme();
    QPointer<QQuickWindow> dialog = ProfileEditor::open(profile, ProfileEditor::ExistingProfile, nullptr);
    QVERIFY(dialog);
    auto* editor = dialog->findChild<ProfileEditor*>();
    QVERIFY(editor);
    // The list holds every scheme, by description, and selects the profile's.
    const QVariantList schemes = editor->colorSchemes();
    QVERIFY(schemes.size() > 5);
    QStringList descriptions;
    for (const QVariant& scheme : schemes) {
        descriptions.append(scheme.toMap().value(QStringLiteral("description")).toString());
    }
    QStringList sorted = descriptions;
    sorted.sort();
    QCOMPARE(descriptions, sorted);
    QQuickItem* list = item(dialog, QStringLiteral("colorSchemeList"));
    QVERIFY(list);
    QCOMPARE(schemes.value(list->property("currentIndex").toInt()).toMap().value(QStringLiteral("name")).toString(), original);

    // Choosing another scheme shows it at once; Cancel undoes it.
    QString other;
    for (const QVariant& scheme : schemes) {
        if (scheme.toMap().value(QStringLiteral("name")).toString() != original) {
            other = scheme.toMap().value(QStringLiteral("name")).toString();
            break;
        }
    }
    editor->selectColorScheme(other);
    QCOMPARE(profile->colorScheme(), other);
    QVERIFY(editor->applyEnabled());
    QMetaObject::invokeMethod(standardButton(dialog, QStringLiteral("Cancel")), "click");
    QTRY_VERIFY(!dialog);
    QCOMPARE(profile->colorScheme(), original);

    // A new scheme from the editor is listed and chosen; a user's own scheme can be
    // removed, a changed system one reset.
    dialog = ProfileEditor::open(profile, ProfileEditor::ExistingProfile, nullptr);
    editor = dialog->findChild<ProfileEditor*>();
    ColorScheme scheme(*ColorSchemeManager::instance()->findColorScheme(original));
    scheme.setDescription(QStringLiteral("ProfileEditorTest scheme"));
    editor->saveColorScheme(scheme, true);
    QVERIFY(names(editor->colorSchemes()).contains(QStringLiteral("ProfileEditorTest scheme")));
    QCOMPARE(editor->value(QStringLiteral("ColorScheme")).toString(), QStringLiteral("ProfileEditorTest scheme"));
    const auto schemeNames = [editor] {
        return names(editor->colorSchemes());
    };
    list = item(dialog, QStringLiteral("colorSchemeList"));
    QTRY_COMPARE(schemeNames().value(list->property("currentIndex").toInt()), QStringLiteral("ProfileEditorTest scheme"));
    QQuickItem* remove = item(dialog, QStringLiteral("removeColorSchemeButton"));
    QQuickItem* reset = item(dialog, QStringLiteral("resetColorSchemeButton"));
    QTRY_VERIFY(remove->isEnabled());
    QVERIFY(!reset->isEnabled());
    QMetaObject::invokeMethod(remove, "click");
    QVERIFY(!schemeNames().contains(QStringLiteral("ProfileEditorTest scheme")));

    // A system scheme (initMain()'s file) saved with changes can be reset, not removed.
    const QString system = QStringLiteral("ProfileEditorTestSystem");
    QVERIFY(schemeNames().contains(system));
    QVERIFY(!editor->canResetColorScheme(system));
    ColorScheme changed(*ColorSchemeManager::instance()->findColorScheme(system));
    const qreal opacity = changed.opacity();
    changed.setOpacity(opacity / 2);
    editor->saveColorScheme(changed, false);
    QVERIFY(editor->canResetColorScheme(system));
    QTRY_VERIFY(reset->isEnabled());
    QVERIFY(!remove->isEnabled());
    QMetaObject::invokeMethod(reset, "click");
    QVERIFY(!editor->canResetColorScheme(system));
    QVERIFY(qFuzzyCompare(entry(editor->colorSchemes(), system).value(QStringLiteral("background")).value<QColor>().alphaF(), float(opacity)));
    QTRY_VERIFY(!reset->isEnabled());
    QMetaObject::invokeMethod(standardButton(dialog, QStringLiteral("Cancel")), "click");
    QTRY_VERIFY(!dialog);
}

void ProfileEditorTest::testColorSchemeEditor()
{
    const Profile::Ptr profile = savedProfile(QStringLiteral("ProfileEditorTest scheme editor"));
    QPointer<QQuickWindow> dialog = ProfileEditor::open(profile, ProfileEditor::ExistingProfile, nullptr);
    QVERIFY(dialog);
    auto* editor = dialog->findChild<ProfileEditor*>();

    // New: a non-modal editor over the dialog, named after its description.
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("newColorSchemeButton")), "click");
    QQuickWindow* schemeEditor = nullptr;
    QTRY_VERIFY((schemeEditor = shownDialog(QStringLiteral("New Color Scheme"))));
    QCOMPARE(schemeEditor->transientParent(), dialog.data());
    QCOMPARE(schemeEditor->modality(), Qt::NonModal);
    auto* backend = schemeEditor->findChild<ColorSchemeEditorBackend*>();
    QVERIFY(backend);
    QCOMPARE(item(schemeEditor, QStringLiteral("descriptionEdit"))->property("text").toString(), QStringLiteral("New Color Scheme"));
    // The Color, Intense color and Faint color columns are equally wide (headers and cells).
    {
        const QStringList colorHeaders{QStringLiteral("Color"), QStringLiteral("Intense color"), QStringLiteral("Faint color")};
        QSet<qreal> widths;
        int columnItems = 0;
        QList<QQuickItem*> pending{item(schemeEditor, QStringLiteral("colorTable"))};
        while (!pending.isEmpty()) {
            QQuickItem* child = pending.takeLast();
            pending += child->childItems();
            if (child->property("column").isValid() || colorHeaders.contains(child->property("text").toString())) {
                widths.insert(child->width());
                ++columnItems;
            }
        }
        QCOMPARE(columnItems, colorHeaders.size() + 3 * backend->colorNames().size());
        QCOMPARE(widths.size(), 1);
        QVERIFY(*widths.cbegin() > 0);
    }
    item(schemeEditor, QStringLiteral("descriptionEdit"))->setProperty("text", QStringLiteral("ProfileEditorTest edited"));
    backend->setColor(1, 2, QColor(1, 2, 3));
    backend->setTransparency(40);
    QMetaObject::invokeMethod(standardButton(schemeEditor, QStringLiteral("Apply")), "click");
    auto saved = ColorSchemeManager::instance()->findColorScheme(QStringLiteral("ProfileEditorTest edited"));
    QVERIFY(saved);
    QColor table[TABLE_COLORS];
    saved->getColorTable(table);
    QCOMPARE(table[1 + 2 * (TABLE_COLORS / 3)], QColor(1, 2, 3));
    QCOMPARE(qRound(saved->opacity() * 100), 60);
    // Saved schemes are chosen for the profile, shown at once.
    QCOMPARE(editor->value(QStringLiteral("ColorScheme")).toString(), QStringLiteral("ProfileEditorTest edited"));
    QCOMPARE(profile->colorScheme(), QStringLiteral("ProfileEditorTest edited"));

    // A colour cell opens QColorDialog transient for the editor.
    // It starts at that cell's colour.
    QPointer<QWidget> picker;
    QWindow* pickerParent = nullptr;
    QColor pickerColor;
    QTimer::singleShot(0, this, [&] {
        QTRY_VERIFY((picker = QApplication::activeModalWidget()));
        pickerParent = picker->windowHandle()->transientParent();
        pickerColor = picker->property("currentColor").value<QColor>();
        picker->close();
    });
    QMetaObject::invokeMethod(item(schemeEditor, QStringLiteral("colorTable")), "clickItem", Q_ARG(QVariant, 0), Q_ARG(QVariant, 1));
    QCOMPARE(pickerParent, schemeEditor);
    QCOMPARE(pickerColor, backend->color(0, 0));

    // Edit opens on the chosen scheme; a second editor replaces the first.
    QPointer<QQuickWindow> first = schemeEditor;
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("editColorSchemeButton")), "click");
    QTRY_VERIFY(!first);
    QQuickWindow* edit = nullptr;
    QTRY_VERIFY((edit = shownDialog(QStringLiteral("Edit Color Scheme"))));
    QCOMPARE(item(edit, QStringLiteral("descriptionEdit"))->property("text").toString(), QStringLiteral("ProfileEditorTest edited"));
    edit->close();
    editor->removeColorScheme(QStringLiteral("ProfileEditorTest edited"));
    QMetaObject::invokeMethod(standardButton(dialog, QStringLiteral("Cancel")), "click");
    QTRY_VERIFY(!dialog);
}

void ProfileEditorTest::testFonts()
{
    const Profile::Ptr profile = savedProfile(QStringLiteral("ProfileEditorTest fonts"));
    const QFont original = profile->font();
    QPointer<QQuickWindow> dialog = ProfileEditor::open(profile, ProfileEditor::ExistingProfile, nullptr);
    QVERIFY(dialog);
    QQuickItem* preview = item(dialog, QStringLiteral("fontPreview"));
    QCOMPARE(preview->property("text").toString(), QStringLiteral("%1 %2pt").arg(original.family()).arg(original.pointSize()));

    // FontDialog: modal over the dialog, its choice shown at once and undone by its Cancel.
    QFont bigger = original;
    bigger.setPointSize(original.pointSize() + 3);
    // FontDialog isn't exported: its signal by name, its font through its KFontChooser.
    QPointer<QDialog> fontDialog;
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("chooseFontButton")), "click");
    QTRY_VERIFY((fontDialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())));
    QVERIFY(fontDialog->inherits("Konsole::FontDialog"));
    QCOMPARE(fontDialog->windowHandle()->transientParent(), dialog.data());
    QMetaObject::invokeMethod(fontDialog, "fontChanged", Q_ARG(QFont, bigger));
    QCOMPARE(profile->font().pointSize(), bigger.pointSize());
    QCOMPARE(preview->property("text").toString(), QStringLiteral("%1 %2pt").arg(bigger.family()).arg(bigger.pointSize()));
    fontDialog->reject();
    QCOMPARE(profile->font().pointSize(), original.pointSize());

    // Its OK keeps it for Apply.
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("chooseFontButton")), "click");
    QTRY_VERIFY(fontDialog->isVisible());
    fontDialog->findChild<KFontChooser*>()->setFont(bigger);
    fontDialog->accept();
    QVERIFY(standardButton(dialog, QStringLiteral("Apply"))->isEnabled());
    QMetaObject::invokeMethod(standardButton(dialog, QStringLiteral("OK")), "click");
    QTRY_VERIFY(!dialog);
    QCOMPARE(profile->font().pointSize(), bigger.pointSize());
}

void ProfileEditorTest::testKeyBindings()
{
    const Profile::Ptr profile = savedProfile(QStringLiteral("ProfileEditorTest keys"));
    QPointer<QQuickWindow> dialog = ProfileEditor::open(profile, ProfileEditor::ExistingProfile, nullptr);
    QVERIFY(dialog);
    auto* editor = dialog->findChild<ProfileEditor*>();
    QVERIFY(names(editor->keyBindings()).contains(QStringLiteral("default")));

    // New: a modal editor over the dialog with Cancel as its default button.
    QMetaObject::invokeMethod(item(dialog, QStringLiteral("newKeyBindingsButton")), "click");
    QQuickWindow* keys = nullptr;
    QTRY_VERIFY((keys = shownDialog(QStringLiteral("New Key Binding List"))));
    QCOMPARE(keys->transientParent(), dialog.data());
    QCOMPARE(keys->modality(), Qt::ApplicationModal);
    auto* backend = keys->findChild<KeyBindingEditorBackend*>();
    QVERIFY(backend);
    QVERIFY(backend->rowCount() > 10);

    // The test area shows the entry a key matches.
    const QVariantMap shown = backend->testKey(Qt::Key_Up, Qt::NoModifier, QString());
    QVERIFY(!shown.value(QStringLiteral("input")).toString().isEmpty());
    QVERIFY(!shown.value(QStringLiteral("output")).toString().isEmpty());

    // Rows are edited, added and removed (F30 has no binding of its own).
    QVERIFY(backend->translator()->findEntry(Qt::Key_F30, Qt::NoModifier).isNull());
    const int rows = backend->rowCount();
    backend->addEntry();
    QCOMPARE(backend->rowCount(), rows + 1);
    backend->setEntry(rows, QStringLiteral("F30"), QStringLiteral("\"test\""));
    QCOMPARE(backend->translator()->findEntry(Qt::Key_F30, Qt::NoModifier).resultToString(), QStringLiteral("\"test\""));
    backend->removeEntries({rows});
    QCOMPARE(backend->rowCount(), rows);
    QVERIFY(backend->translator()->findEntry(Qt::Key_F30, Qt::NoModifier).isNull());

    // An empty description is refused with a message box over the editor.
    QQuickItem* description = item(keys, QStringLiteral("descriptionEdit"));
    description->setProperty("text", QString());
    // Polled without blocking: KMessageBox runs an event loop (its notification) before it
    // shows the box.
    QWindow* messageParent = nullptr;
    bool messageShown = false;
    QTimer poll;
    connect(&poll, &QTimer::timeout, this, [&] {
        const auto widgets = QApplication::topLevelWidgets();
        for (QWidget* widget : widgets) {
            if (widget->objectName() == QLatin1String("error") && widget->isVisible()) {
                messageShown = true;
                messageParent = widget->windowHandle()->transientParent();
                widget->close();
                poll.stop();
            }
        }
    });
    poll.start(50);
    QMetaObject::invokeMethod(standardButton(keys, QStringLiteral("OK")), "click");
    QTRY_VERIFY(messageShown);
    QCOMPARE(messageParent, keys);
    QVERIFY(keys->isVisible());

    // Saved under its description and chosen for the profile.
    description->setProperty("text", QStringLiteral("ProfileEditorTest bindings"));
    QPointer<QQuickWindow> guard(keys);
    QMetaObject::invokeMethod(standardButton(keys, QStringLiteral("OK")), "click");
    QTRY_VERIFY(!guard);
    QVERIFY(names(editor->keyBindings()).contains(QStringLiteral("ProfileEditorTest bindings")));
    QCOMPARE(editor->value(QStringLiteral("KeyBindings")).toString(), QStringLiteral("ProfileEditorTest bindings"));
    QQuickItem* remove = item(dialog, QStringLiteral("removeKeyBindingsButton"));
    QTRY_VERIFY(remove->isEnabled());
    QMetaObject::invokeMethod(standardButton(dialog, QStringLiteral("Apply")), "click");
    QCOMPARE(profile->keyBindings(), QStringLiteral("ProfileEditorTest bindings"));

    // A user's own scheme can be removed.
    QMetaObject::invokeMethod(remove, "click");
    QVERIFY(!names(editor->keyBindings()).contains(QStringLiteral("ProfileEditorTest bindings")));
    QMetaObject::invokeMethod(standardButton(dialog, QStringLiteral("Cancel")), "click");
    QTRY_VERIFY(!dialog);
}

QTEST_MAIN(ProfileEditorTest)

#include "ProfileEditorTest.moc"
