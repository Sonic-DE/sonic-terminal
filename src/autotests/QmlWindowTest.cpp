/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The QML window shows what its compat window lays out: the same terminal content as a
// widget window of the same size, the same geometry, focus through the facades, and close.

#include "ParityImages.h"
#include "TestTheme.h"

#include "../MainWindow.h"
#include "../ViewManager.h"
#include "../profile/ProfileManager.h"
#include "../quick/ChromeAdapter.h"
#include "../quick/DockAdapter.h"
#include "../quick/SearchTabsAdapter.h"
#include "../quick/TerminalView.h"
#include "../quick/TerminalViewRegistry.h"
#include "../quick/WindowController.h"
#include "../searchtabs/SearchTabs.h"
#include "../session/Session.h"
#include "../session/SessionController.h"
#include "../terminalDisplay/TerminalColor.h"
#include "../terminalDisplay/TerminalDisplay.h"
#include "../terminalDisplay/TerminalFonts.h"
#include "../terminalDisplay/TerminalScrollBar.h"
#include "../widgets/ViewContainer.h"

#include <KActionCollection>
#include <KToolBar>
#include <KXMLGUIFactory>

#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTabBar>
#include <QTabWidget>
#include <QTest>
#include <QToolBar>

using namespace Konsole;

namespace {
struct PaintCounter : QObject {
    int count = 0;
    bool eventFilter(QObject*, QEvent* event) override
    {
        count += event->type() == QEvent::Paint;
        return false;
    }
};
}

class QmlWindowTest : public QObject {
    Q_OBJECT

public:
    static void initMain()
    {
        TestTheme::setUp();
    }

private Q_SLOTS:
    void initTestCase()
    {
        // The container hides its tab bar in any other application (the KPart).
        QCoreApplication::setApplicationName(QStringLiteral("konsole"));
    }

private Q_SLOTS:
    void init();
    void cleanup();
    void testTerminalLooksLikeTheWidgetWindow();
    void testSplitPanesLookLikeTheWidgetWindow();
    void testChromeFollowsTheWidgets();
    void testKeys();
    void testSearchTabs();
    void testPluginDock();
    void testClosedDockRoomIsRepainted();
    void testDialogsAreTransientForTheQmlWindow();
    void testHamburgerMenuOpensUnderItsButton();
    void testFirstTerminalHasFocus();
    void testClicksKeepTheTerminalFocus();
    void testUnlockedToolBarDragsToAnEdge();
    void testGeometryFollows();
    void testFocusBridge();
    void testCloseClosesTheCompatWindow();

private:
    static TerminalDisplay* displayOf(MainWindow* window);
    static QRect cursorCell(TerminalDisplay* display);

    QPointer<MainWindow> _widgetWindow;
    QPointer<MainWindow> _compatWindow;
    QPointer<WindowController> _controller;
};

void QmlWindowTest::init()
{
    _widgetWindow = new MainWindow();
    _compatWindow = new MainWindow();
    _controller = new WindowController(_compatWindow);
    QVERIFY(_controller->qmlWindow());
    for (MainWindow* window : {_widgetWindow.data(), _compatWindow.data()}) {
        window->createSession(ProfileManager::instance()->defaultProfile(), QDir::tempPath());
        window->resize(900, 600);
        window->show();
    }
    QVERIFY(QTest::qWaitForWindowExposed(_widgetWindow));
    QVERIFY(QTest::qWaitForWindowExposed(_controller->qmlWindow()));
    QVERIFY(TerminalViewRegistry::view(displayOf(_compatWindow))->sceneMode());
    QVERIFY(!TerminalViewRegistry::view(displayOf(_widgetWindow))->sceneMode());
}

void QmlWindowTest::cleanup()
{
    delete _widgetWindow;
    delete _compatWindow;
}

TerminalDisplay* QmlWindowTest::displayOf(MainWindow* window)
{
    const QList<TerminalDisplay*> displays = window->centralWidget()->findChildren<TerminalDisplay*>();
    return displays.isEmpty() ? nullptr : displays.constFirst();
}

QRect QmlWindowTest::cursorCell(TerminalDisplay* display)
{
    const int width = display->terminalFont()->fontWidth();
    const int height = display->terminalFont()->fontHeight();
    const QPoint cursor = TerminalViewRegistry::view(display)->cursorPosition();
    return QRect(display->contentRect().topLeft() + QPoint(cursor.x() * width, cursor.y() * height), QSize(width, height)).adjusted(-2, -2, 2, 2);
}

