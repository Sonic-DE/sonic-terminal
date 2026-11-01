/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The QML dialogs keep every page, group, control, text, tooltip and choice of the widget
// dialogs they replace (DialogContent.h collects both sides).

#include "DialogContent.h"
#include "TestTheme.h"

#include "../CopyInputDialog.h"
#include "../HistorySizeDialog.h"
#include "../RenameTabDialog.h"
#include "../quick/CopyInputModel.h"
#include "../quick/QmlDialogs.h"

#include <KLocalizedString>

#include <QDir>
#include <QInputDialog>
#include <QPointer>
#include <QQmlComponent>
#include <QRadioButton>
#include <QTest>

#include <functional>
#include <memory>

using namespace Konsole;

class DialogContentTest : public QObject {
    Q_OBJECT

public:
    static void initMain()
    {
        TestTheme::setUp();
    }

private Q_SLOTS:
    void testWidgetCollector();
    void testQmlCollector();
    void testDialog_data();
    void testDialog();

private:
    std::unique_ptr<QQuickWindow> loadQml(const QByteArray& qml);
};

std::unique_ptr<QQuickWindow> DialogContentTest::loadQml(const QByteArray& qml)
{
    QQmlComponent component(QmlDialogs::engine());
    component.setData(qml, QUrl::fromLocalFile(QDir::temp().filePath(QStringLiteral("DialogContentFixture.qml"))));
    auto* window = qobject_cast<QQuickWindow*>(component.create());
    if (!window) {
        qWarning() << component.errorString();
    }
    return std::unique_ptr<QQuickWindow>(window);
}

void DialogContentTest::testWidgetCollector()
{
    // A dialog compared with itself misses nothing; without one of its controls, exactly
    // that control's entries.
    HistorySizeDialog dialog(nullptr);
    const QSet<QString> entries = DialogContent::fromWidgets(&dialog);
    QVERIFY(!entries.isEmpty());
    QVERIFY(DialogContent::missing(entries, entries).isEmpty());
    const QList<QRadioButton*> radios = dialog.findChildren<QRadioButton*>();
    QVERIFY(!radios.isEmpty());
    QRadioButton* removed = radios.constFirst();
    const QString entry = QStringLiteral("radio:") + DialogContent::clean(removed->text());
    QVERIFY(entries.contains(entry));
    QSet<QString> expected{entry};
    if (!removed->toolTip().isEmpty()) {
        bool shared = false;
        for (QWidget* other : dialog.findChildren<QWidget*>()) {
            shared = shared || (other != removed && other->toolTip() == removed->toolTip());
        }
        if (!shared) {
            expected.insert(QStringLiteral("tooltip:") + DialogContent::clean(removed->toolTip()));
        }
    }
    delete removed;
    const QStringList missing = DialogContent::missing(entries, DialogContent::fromWidgets(&dialog));
    QCOMPARE(QSet<QString>(missing.cbegin(), missing.cend()), expected);
    // An intended removal isn't reported.
    QVERIFY(DialogContent::missing(entries, DialogContent::fromWidgets(&dialog), expected).isEmpty());
}

void DialogContentTest::testQmlCollector()
{
    const QByteArray fixture = R"(
        import QtQuick
        import QtQuick.Controls
        import QtQuick.Controls as QQC2
        import QtQuick.Layouts
        import org.sonicde.terminal
        // org.sonicde.terminal has a TabBar of its own.
        PageDialog {
            standardButtons: DialogButtonBox.Ok
            DialogPage {
                title: "&First"
                CheckBox { text: "&Alpha"; ToolTip.text: "Alpha tip" }
                RadioButton { text: "Beta" }
                GroupBox { title: "Group"; Label { text: "Inside && out" } }
            }
            DialogPage {
                title: "Second"
                ComboBox { model: ["One", "Two"] }
                SpinBox { from: 1; to: 9; property string suffix: " lines" }
                QQC2.TabBar { TabButton { text: "Tab" } }
                Button { text: "Push" }
            }
        }
    )";
    std::unique_ptr<QQuickWindow> dialog = loadQml(fixture);
    QVERIFY(dialog);
    const QSet<QString> entries = DialogContent::fromQml(dialog.get());
    // The second page isn't the current one; its controls count too.
    const QSet<QString> expected{
        QStringLiteral("page:First"),
        QStringLiteral("page:Second"),
        QStringLiteral("check:Alpha"),
        QStringLiteral("tooltip:Alpha tip"),
        QStringLiteral("radio:Beta"),
        QStringLiteral("group:Group"),
        QStringLiteral("label:Inside & out"),
        QStringLiteral("choice:One"),
        QStringLiteral("choice:Two"),
        QStringLiteral("range:1-9"),
        QStringLiteral("suffix:lines"),
        QStringLiteral("tab:Tab"),
        QStringLiteral("button:Push"),
        QStringLiteral("button:OK"),
    };
    QVERIFY2(DialogContent::missing(expected, entries).isEmpty(), qPrintable(DialogContent::missing(expected, entries).join(u'\n')));
    // A control's own text isn't also a label.
    QVERIFY(!entries.contains(QStringLiteral("label:Alpha")));

    // Without the check box, its entries are reported missing.
    std::unique_ptr<QQuickWindow> without = loadQml(QByteArray(fixture).replace(R"(CheckBox { text: "&Alpha"; ToolTip.text: "Alpha tip" })", ""));
    QVERIFY(without);
    QCOMPARE(DialogContent::missing(expected, DialogContent::fromQml(without.get())),
        (QStringList{QStringLiteral("check:Alpha"), QStringLiteral("tooltip:Alpha tip")}));
}

