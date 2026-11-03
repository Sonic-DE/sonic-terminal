/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The QML dialogs keep every page, group, control, text, tooltip and choice of the widget
// dialogs they replace (DialogContent.h collects both sides).

#include "DialogContent.h"
#include "DialogCases.h"
#include "TestTheme.h"

#include "../quick/QmlDialogs.h"

#include <QDir>
#include <QPointer>
#include <QQmlComponent>
#include <QRadioButton>
#include <QTest>

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
    QTest::addColumn<int>("index");
    const QList<DialogCases::Case> cases = DialogCases::all();
    for (int i = 0; i < cases.size(); ++i) {
        QTest::newRow(cases.at(i).name) << i;
    }
}

void DialogContentTest::testDialog()
{
    QFETCH(int, index);
    const DialogCases::Case dialogCase = DialogCases::all().at(index);
    std::unique_ptr<QWidget> widgetDialog(dialogCase.widget());
    auto* pageDialog = qobject_cast<KPageDialog*>(widgetDialog.get());
    const QSet<QString> expected = pageDialog ? DialogContent::fromWidgetPages(pageDialog) : DialogContent::fromWidgets(widgetDialog.get());
    QVERIFY(!expected.isEmpty());

    QPointer<QQuickWindow> dialog = QmlDialogs::open(dialogCase.type, nullptr, dialogCase.properties(this));
    QVERIFY(dialog);
    QCOMPARE(dialog->title(), widgetDialog->windowTitle());
    const QSet<QString> found = pageDialog ? DialogContent::fromQmlPages(dialog) : DialogContent::fromQml(dialog);
    const QStringList missing = DialogContent::missing(expected, found);
    QVERIFY2(missing.isEmpty(), qPrintable(missing.join(u'\n')));
    dialog->close();
    QTRY_VERIFY(!dialog);
}

QTEST_MAIN(DialogContentTest)

#include "DialogContentTest.moc"