void QmlWindowTest::testTerminalLooksLikeTheWidgetWindow()
{
    for (MainWindow* window : {_widgetWindow.data(), _compatWindow.data()}) {
        Session* session = TerminalViewRegistry::view(displayOf(window))->sessionController()->session();
        session->sendTextToTerminal(QStringLiteral("printf '\\033[2J\\033[Hred \\033[31mtext\\033[0m and \\033[1mbold\\033[0m\\n'"), QLatin1Char('\r'));
    }
    QTRY_VERIFY(TerminalViewRegistry::view(displayOf(_compatWindow))->sessionController()->session()->getAllDisplayedText().contains(QLatin1String("red text and bold")));
    QTRY_VERIFY(TerminalViewRegistry::view(displayOf(_widgetWindow))->sessionController()->session()->getAllDisplayedText().contains(QLatin1String("red text and bold")));
    QTest::qWait(1500); // the first layout's size notification

    // The compat central widget lies where the QML chrome leaves the central area (the hidden
    // bars take the QML bars' extents); the terminal gets the widget window's size.
    QWidget* widgetCentral = _widgetWindow->centralWidget();
    QWidget* compatCentral = _compatWindow->centralWidget();
    const QRect central(compatCentral->mapTo(_compatWindow, QPoint(0, 0)), compatCentral->size());
    auto* centralArea = _controller->qmlWindow()->findChild<QQuickItem*>(QStringLiteral("centralArea"));
    QVERIFY(centralArea);
    QTRY_COMPARE(centralArea->mapRectToScene(centralArea->boundingRect()).toRect(), central);
    QCOMPARE(compatCentral->size(), widgetCentral->size());
    QCOMPARE(displayOf(_compatWindow)->geometry(), displayOf(_widgetWindow)->geometry());
    const QImage widgetImage = widgetCentral->grab().toImage().convertToFormat(QImage::Format_ARGB32);
    const QImage sceneImage = _controller->qmlWindow()->grabWindow().copy(central).convertToFormat(QImage::Format_ARGB32);

    TerminalDisplay* display = displayOf(_widgetWindow);
    const QPoint offset = display->mapTo(widgetCentral, QPoint(0, 0));
    const QRect scrollBar = display->scrollBar()->isHidden() ? QRect() : display->scrollBar()->geometry().translated(offset);
    const QString difference = ParityImages::compare(widgetImage, sceneImage, {scrollBar}, {}, {cursorCell(display).translated(offset)});
    QVERIFY2(difference.isEmpty(), qPrintable(difference));
}

void QmlWindowTest::testSplitPanesLookLikeTheWidgetWindow()
{
    for (MainWindow* window : {_widgetWindow.data(), _compatWindow.data()}) {
        Session* session = TerminalViewRegistry::view(displayOf(window))->sessionController()->session();
        session->sendTextToTerminal(QStringLiteral("printf '\\033[2J\\033[Hfirst line\\nsecond line\\n'"), QLatin1Char('\r'));
    }
    QTRY_VERIFY(TerminalViewRegistry::view(displayOf(_compatWindow))->sessionController()->session()->getAllDisplayedText().contains(QLatin1String("second line")));
    QTRY_VERIFY(TerminalViewRegistry::view(displayOf(_widgetWindow))->sessionController()->session()->getAllDisplayedText().contains(QLatin1String("second line")));
    QTest::qWait(1500);
    // The pane that was split shrinks; the new one opens beside it.
    for (MainWindow* window : {_widgetWindow.data(), _compatWindow.data()}) {
        QAction* split = nullptr;
        const auto clients = window->guiFactory()->clients();
        for (KXMLGUIClient* client : clients) {
            split = split ? split : client->actionCollection()->action(QStringLiteral("split-view-left-right"));
        }
        QVERIFY(split);
        split->trigger();
    }
    QTRY_COMPARE(_compatWindow->centralWidget()->findChildren<TerminalDisplay*>().size(), 2);
    QTRY_COMPARE(_widgetWindow->centralWidget()->findChildren<TerminalDisplay*>().size(), 2);
    QTest::qWait(1500);

    const auto byPosition = [](MainWindow* window) {
        QList<TerminalDisplay*> displays = window->centralWidget()->findChildren<TerminalDisplay*>();
        std::sort(displays.begin(), displays.end(), [window](TerminalDisplay* a, TerminalDisplay* b) {
            return a->mapTo(window, QPoint(0, 0)).x() < b->mapTo(window, QPoint(0, 0)).x();
        });
        return displays;
    };
    const QList<TerminalDisplay*> widgetDisplays = byPosition(_widgetWindow);
    const QList<TerminalDisplay*> compatDisplays = byPosition(_compatWindow);
    const QImage widgetImage = _widgetWindow->grab().toImage().convertToFormat(QImage::Format_ARGB32);
    const QImage sceneImage = _controller->qmlWindow()->grabWindow().convertToFormat(QImage::Format_ARGB32);
    for (int i = 0; i < 2; ++i) {
        // The terminal text area, without the header bar, scroll bar and cursor.
        TerminalDisplay* widget = widgetDisplays.at(i);
        TerminalDisplay* compat = compatDisplays.at(i);
        // The Kirigami chrome can leave the panes a little shorter; compare what both show.
        const QSize common = widget->contentRect().size().boundedTo(compat->contentRect().size());
        const QRect widgetRect = QRect(widget->contentRect().topLeft(), common).translated(widget->mapTo(_widgetWindow, QPoint(0, 0)));
        const QRect sceneRect = QRect(compat->contentRect().topLeft(), common).translated(compat->mapTo(_compatWindow, QPoint(0, 0)));
        const QRect cursor = cursorCell(widget);
        QList<QRect> scrollBars;
        for (TerminalDisplay* display : {widget, compat}) {
            if (!display->scrollBar()->isHidden()) {
                scrollBars.append(display->scrollBar()->geometry().translated(-display->contentRect().topLeft()));
            }
        }
        const QString difference = ParityImages::compare(widgetImage.copy(widgetRect), sceneImage.copy(sceneRect), scrollBars, {},
            {cursor.translated(-widget->contentRect().topLeft())});
        QVERIFY2(difference.isEmpty(), qPrintable(QStringLiteral("pane %1: %2").arg(i).arg(difference)));
    }
}

