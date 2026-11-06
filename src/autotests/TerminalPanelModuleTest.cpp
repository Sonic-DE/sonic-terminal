/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// A program with no link to the terminal's libraries shows the whole terminal as a panel does: in
// a QQuickView with a transparent colour, from the QML module on the import path.

#include <KActionCollection>
#include <KXmlGuiWindow>

#include <QAction>
#include <QApplication>
#include <QImage>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QToolBar>

class TerminalPanelModuleTest : public QObject {
    Q_OBJECT

public:
    static void initMain()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

private Q_SLOTS:
    void init();
    void cleanup();
    void testUsesTheTerminalsMenusAndToolBars();
    void testHidesWhatNeedsTheApplication();
    void testToolBarsMoveToTheBottom();
    void testSplitsAndTabs();
    void testOnlyTheTextAreaHasABackground();
    void testWindowShortcutsNeedTheFocus();
    void testToolBarAsksForTheHostsMenu();
    void testPressAnywhereIsReported();
    void testEndingTheLastShellIsReported();

private:
    QQuickItem* area() const;
    KXmlGuiWindow* compat() const;
    int terminalCount() const;

    QPointer<QQuickView> _view;
};

void TerminalPanelModuleTest::init()
{
    _view = new QQuickView;
    _view->engine()->addImportPath(QStringLiteral(TERMINAL_QML_DIR));
    _view->setResizeMode(QQuickView::SizeRootObjectToView);
    QQmlComponent component(_view->engine());
    component.setData(R"(
        import QtQuick
        import org.sonicde.terminal.app
        // What a panel could layer behind the widget.
        Rectangle {
            color: "#ff0000"
            property alias area: area
            TerminalPanelArea { id: area; anchors.fill: parent }
        })",
        QUrl());
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    auto* root = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(root);
    _view->setContent(QUrl(), nullptr, root);
    _view->resize(900, 400);
    _view->show();
    QVERIFY(QTest::qWaitForWindowExposed(_view));
    _view->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(_view));
    QVERIFY(compat());
    area()->forceActiveFocus();
    QTRY_VERIFY(area()->hasActiveFocus());
}

void TerminalPanelModuleTest::cleanup()
{
    delete _view;
}

QQuickItem* TerminalPanelModuleTest::area() const
{
    return _view && _view->rootObject() ? _view->rootObject()->property("area").value<QQuickItem*>() : nullptr;
}

KXmlGuiWindow* TerminalPanelModuleTest::compat() const
{
    return area() ? qobject_cast<KXmlGuiWindow*>(area()->property("compatWindow").value<QObject*>()) : nullptr;
}

int TerminalPanelModuleTest::terminalCount() const
{
    int count = 0;
    QList<QQuickItem*> items{area()};
    while (!items.isEmpty()) {
        QQuickItem* item = items.takeFirst();
        items.append(item->childItems());
        count += item->inherits("Konsole::TerminalView") && item->isVisible();
    }
    return count;
}

void TerminalPanelModuleTest::testUsesTheTerminalsMenusAndToolBars()
{
    QCOMPARE(compat()->componentName(), QStringLiteral("konsole"));
    QVERIFY(compat()->actionCollection()->action(QStringLiteral("new-tab")));
    const auto toolBars = compat()->findChildren<QToolBar*>();
    QVERIFY(!toolBars.isEmpty());
    for (const QToolBar* toolBar : toolBars) {
        QVERIFY(toolBar->isVisibleTo(compat()));
        QVERIFY(!toolBar->isMovable());
    }
}

void TerminalPanelModuleTest::testHidesWhatNeedsTheApplication()
{
    for (const char* name : {"new-window", "close-window", "detach-tab", "detach-view", "options_show_menubar"}) {
        QAction* action = compat()->actionCollection()->action(QString::fromLatin1(name));
        QVERIFY2(action, name);
        QVERIFY2(!action->isVisible(), name);
    }
}

void TerminalPanelModuleTest::testToolBarsMoveToTheBottom()
{
    const auto toolBars = compat()->findChildren<QToolBar*>();
    QVERIFY(area()->setProperty("toolBarsAtBottom", true));
    for (QToolBar* toolBar : toolBars) {
        QCOMPARE(compat()->toolBarArea(toolBar), Qt::BottomToolBarArea);
        QVERIFY(toolBar->isVisibleTo(compat()));
    }
    QVERIFY(area()->setProperty("toolBarsAtBottom", false));
    for (QToolBar* toolBar : toolBars) {
        QCOMPARE(compat()->toolBarArea(toolBar), Qt::TopToolBarArea);
    }
}

