/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The QML dialogs lay out their pages as the widget dialogs they replace do (DialogLayout.h):
// labels against their controls or not, controls stretched or not, wrapped labels as wide,
// and a default size no larger. Run once with the parity runs' theme and once with a large
// font and large icons (DIALOG_LAYOUT_SETUP=large), so a layout that only fits one setup
// shows.

#include "DialogLayout.h"
#include "DialogCases.h"
#include "TestTheme.h"

#include "../quick/QmlDialogs.h"

#include <QDir>
#include <QFormLayout>
#include <QLineEdit>
#include <QPointer>
#include <QQmlComponent>
#include <QTest>

#include <memory>

using namespace Konsole;

namespace {
// Joseph's decisions (terminal-panel-qml.md, "Intended layout differences"): on these pages
// every label sits just before its control.
QStringList besideControlPages(const QString& type)
{
    if (type == QLatin1String("EditProfileDialog")) {
        return {QStringLiteral("Scrolling"), QStringLiteral("Advanced")};
    }
    return {};
}
// Controls that decision moved next to their label (the .ui right-aligned them in their cell).
QStringList movedControls(const QString& type)
{
    if (type == QLatin1String("EditProfileDialog")) {
        return {QStringLiteral("Advanced|autoSaveIntervalWidget")};
    }
    return {};
}
// A QML dialog's default size may exceed the widget dialog's by this much (its own spacing
// and controls); a dialog sized by something else (a list's full height) goes far beyond it.
constexpr qreal sizeSlack = 1.15;
}

class DialogLayoutTest : public QObject {
    Q_OBJECT

public:
    static void initMain()
    {
        if (qEnvironmentVariable("DIALOG_LAYOUT_SETUP") == QLatin1String("large")) {
            TestTheme::setUpLarge();
        } else {
            TestTheme::setUp();
        }
    }

private Q_SLOTS:
    void testCollectors();
    void testDialog_data();
    void testDialog();

private:
    std::unique_ptr<QQuickWindow> loadQml(const QByteArray& qml);
};

std::unique_ptr<QQuickWindow> DialogLayoutTest::loadQml(const QByteArray& qml)
{
    QQmlComponent component(QmlDialogs::engine());
    component.setData(qml, QUrl::fromLocalFile(QDir::temp().filePath(QStringLiteral("DialogLayoutFixture.qml"))));
    auto* window = qobject_cast<QQuickWindow*>(component.create());
    if (!window) {
        qWarning() << component.errorString();
    }
    return std::unique_ptr<QQuickWindow>(window);
}

void DialogLayoutTest::testCollectors()
{
    // A form: its label right-aligned against a stretched field.
    QWidget form;
    auto* formLayout = new QFormLayout(&form);
    formLayout->setLabelAlignment(Qt::AlignRight);
    auto* field = new QLineEdit(&form);
    field->setObjectName(QStringLiteral("field"));
    formLayout->addRow(QStringLiteral("Name:"), field);
    form.resize(500, 100);
    form.show();
    QVERIFY(QTest::qWaitForWindowExposed(&form));
    const DialogLayout::Facts widgetFacts = DialogLayout::fromWidgetDialog(&form);
    QCOMPARE(widgetFacts.labelBesideControl.value(QStringLiteral("|Name:")), true);
    QCOMPARE(widgetFacts.stretched.value(QStringLiteral("|field")), true);

    // The same in QML, then with the label left in a wide column and the field at its size:
    // both differences are reported.
    const QByteArray fixture = R"(
        import QtQuick
        import QtQuick.Controls
        import QtQuick.Layouts
        Window {
            width: 500; height: 100; visible: true
            GridLayout {
                anchors.fill: parent
                columns: 2
                Label { text: "Name:"; Layout.alignment: Qt.AlignRight; Layout.preferredWidth: 200; horizontalAlignment: Text.AlignRight }
                TextField { objectName: "field"; Layout.fillWidth: true }
            }
        }
    )";
    std::unique_ptr<QQuickWindow> same = loadQml(fixture);
    QVERIFY(same);
    QVERIFY(QTest::qWaitForWindowExposed(same.get()));
    QVERIFY2(DialogLayout::differences(widgetFacts, DialogLayout::fromQmlDialog(same.get()), {}).isEmpty(),
        qPrintable(DialogLayout::differences(widgetFacts, DialogLayout::fromQmlDialog(same.get()), {}).join(u'\n')));

    std::unique_ptr<QQuickWindow> moved = loadQml(QByteArray(fixture)
            .replace("horizontalAlignment: Text.AlignRight", "horizontalAlignment: Text.AlignLeft")
            .replace("Layout.fillWidth: true", "Layout.fillWidth: false"));
    QVERIFY(moved);
    QVERIFY(QTest::qWaitForWindowExposed(moved.get()));
    const QStringList found = DialogLayout::differences(widgetFacts, DialogLayout::fromQmlDialog(moved.get()), {});
    QCOMPARE(found.size(), 2);
    QVERIFY(found.at(0).startsWith(QLatin1String("control |field")));
    QVERIFY(found.at(1).startsWith(QLatin1String("label |Name:")));
}

void DialogLayoutTest::testDialog_data()
{
    QTest::addColumn<int>("index");
    const QList<DialogCases::Case> cases = DialogCases::all();
    for (int i = 0; i < cases.size(); ++i) {
        QTest::newRow(cases.at(i).name) << i;
    }
}

void DialogLayoutTest::testDialog()
{
    QFETCH(int, index);
    const DialogCases::Case dialogCase = DialogCases::all().at(index);
    std::unique_ptr<QWidget> widgetDialog(dialogCase.widget());
    widgetDialog->show();
    QVERIFY(QTest::qWaitForWindowExposed(widgetDialog.get()));
    const QSize widgetSize = widgetDialog->size();
    const DialogLayout::Facts expected = DialogLayout::fromWidgetDialog(widgetDialog.get());
    widgetDialog->hide();

    // A dialog sized from the window it's opened from opens from one of the same size.
    QWidget* opener = widgetDialog->parentWidget();
    if (opener) {
        opener->winId();
    }
    QPointer<QQuickWindow> dialog = QmlDialogs::open(dialogCase.type, opener, dialogCase.properties(this));
    QVERIFY(dialog);
    QVERIFY(QTest::qWaitForWindowExposed(dialog));
    DialogLayout::settle();
    const QSize qmlSize = dialog->size();
    QStringList found = DialogLayout::differences(expected, DialogLayout::fromQmlDialog(dialog), besideControlPages(dialogCase.type), movedControls(dialogCase.type));
    if (qmlSize.width() > widgetSize.width() * sizeSlack || qmlSize.height() > widgetSize.height() * sizeSlack) {
        found.append(QStringLiteral("default size %1×%2, the original's %3×%4")
                .arg(qmlSize.width())
                .arg(qmlSize.height())
                .arg(widgetSize.width())
                .arg(widgetSize.height()));
    }
    QVERIFY2(found.isEmpty(), qPrintable(found.join(u'\n')));
    dialog->close();
    QTRY_VERIFY(!dialog);
}

QTEST_MAIN(DialogLayoutTest)

#include "DialogLayoutTest.moc"