void QmlWindowTest::testChromeFollowsTheWidgets()
{
    QQuickWindow* qml = _controller->qmlWindow();
    auto* chrome = qml->findChild<ChromeAdapter*>();
    QVERIFY(chrome);

    // Menu bar entries and toolbar actions, in order and state, as the widget window has them.
    QStringList widgetMenus;
    for (QAction* action : _widgetWindow->menuBar()->actions()) {
        if (action->menu() && action->isVisible()) {
            widgetMenus.append(action->text());
        }
    }
    QStringList qmlMenus;
    for (const QVariant& menu : chrome->menus()) {
        qmlMenus.append(menu.toMap().value(QStringLiteral("text")).toString());
    }
    QVERIFY(!widgetMenus.isEmpty());
    QCOMPARE(qmlMenus, widgetMenus);
    QCOMPARE(chrome->menuBarVisible(), _widgetWindow->menuBar()->isVisible());

    // The QML window draws a button for every action of the compat toolbars, as they are at startup.
    const auto drawnActions = [qml] {
        QStringList names;
        std::function<void(QQuickItem*)> collect = [&](QQuickItem* item) {
            const auto* action = qobject_cast<QAction*>(item->property("qAction").value<QObject*>());
            if (action && item->isVisible() && item->width() > 0) {
                names.append(action->objectName());
            }
            for (QQuickItem* child : item->childItems()) {
                collect(child);
            }
        };
        collect(qml->contentItem());
        return names;
    };
    const auto compatActions = [this] {
        QStringList names;
        for (QToolBar* toolBar : _compatWindow->findChildren<QToolBar*>(Qt::FindDirectChildrenOnly)) {
            for (QAction* action : toolBar->actions()) {
                if (!toolBar->isHidden() && action->isVisible() && !action->isSeparator() && toolBar->widgetForAction(action)
                    && qobject_cast<QToolButton*>(toolBar->widgetForAction(action))) {
                    names.append(action->objectName());
                }
            }
        }
        return names;
    };
    QVERIFY(compatActions().contains(QStringLiteral("new-tab")));
    QTRY_COMPARE(drawnActions(), compatActions());

    for (MainWindow* window : {_widgetWindow.data(), _compatWindow.data()}) {
        for (QToolBar* toolBar : window->findChildren<QToolBar*>(Qt::FindDirectChildrenOnly)) {
            toolBar->show();
        }
    }

    const auto actionsOf = [](const QList<QToolBar*>& toolBars) {
        QStringList names;
        for (QToolBar* toolBar : toolBars) {
            if (toolBar->isHidden()) {
                continue;
            }
            for (QAction* action : toolBar->actions()) {
                if (action->isVisible() && !action->isSeparator()) {
                    names.append(action->objectName() + (action->isEnabled() ? QStringLiteral(":on") : QStringLiteral(":off")));
                }
            }
        }
        return names;
    };
    QVERIFY(!actionsOf(_widgetWindow->findChildren<QToolBar*>(Qt::FindDirectChildrenOnly)).isEmpty());
    // The window's and the session's actions share one toolbar.
    const QList<QToolBar*> toolBars = _widgetWindow->findChildren<QToolBar*>(Qt::FindDirectChildrenOnly);
    QCOMPARE(toolBars.size(), 1);
    QCOMPARE(toolBars.first()->objectName(), QStringLiteral("mainToolBar"));
    const QStringList toolBarActions = actionsOf(toolBars);
    QVERIFY(toolBarActions.contains(QStringLiteral("new-tab:on")));
    QVERIFY(std::any_of(toolBarActions.cbegin(), toolBarActions.cend(), [](const QString& name) {
        return name.startsWith(QStringLiteral("edit_paste:"));
    }));
    // New Tab and Split View lead it, also after the GUI is rebuilt (session switch, Configure Toolbars).
    const auto leading = [this] {
        const QList<QAction*> actions = _widgetWindow->findChild<QToolBar*>(QStringLiteral("mainToolBar"))->actions();
        return QStringList{actions.value(0)->objectName(), actions.value(1)->objectName()};
    };
    const QStringList fixed{QStringLiteral("new-tab"), QStringLiteral("split-view")};
    QCOMPARE(leading(), fixed);
    for (KXMLGUIClient* client : _widgetWindow->guiFactory()->clients()) {
        if (client != _widgetWindow.data()) {
            _widgetWindow->guiFactory()->removeClient(client);
            _widgetWindow->guiFactory()->addClient(client);
            break;
        }
    }
    QCOMPARE(leading(), fixed);
    QVERIFY(QMetaObject::invokeMethod(_widgetWindow.data(), "saveNewToolbarConfig"));
    QCOMPARE(_widgetWindow->findChildren<QToolBar*>(Qt::FindDirectChildrenOnly).size(), 1);
    QCOMPARE(leading(), fixed);
    // A tab opened and closed again (its session's actions replace the first's in the
    // toolbar, then the first's come back) leaves the toolbar as it was.
    {
        const auto order = [this] {
            QStringList names;
            for (QAction* action : _widgetWindow->findChild<QToolBar*>(QStringLiteral("mainToolBar"))->actions()) {
                names.append(action->objectName());
            }
            return names;
        };
        const auto sessionClient = [this]() -> SessionController* {
            for (KXMLGUIClient* client : _widgetWindow->guiFactory()->clients()) {
                if (auto controller = dynamic_cast<SessionController*>(client)) {
                    return controller;
                }
            }
            return nullptr;
        };
        // A tab's client is plugged when its view takes the focus.
        _widgetWindow->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(_widgetWindow));
        const QStringList before = order();
        SessionController* first = sessionClient();
        QVERIFY(first);
        _widgetWindow->newTab();
        SessionController* opened = nullptr;
        QTRY_VERIFY((opened = sessionClient()) && opened != first);
        QCOMPARE(leading(), fixed);
        QAction* close = opened->actionCollection()->action(QStringLiteral("close-session"));
        QVERIFY(close);
        close->trigger();
        QTRY_COMPARE(sessionClient(), first);
        QCOMPARE(order(), before);
    }
    QStringList qmlActions;
    QTRY_VERIFY(!chrome->topLines().isEmpty());
    for (const QVariant& line : chrome->topLines()) {
        for (const QVariant& toolBar : line.toList()) {
            for (const QVariant& item : toolBar.toMap().value(QStringLiteral("items")).toList()) {
                const auto* action = qobject_cast<QAction*>(item.toMap().value(QStringLiteral("action")).value<QObject*>());
                if (action && !action->isSeparator()) {
                    qmlActions.append(action->objectName() + (action->isEnabled() ? QStringLiteral(":on") : QStringLiteral(":off")));
                }
            }
        }
    }
    QTRY_COMPARE(qmlActions, actionsOf(_widgetWindow->findChildren<QToolBar*>(Qt::FindDirectChildrenOnly)));

    // The tabs: one QML tab per tab of the hidden bar, at its place, with its title.
    for (MainWindow* window : {_widgetWindow.data(), _compatWindow.data()}) {
        window->createSession(ProfileManager::instance()->defaultProfile(), QDir::tempPath());
        window->createSession(ProfileManager::instance()->defaultProfile(), QDir::tempPath());
    }
    QTabBar* tabBar = qobject_cast<QTabWidget*>(_compatWindow->centralWidget())->tabBar();
    auto* qmlTabBar = qml->findChild<QQuickItem*>(QStringLiteral("tabBarControl"));
    QVERIFY(qmlTabBar);
    QTRY_COMPARE(qmlTabBar->property("count").toInt(), 3);
    QTRY_VERIFY(qmlTabBar->isVisible());
    const QPoint tabBarInWindow = tabBar->mapTo(_compatWindow, QPoint(0, 0));
    for (int i = 0; i < 3; ++i) {
        QQuickItem* tab = nullptr;
        QMetaObject::invokeMethod(qmlTabBar, "itemAt", Q_RETURN_ARG(QQuickItem*, tab), Q_ARG(int, i));
        QVERIFY(tab);
        QTRY_COMPARE(tab->mapRectToScene(tab->boundingRect()).toRect(), tabBar->tabRect(i).translated(tabBarInWindow));
        // The titles fit their tabs here, so nothing is elided.
        QString label = tabBar->tabText(i);
        label.remove(QRegularExpression(QStringLiteral("&(?!&)")));
        QTRY_COMPARE(tab->property("tabData").toMap().value(QStringLiteral("label")).toString(), label);
    }

    // Clicking a QML tab switches the real one.
    QVERIFY(QTest::qWaitForWindowExposed(qml));
    QTRY_COMPARE(tabBar->currentIndex(), 2);
    const QPoint first = tabBar->tabRect(0).translated(tabBarInWindow).center();
    QTest::mouseClick(qml, Qt::LeftButton, Qt::NoModifier, first);
    QTRY_COMPARE(tabBar->currentIndex(), 0);
    QTRY_COMPARE(qmlTabBar->property("currentIndex").toInt(), 0);
}

