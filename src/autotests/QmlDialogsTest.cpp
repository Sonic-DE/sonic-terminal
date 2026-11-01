/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// QML dialogs behave as QDialogs: transient for the window they were opened from, accepted
// or rejected by their buttons, keys and closing, deleted once closed; a PageDialog's
// search hides and outlines as KPageView's does.

#include "TestTheme.h"

#include "../MainWindow.h"
#include "../quick/QmlDialogs.h"
#include "../quick/WindowController.h"

#include <QDir>
#include <QPointer>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>

using namespace Konsole;

class QmlDialogsTest : public QObject {
    Q_OBJECT

public:
    static void initMain()
    {
        TestTheme::setUp();
    }

private Q_SLOTS:
    void testTransientForTheQmlWindow();
    void testTransientForAWidgetWindow();
    void testButtonsAndKeys_data();
    void testButtonsAndKeys();
    void testPageSearch();

private:
    static QQuickItem* findButton(QQuickItem* item, const QString& text);
};

QQuickItem* QmlDialogsTest::findButton(QQuickItem* item, const QString& text)
{
    const QList<QQuickItem*> children = item->childItems();
    for (QQuickItem* child : children) {
        if (child->inherits("QQuickAbstractButton") && child->property("text").toString().remove(u'&') == text) {
            return child;
        }
        if (QQuickItem* found = findButton(child, text)) {
            return found;
        }
    }
    return nullptr;
}

void QmlDialogsTest::testTransientForTheQmlWindow()
{
    auto* window = new MainWindow();
    auto* controller = new WindowController(window);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(controller->qmlWindow()));

    QPointer<QQuickWindow> dialog = QmlDialogs::open(QStringLiteral("DialogWindow"), window->centralWidget());
    QVERIFY(dialog);
    QCOMPARE(dialog->transientParent(), controller->qmlWindow());
    QCOMPARE(dialog->modality(), Qt::ApplicationModal);
    QVERIFY(dialog->isVisible());
    dialog->close();
    QTRY_VERIFY(!dialog);
    delete window;
}

void QmlDialogsTest::testTransientForAWidgetWindow()
{
    // A KPart host.
    QWidget host;
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    QPointer<QQuickWindow> dialog = QmlDialogs::open(QStringLiteral("DialogWindow"), &host, {}, Qt::NonModal);
    QVERIFY(dialog);
    QCOMPARE(dialog->transientParent(), host.windowHandle());
    QCOMPARE(dialog->modality(), Qt::NonModal);
    dialog->close();
    QTRY_VERIFY(!dialog);
}

void QmlDialogsTest::testButtonsAndKeys_data()
{
    QTest::addColumn<QString>("how");
    QTest::addColumn<bool>("accepted");
    QTest::newRow("OK") << QStringLiteral("OK") << true;
    QTest::newRow("Cancel") << QStringLiteral("Cancel") << false;
    QTest::newRow("Return") << QStringLiteral("Return") << true;
    QTest::newRow("Esc") << QStringLiteral("Esc") << false;
    QTest::newRow("close") << QStringLiteral("close") << false;
}

void QmlDialogsTest::testButtonsAndKeys()
{
    QFETCH(QString, how);
    QFETCH(bool, accepted);
    QWidget host;
    host.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host));
    // QDialogButtonBox::Ok | QDialogButtonBox::Cancel, the values QML's StandardButton shares.
    QPointer<QQuickWindow> dialog = QmlDialogs::open(QStringLiteral("DialogWindow"), &host, {{QStringLiteral("standardButtons"), 0x00000400 | 0x00400000}});
    QVERIFY(dialog);
    QVERIFY(QTest::qWaitForWindowExposed(dialog));
    QSignalSpy acceptedSpy(dialog, SIGNAL(accepted()));
    QSignalSpy rejectedSpy(dialog, SIGNAL(rejected()));
    if (how == QLatin1String("OK") || how == QLatin1String("Cancel")) {
        QQuickItem* button = findButton(dialog->contentItem(), how);
        QVERIFY2(button, qPrintable(how));
        QMetaObject::invokeMethod(button, "click");
    } else if (how == QLatin1String("Return") || how == QLatin1String("Esc")) {
        dialog->requestActivate();
        QTRY_VERIFY(dialog->isActive());
        QTest::keyClick(dialog, how == QLatin1String("Return") ? Qt::Key_Return : Qt::Key_Escape);
    } else {
        dialog->close();
    }
    QCOMPARE(acceptedSpy.count(), accepted ? 1 : 0);
    QCOMPARE(rejectedSpy.count(), accepted ? 0 : 1);
    QTRY_VERIFY(!dialog);
}