void DialogContentTest::testDialog_data()
{
    // The widget dialog as its caller set it up, and the QML dialog replacing it.
    QTest::addColumn<std::function<QWidget*()>>("widget");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QVariantMap>("properties");

    QTest::newRow("Rename Tab") << std::function<QWidget*()>([] {
        auto* dialog = new RenameTabDialog;
        dialog->setColor(QColor());
        dialog->setActivityColor(QColor());
        return dialog;
    }) << QStringLiteral("RenameTabDialog")
                                << QVariantMap();
    QTest::newRow("History Size") << std::function<QWidget*()>([] {
        auto* dialog = new HistorySizeDialog;
        dialog->setMode(Enum::FixedSizeHistory);
        return dialog;
    }) << QStringLiteral("HistorySizeDialog")
                                  << QVariantMap{{QStringLiteral("mode"), Enum::FixedSizeHistory}};
    QTest::newRow("Copy Input") << std::function<QWidget*()>([] {
        return new CopyInputDialog;
    }) << QStringLiteral("CopyInputDialog")
                                << QVariantMap();
    // QInputDialog::getMultiLineText() (EditProfileDialog::showEnvironmentEditor()).
    QTest::newRow("Environment") << std::function<QWidget*()>([] {
        auto* dialog = new QInputDialog;
        dialog->setInputMode(QInputDialog::TextInput);
        dialog->setOptions(QInputDialog::UsePlainTextEditForTextInput);
        dialog->setWindowTitle(i18n("Edit Environment"));
        dialog->setLabelText(i18n("One environment variable per line"));
        return dialog;
    }) << QStringLiteral("TextInputDialog")
                                 << QVariantMap{{QStringLiteral("title"), i18n("Edit Environment")},
                                        {QStringLiteral("labelText"), i18n("One environment variable per line")},
                                        {QStringLiteral("multiLine"), true}};
    const QString editorLabel = i18n(
        "The format is e.g. 'editorExec PATH:LINE:COLUMN'\n\n"
        "PATH    will be replaced by the path to the text file\n"
        "LINE    will be replaced by the line number\n"
        "COLUMN  (optional) will be replaced by the column number\n"
        "Note: you will need to replace 'PATH:LINE:COLUMN' by the actual\n"
        "syntax the editor you want to use supports; e.g.:\n"
        "gedit +LINE:COLUMN PATH\n\n"
        "If PATH or LINE aren't present in the command, this setting\n"
        "will be ignored and the file will be opened by the default text\n"
        "editor.");
    QTest::newRow("Custom text editor") << std::function<QWidget*()>([editorLabel] {
        auto* dialog = new QInputDialog;
        dialog->setLabelText(editorLabel);
        dialog->setWindowTitle(i18n("Text Editor Custom Command"));
        return dialog;
    }) << QStringLiteral("TextInputDialog")
                                        << QVariantMap{{QStringLiteral("title"), i18n("Text Editor Custom Command")},
                                               {QStringLiteral("labelText"), editorLabel}};
}

void DialogContentTest::testDialog()
{
    QFETCH(std::function<QWidget*()>, widget);
    QFETCH(QString, type);
    QFETCH(QVariantMap, properties);
    std::unique_ptr<QWidget> widgetDialog(widget());
    const QSet<QString> expected = DialogContent::fromWidgets(widgetDialog.get());
    QVERIFY(!expected.isEmpty());

    if (type == QLatin1String("CopyInputDialog")) {
        properties.insert(QStringLiteral("sessions"), QVariant::fromValue(new CopyInputModel(this)));
    }
    QPointer<QQuickWindow> dialog = QmlDialogs::open(type, nullptr, properties);
    QVERIFY(dialog);
    QCOMPARE(dialog->title(), widgetDialog->windowTitle());
    const QStringList missing = DialogContent::missing(expected, DialogContent::fromQml(dialog));
    QVERIFY2(missing.isEmpty(), qPrintable(missing.join(u'\n')));
    dialog->close();
    QTRY_VERIFY(!dialog);
}

QTEST_MAIN(DialogContentTest)

#include "DialogContentTest.moc"