void QmlWindowTest::testKeys()
{
    QQuickWindow* qml = _controller->qmlWindow();
    qml->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(qml));
    TerminalView* view = TerminalViewRegistry::view(displayOf(_compatWindow));
    view->forceActiveFocus();
    QTRY_VERIFY(view->hasActiveFocus());
    Session* session = view->sessionController()->session();

    // Kirigami's quit (Ctrl+Q) and page navigation (Alt+Left/Right) leave these keys to the
    // terminal: the shell gets what the widget window sends it.
    const auto sendKeys = [](QWindow* window) {
        QTest::keyClick(window, Qt::Key_Q, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_Left, Qt::AltModifier);
        QTest::keyClick(window, Qt::Key_Right, Qt::AltModifier);
        QTest::keyClick(window, Qt::Key_Return);
    };
    const auto lastLine = [](Session* session) {
        const QStringList lines = session->getAllDisplayedTextList(true);
        return lines.isEmpty() ? QString() : lines.last();
    };
    Session* widgetSession = TerminalViewRegistry::view(displayOf(_widgetWindow))->sessionController()->session();
    for (Session* each : {widgetSession, session}) {
        each->sendTextToTerminal(QStringLiteral("stty -ixon -echo; exec cat -v"), QLatin1Char('\r'));
    }
    QTest::qWait(500);
    _widgetWindow->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(_widgetWindow));
    displayOf(_widgetWindow)->setFocus();
    sendKeys(_widgetWindow->windowHandle());
    QTRY_VERIFY(lastLine(widgetSession).contains(QLatin1String("^[[1;3D^[[1;3C")));
    qml->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(qml));
    view->forceActiveFocus();
    sendKeys(qml);
    QTRY_COMPARE(lastLine(session), lastLine(widgetSession));

    // The compat window's action shortcuts fire in the QML window...
    QTabBar* tabBar = qobject_cast<QTabWidget*>(_compatWindow->centralWidget())->tabBar();
    const int tabs = tabBar->count();
    QTest::keyClick(qml, Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_COMPARE(tabBar->count(), tabs + 1);

    // ...but not while a menu is open (the menu is the focus window, as in the widget window).
    auto* chrome = qml->findChild<ChromeAdapter*>();
    QVERIFY(chrome);
    _compatWindow->menuBar()->show();
    QTRY_VERIFY(!chrome->menus().isEmpty());
    QTRY_VERIFY(!chrome->menuBarItems().isEmpty());
    chrome->popupMenu(0);
    QTRY_VERIFY(chrome->openMenu() && chrome->openMenu()->isVisible());
    QTRY_VERIFY(QGuiApplication::focusWindow() != qml);
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier);
    QTest::qWait(300);
    QCOMPARE(tabBar->count(), tabs + 1);
    chrome->openMenu()->hide();
}