void QmlDialogsTest::testPageSearch()
{
    QQmlComponent component(QmlDialogs::engine());
    component.setData(R"(
        import QtQuick
        import QtQuick.Controls
        import org.sonicde.terminal
        PageDialog {
            DialogPage { title: "First"; CheckBox { objectName: "alpha"; text: "Alpha &option" } }
            DialogPage { title: "Second"; Label { objectName: "beta"; text: "Beta text" } }
            DialogPage { title: "Gamma page"; ComboBox { objectName: "choices"; model: ["One", "Delta"] } }
        }
    )",
        QUrl::fromLocalFile(QDir::temp().filePath(QStringLiteral("PageSearch.qml"))));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object, qPrintable(QString::number(component.status()) + u' ' + component.errorString()));
    auto* dialog = qobject_cast<QQuickWindow*>(object.get());
    dialog->show();
    QVERIFY(QTest::qWaitForWindowExposed(dialog));

    auto* search = dialog->findChild<QQuickItem*>(QStringLiteral("pageSearch"));
    QVERIFY(search);
    // Translated through the engine's localized context, not left as a call.
    QCOMPARE(search->property("placeholderText").toString(), QStringLiteral("Search…"));
    auto* title = dialog->findChild<QQuickItem*>(QStringLiteral("pageTitle"));
    QCOMPARE(title->property("text").toString(), QStringLiteral("First"));

    const auto hidden = [dialog] {
        QStringList titles;
        const QVariantList pages = dialog->property("hiddenPages").toList();
        for (const QVariant& page : pages) {
            titles.append(page.value<QObject*>()->property("title").toString());
        }
        return titles;
    };
    // A label's text: the other pages are hidden and the matching one is shown.
    search->setProperty("text", QStringLiteral("beta"));
    QTRY_COMPARE(hidden(), (QStringList{QStringLiteral("First"), QStringLiteral("Gamma page")}));
    QCOMPARE(title->property("text").toString(), QStringLiteral("Second"));
    QCOMPARE(dialog->property("outlines").toList().size(), 1);
    // A button's text without its accelerator marker, a combo box choice, a page name.
    search->setProperty("text", QStringLiteral("alpha option"));
    QTRY_COMPARE(hidden(), (QStringList{QStringLiteral("Second"), QStringLiteral("Gamma page")}));
    QCOMPARE(title->property("text").toString(), QStringLiteral("First"));
    search->setProperty("text", QStringLiteral("delta"));
    QTRY_COMPARE(hidden(), (QStringList{QStringLiteral("First"), QStringLiteral("Second")}));
    QTRY_COMPARE(dialog->property("outlines").toList().size(), 1);
    // Only the page's name matches: shown, nothing outlined.
    search->setProperty("text", QStringLiteral("gamma"));
    QTRY_COMPARE(dialog->property("outlines").toList().size(), 0);
    QCOMPARE(hidden(), (QStringList{QStringLiteral("First"), QStringLiteral("Second")}));
    // Cleared: every page again.
    search->setProperty("text", QString());
    QTRY_VERIFY(hidden().isEmpty());
}

QTEST_MAIN(QmlDialogsTest)

#include "QmlDialogsTest.moc"