void TerminalPanelModuleTest::testSplitsAndTabs()
{
    QTRY_COMPARE(terminalCount(), 1);
    QAction* split = compat()->actionCollection()->action(QStringLiteral("split-view-left-right"));
    QVERIFY(split && split->isEnabled());
    split->trigger();
    QTRY_COMPARE(terminalCount(), 2);
    QAction* tab = compat()->actionCollection()->action(QStringLiteral("new-tab"));
    QVERIFY(tab && tab->isEnabled());
    tab->trigger();
    // The new tab's one terminal shows; the split ones are in the other tab.
    QTRY_COMPARE(terminalCount(), 1);
}

void TerminalPanelModuleTest::testOnlyTheTextAreaHasABackground()
{
    QTRY_COMPARE(terminalCount(), 1);
    QTest::qWait(200);
    const QImage image = _view->grabWindow();
    const QColor behind(255, 0, 0);
    // The right of the toolbar, past its buttons: the first lines hold only the toolbar.
    int behindPixels = 0;
    for (int x = image.width() / 3; x < image.width() / 2; ++x) {
        behindPixels += image.pixelColor(x, 12) == behind;
    }
    QVERIFY2(behindPixels > 0, "nothing behind the toolbar shows through");
    // The text area's middle.
    QVERIFY(image.pixelColor(image.width() / 2, image.height() / 2) != behind);
}

void TerminalPanelModuleTest::testWindowShortcutsNeedTheFocus()
{
    // Of the window's collection, which a host with one session doesn't bind.
    auto* action = new QAction(compat()->actionCollection());
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F9));
    compat()->actionCollection()->addAction(QStringLiteral("test_action"), action);
    QSignalSpy triggered(action, &QAction::triggered);

    QTest::qWait(50);
    QTest::keyClick(_view, Qt::Key_F9, Qt::ControlModifier | Qt::AltModifier);
    QTRY_COMPARE(triggered.count(), 1);

    _view->rootObject()->forceActiveFocus();
    QTRY_VERIFY(!area()->hasActiveFocus());
    QTest::qWait(50);
    QTest::keyClick(_view, Qt::Key_F9, Qt::ControlModifier | Qt::AltModifier);
    QTest::qWait(50);
    QCOMPARE(triggered.count(), 1);
}

void TerminalPanelModuleTest::testToolBarAsksForTheHostsMenu()
{
    QSignalSpy requested(area(), SIGNAL(toolBarContextMenuRequested(QQuickItem*, QPointF)));
    QVERIFY(requested.isValid());
    QTest::qWait(100);
    // An empty part of the toolbar.
    QTest::mouseClick(_view, Qt::RightButton, Qt::NoModifier, QPoint(_view->width() / 2, 12));
    QTRY_COMPARE(requested.count(), 1);
}

// The terminal has the scene's focus before any press, so a panel can't learn it from focus.
void TerminalPanelModuleTest::testPressAnywhereIsReported()
{
    QTRY_COMPARE(terminalCount(), 1);
    QTRY_VERIFY(area()->hasActiveFocus());
    QSignalSpy pressed(area(), SIGNAL(pressedInside()));
    QTest::qWait(100);
    QTest::mouseClick(_view, Qt::LeftButton, Qt::NoModifier, QPoint(_view->width() / 2, _view->height() / 2));
    QTRY_COMPARE(pressed.count(), 1);
    QVERIFY(area()->hasActiveFocus());
}

void TerminalPanelModuleTest::testEndingTheLastShellIsReported()
{
    QSignalSpy finished(area(), SIGNAL(finished()));
    QTRY_COMPARE(terminalCount(), 1);
    QVERIFY(QMetaObject::invokeMethod(area(), "sendInput", Q_ARG(QString, QStringLiteral("exit\n"))));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
    QVERIFY(!compat());
}

QTEST_MAIN(TerminalPanelModuleTest)

#include "TerminalPanelModuleTest.moc"