void QmlWindowTest::testSearchTabs()
{
    QQuickWindow* qml = _controller->qmlWindow();
    qml->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(qml));
    for (int i = 0; i < 2; ++i) {
        _compatWindow->createSession(ProfileManager::instance()->defaultProfile(), QDir::tempPath());
    }
    auto* container = qobject_cast<TabbedViewContainer*>(_compatWindow->centralWidget());
    QTRY_COMPARE(container->currentIndex(), 2);
    auto* search = qml->findChild<SearchTabsAdapter*>();
    QVERIFY(search);

    // Opened in the QML window, not as the widget popup in the hidden window.
    container->searchTabs();
    QTRY_VERIFY(search->isOpen());
    // SearchTabs has no Q_OBJECT, so findChild() can't name it.
    const QList<QFrame*> frames = _compatWindow->findChildren<QFrame*>();
    QVERIFY(std::none_of(frames.cbegin(), frames.cend(), [](QFrame* frame) {
        return dynamic_cast<SearchTabs*>(frame);
    }));
    QCOMPARE(search->entries().size(), 3);
    QCOMPARE(search->currentIndex(), 1);
    QVERIFY(search->geometry().isValid());
    // Return switches to the preselected entry (the second tab) and closes.
    QTest::keyClick(qml, Qt::Key_Return);
    QTRY_VERIFY(!search->isOpen());
    QTRY_COMPARE(container->currentIndex(), 1);

    // Typing filters; Escape closes and gives the terminal its focus back.
    container->searchTabs();
    QTRY_VERIFY(search->isOpen());
    for (int i = 0; i < 4; ++i) {
        QTest::keyClick(qml, Qt::Key_Z);
    }
    QTRY_VERIFY(search->entries().isEmpty());
    QTest::keyClick(qml, Qt::Key_Escape);
    QTRY_VERIFY(!search->isOpen());
    TerminalView* active = TerminalViewRegistry::view(container->connectedViewManager()->activeViewController()->view());
    QVERIFY(active);
    QTRY_VERIFY(active->hasActiveFocus());
}

void QmlWindowTest::testPluginDock()
{
    QQuickWindow* qml = _controller->qmlWindow();
    // As the widget plugins add theirs (SSHManager, the compat test plugin).
    auto* dock = new QDockWidget(_compatWindow);
    dock->setWindowTitle(QStringLiteral("Plugin dock"));
    auto* content = new QLabel(QStringLiteral("plugin content"));
    PaintCounter painted;
    content->installEventFilter(&painted);
    dock->setWidget(content);
    _compatWindow->addDockWidget(Qt::LeftDockWidgetArea, dock);

    auto* docks = qml->findChild<DockAdapter*>();
    QVERIFY(docks);
    QTRY_COMPARE(docks->docks().size(), 1);
    const QVariantMap entry = docks->docks().constFirst().toMap();
    QCOMPARE(entry.value(QStringLiteral("title")).toString(), QStringLiteral("Plugin dock"));
    // The plugin's widget is embedded in the QML window and shown; the dock keeps its room.
    QVERIFY(content->windowHandle());
    QTRY_COMPARE(content->windowHandle()->parent(), qml);
    QTRY_VERIFY(content->isVisible());
    // It paints at the size it is laid out at.
    QTRY_VERIFY(content->height() > 100);
    painted.count = 0;
    content->update();
    QTRY_VERIFY(painted.count > 0);
    QWidget* compatCentral = _compatWindow->centralWidget();
    const QRect central(compatCentral->mapTo(_compatWindow, QPoint(0, 0)), compatCentral->size());
    QVERIFY(central.left() > 0);
    auto* centralArea = qml->findChild<QQuickItem*>(QStringLiteral("centralArea"));
    QTRY_COMPARE(centralArea->mapRectToScene(centralArea->boundingRect()).toRect(), central);

    // The QML title's close button closes the real dock; the plugin's show() brings it back.
    docks->closeDock(dock);
    QTRY_VERIFY(!dock->isVisible());
    QTRY_VERIFY(!content->isVisible());
    // The terminal takes the dock's room back, in the compat window and the QML window.
    const auto centralNow = [compatCentral, this] {
        return QRect(compatCentral->mapTo(_compatWindow, QPoint(0, 0)), compatCentral->size());
    };
    QTRY_COMPARE(centralNow().left(), 0);
    QTRY_COMPARE(centralNow().width(), _compatWindow->width());
    QTRY_COMPARE(centralArea->mapRectToScene(centralArea->boundingRect()).toRect(), centralNow());
    TerminalView* view = TerminalViewRegistry::view(displayOf(_compatWindow));
    QTRY_COMPARE(view->mapRectToScene(view->boundingRect()).toRect().width(), centralNow().width());
    dock->show();
    QTRY_VERIFY(content->isVisible());
}

void QmlWindowTest::testClosedDockRoomIsRepainted()
{
    // A dock on the right (SSHManager's place in the report), opened and closed while the
    // shell prints nothing: the terminal paints the room the dock leaves.
    QQuickWindow* qml = _controller->qmlWindow();
    QVERIFY(QTest::qWaitForWindowExposed(qml));
    auto* dock = new QDockWidget(_compatWindow);
    dock->setWidget(new QLabel(QStringLiteral("right dock")));
    _compatWindow->addDockWidget(Qt::RightDockWidgetArea, dock);
    TerminalDisplay* display = displayOf(_compatWindow);
    TerminalView* view = TerminalViewRegistry::view(display);
    QWidget* compatCentral = _compatWindow->centralWidget();
    QTRY_VERIFY(compatCentral->width() < _compatWindow->width() - 50);
    QTest::qWait(500);
    dock->close();
    QTRY_COMPARE(compatCentral->width(), _compatWindow->width());
    QTRY_COMPARE(qRound(view->width()), display->width());
    // The strip the dock covered, inside the terminal's content and left of its scroll bar.
    const QColor background = display->terminalColor()->backgroundColor();
    const auto stripPixel = [qml, view, display] {
        const QRect content = display->contentRect();
        const QPointF point(content.right() - 40, content.bottom() - 20);
        return qml->grabWindow().pixelColor(view->mapToScene(point).toPoint());
    };
    QTRY_COMPARE(stripPixel().rgb(), background.rgb());
    delete dock;
}

void QmlWindowTest::testDialogsAreTransientForTheQmlWindow()
{
    // As the frozen classes parent their dialogs and message boxes: to the window or a
    // terminal in it.
    for (QWidget* parent : {static_cast<QWidget*>(_compatWindow), static_cast<QWidget*>(displayOf(_compatWindow))}) {
        QDialog dialog(parent);
        dialog.show();
        QVERIFY(dialog.windowHandle());
        QCOMPARE(dialog.windowHandle()->transientParent(), _controller->qmlWindow());
        dialog.hide();
    }

    // Or to the active window (QApplication::activeWindow() in the widget window), here the
    // compat window behind the active QML window: the rename tab dialog, now QML.
    QQuickWindow* qml = _controller->qmlWindow();
    qml->requestActivate();
    QTRY_VERIFY(qml->isActive());
    QAction* rename = nullptr;
    const auto clients = _compatWindow->guiFactory()->clients();
    for (KXMLGUIClient* client : clients) {
        rename = rename ? rename : client->actionCollection()->action(QStringLiteral("rename-session"));
    }
    QVERIFY(rename);
    rename->trigger();
    QQuickWindow* renameDialog = nullptr;
    QTRY_VERIFY((renameDialog = qobject_cast<QQuickWindow*>(QGuiApplication::modalWindow())));
    QCOMPARE(renameDialog->transientParent(), qml);
    renameDialog->close();
}

void QmlWindowTest::testHamburgerMenuOpensUnderItsButton()
{
    // KHamburgerMenu uses its button only in an active window, which a compat window never
    // is; the QML toolbar's button takes its place.
    QQuickWindow* qml = _controller->qmlWindow();
    qml->requestActivate();
    QTRY_VERIFY(qml->isActive());
    QAction* hamburger = _compatWindow->actionCollection()->action(QStringLiteral("hamburger_menu"));
    QVERIFY(hamburger);
    // The toolbars can be rebuilt meanwhile: the button showing the action now.
    const auto hamburgerButton = [qml, hamburger]() -> QQuickItem* {
        QList<QQuickItem*> items = {qml->contentItem()};
        while (!items.isEmpty()) {
            QQuickItem* item = items.takeFirst();
            if (item->property("qAction").value<QObject*>() == hamburger && item->isVisible()) {
                return item;
            }
            items += item->childItems();
        }
        return nullptr;
    };
    QVERIFY(hamburgerButton());
    hamburger->trigger();
    QMenu* menu = nullptr;
    QTRY_VERIFY((menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())));
    QTRY_COMPARE(menu->geometry().top(), hamburgerButton()->mapToGlobal(QPointF(0, hamburgerButton()->height())).toPoint().y());
    menu->close();
}

void QmlWindowTest::testClicksKeepTheTerminalFocus()
{
    QQuickWindow* qml = _controller->qmlWindow();
    TerminalView* view = TerminalViewRegistry::view(displayOf(_compatWindow));
    QTRY_VERIFY(qml->isActive());
    QTRY_VERIFY(view->hasActiveFocus());

    // As the widget window's toolbar buttons and menu bar, its chrome doesn't take the focus by a click.
    QList<QQuickItem*> buttons;
    std::function<void(QQuickItem*)> collect = [&](QQuickItem* item) {
        if (item->property("qAction").isValid() || QByteArray(item->metaObject()->className()).startsWith("QQuickMenuBarItem")) {
            buttons.append(item);
        }
        for (QQuickItem* child : item->childItems()) {
            collect(child);
        }
    };
    collect(qml->contentItem());
    QVERIFY(!buttons.isEmpty());
    for (const QQuickItem* button : std::as_const(buttons)) {
        QVERIFY2(!(button->focusPolicy() & Qt::ClickFocus), button->metaObject()->className());
    }

    // A click on the terminal gives it the focus back, as the widget's focus policy does.
    QQuickItem other(qml->contentItem());
    other.forceActiveFocus();
    QTRY_VERIFY(!view->hasActiveFocus());
    QTest::mouseClick(qml, Qt::LeftButton, {}, view->mapToScene(QPointF(view->width() / 2, view->height() / 2)).toPoint());
    QTRY_VERIFY(view->hasActiveFocus());
}

void QmlWindowTest::testUnlockedToolBarDragsToAnEdge()
{
    QQuickWindow* qml = _controller->qmlWindow();
    QVERIFY(QTest::qWaitForWindowExposed(qml));
    auto* toolBar = _compatWindow->findChild<KToolBar*>(QStringLiteral("mainToolBar"), Qt::FindDirectChildrenOnly);
    QVERIFY(toolBar);
    const auto grip = [qml]() -> QQuickItem* {
        QQuickItem* found = nullptr;
        std::function<void(QQuickItem*)> find = [&](QQuickItem* item) {
            if (item->objectName() == QLatin1String("toolBarGrip") && item->isVisible()) {
                found = item;
            }
            for (QQuickItem* child : item->childItems()) {
                find(child);
            }
        };
        find(qml->contentItem());
        return found;
    };

    // Locked (the default): no grip, as QToolBar shows no handle.
    KToolBar::setToolBarsLocked(true);
    QTRY_VERIFY(!toolBar->isMovable());
    QTRY_VERIFY(!grip());

    // Unlocked: dragging the grip to the window's bottom edge moves the toolbar there.
    KToolBar::setToolBarsLocked(false);
    QTRY_VERIFY(grip());
    // While dragging, an indicator covers the edge the toolbar will go to.
    const auto indicator = [qml]() -> QRectF {
        QRectF rect;
        std::function<void(QQuickItem*)> find = [&](QQuickItem* item) {
            if (item->objectName() == QLatin1String("toolBarDropIndicator") && item->isVisible()) {
                rect = item->mapRectToScene(item->boundingRect());
            }
            for (QQuickItem* child : item->childItems()) {
                find(child);
            }
        };
        find(qml->contentItem()->parentItem() ? qml->contentItem()->parentItem() : qml->contentItem());
        return rect;
    };
    const auto drag = [qml, &indicator](QQuickItem* handle, QPoint to, bool atBottom) {
        const QPoint from = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2)).toPoint();
        QTest::mousePress(qml, Qt::LeftButton, {}, from);
        for (int step = 1; step <= 10; ++step) {
            QTest::mouseMove(qml, from + (to - from) * step / 10);
        }
        QTRY_VERIFY(!indicator().isEmpty());
        QCOMPARE(indicator().width(), qreal(qml->width()));
        QCOMPARE(atBottom ? indicator().bottom() : indicator().top(), atBottom ? qreal(qml->height()) : 0.0);
        QTest::mouseRelease(qml, Qt::LeftButton, {}, to);
        QTRY_VERIFY(indicator().isEmpty());
    };
    drag(grip(), QPoint(qml->width() / 2, qml->height() - 5), true);
    QTRY_COMPARE(_compatWindow->toolBarArea(toolBar), Qt::BottomToolBarArea);
    auto* chrome = qml->findChild<ChromeAdapter*>();
    QTRY_VERIFY(!chrome->bottomLines().isEmpty());

    // And back to the top, from its grip in the bottom bar.
    const auto gripRect = [&grip] {
        QQuickItem* handle = grip();
        return handle ? handle->mapRectToScene(handle->boundingRect()) : QRectF();
    };
    QTRY_VERIFY(gripRect().top() > qml->height() / 2 && gripRect().bottom() <= qml->height());
    // The terminal takes the room the top bar left.
    auto* centralArea = qml->findChild<QQuickItem*>(QStringLiteral("centralArea"));
    QTRY_COMPARE(centralArea->mapRectToScene(centralArea->boundingRect()).top(), 0.0);
    drag(grip(), QPoint(qml->width() / 2, 5), false);
    QTRY_COMPARE(_compatWindow->toolBarArea(toolBar), Qt::TopToolBarArea);
    QTRY_VERIFY(chrome->bottomLines().isEmpty());
    KToolBar::setToolBarsLocked(true);
}

void QmlWindowTest::testFirstTerminalHasFocus()
{
    // Its facade took the focus before the QML window was shown (init()); as in the widget
    // window, the terminal has it (the solid cursor).
    TerminalDisplay* display = displayOf(_compatWindow);
    TerminalView* view = TerminalViewRegistry::view(display);
    QTRY_VERIFY(_controller->qmlWindow()->isActive());
    QTRY_VERIFY(view->hasActiveFocus());
    // A filled block cursor, not the unfocused outline: its middle isn't the background.
    const QColor background = display->terminalColor()->backgroundColor();
    const auto cursorMiddle = [this, display] {
        const QRect cell = cursorCell(display).translated(display->mapTo(_compatWindow, QPoint(0, 0)));
        return _controller->qmlWindow()->grabWindow().pixelColor(cell.center());
    };
    QTRY_VERIFY(cursorMiddle().rgb() != background.rgb());

    // A menu of the window has the input: as QWidget::hasFocus() in the widget window, the
    // terminal keeps the focus.
    auto* chrome = _controller->qmlWindow()->findChild<ChromeAdapter*>();
    QVERIFY(chrome);
    chrome->popupMenu(0);
    QMenu* menu = nullptr;
    QTRY_VERIFY((menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())));
    QTRY_VERIFY(!view->hasActiveFocus());
    QVERIFY(view->terminalHasFocus());
    QTRY_VERIFY(cursorMiddle().rgb() != background.rgb());
    menu->close();
    QTRY_VERIFY(view->hasActiveFocus());

    // A dialog has it: the terminal hasn't (QApplication's focus widget is the dialog's).
    QDialog dialog(_compatWindow);
    dialog.show();
    dialog.activateWindow();
    QTRY_VERIFY(QGuiApplication::focusWindow() == dialog.windowHandle());
    QVERIFY(!view->terminalHasFocus());
    dialog.hide();
}

void QmlWindowTest::testGeometryFollows()
{
    QQuickWindow* qml = _controller->qmlWindow();
    QCOMPARE(qml->size(), _compatWindow->size());

    // The program resizes the compat window: the QML window follows.
    _compatWindow->resize(700, 500);
    QTRY_COMPARE(qml->size(), QSize(700, 500));

    // The user resizes the QML window: the compat window lays out at that size.
    qml->resize(800, 550);
    QTRY_COMPARE(_compatWindow->size(), QSize(800, 550));
    QTRY_COMPARE(displayOf(_compatWindow)->size().width() > 0, true);
}

void QmlWindowTest::testFocusBridge()
{
    TerminalDisplay* display = displayOf(_compatWindow);
    TerminalView* view = TerminalViewRegistry::view(display);
    QQuickWindow* qml = _controller->qmlWindow();
    // Move focus away (to an item outside the terminal), then focus the facade as old code does.
    auto* elsewhere = new QQuickItem(qml->contentItem());
    elsewhere->forceActiveFocus();
    QTRY_VERIFY(!view->hasActiveFocus());
    display->setFocus();
    QTRY_VERIFY(view->hasActiveFocus());
    QVERIFY(_compatWindow->isVisible());
    QVERIFY(!_compatWindow->isActiveWindow());
    // A second request after focus moved focuses it again.
    elsewhere->forceActiveFocus();
    QTRY_VERIFY(!view->hasActiveFocus());
    display->clearFocus();
    display->setFocus();
    QTRY_VERIFY(view->hasActiveFocus());
}

void QmlWindowTest::testCloseClosesTheCompatWindow()
{
    QPointer<QQuickWindow> qml = _controller->qmlWindow();
    qml->close();
    QTRY_VERIFY(!_compatWindow || !_compatWindow->isVisible());
    QTRY_VERIFY(!qml || !qml->isVisible());
}

QTEST_MAIN(QmlWindowTest)

#include "QmlWindowTest.moc"
