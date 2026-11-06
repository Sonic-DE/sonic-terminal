/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// A terminal shown by a TerminalPane in a QML scene: input, find bar, labels, messages,
// accessibility and scrolling.

#include "../MainWindow.h"
#include "../ViewManager.h"
#include "TestTheme.h"

#include "../quick/TerminalView.h"
#include "../quick/TerminalViewRegistry.h"
#include "../session/Session.h"
#include "../session/SessionController.h"
#include "../session/SessionManager.h"
#include "../terminalDisplay/TerminalDisplay.h"
#include "../terminalDisplay/TerminalFonts.h"
#include "../terminalDisplay/TerminalScrollBar.h"
#include "../widgets/IncrementalSearchBar.h"
#include "../widgets/TerminalHeaderBar.h"

#include <KActionCollection>
#include <KMessageWidget>
#include <KStandardActions>

#include <KConfig>
#include <KConfigGroup>

#include <QAccessible>
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScroller>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStyleHints>
#include <QStyleOptionSlider>
#include <QTest>
#include <QTimer>
#include <QToolButton>

#include <memory>

using namespace Konsole;

class TerminalViewTest : public QObject {
    Q_OBJECT

public:
    static void initMain();

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testQmlLoadsWithoutWarnings();
    void testHeaderBarInput();
    void testFindBar();
    void testLabels();
    void testMessages();
    void testAccessibility();
    void testResizeChangesTheTerminalSize();
    void testResizeRepaintsTheWholeTerminal();
    void testKeysReachTheShell();
    void testSelection();
    void testContextMenu();
    void testScrollBarInput();
    void testTouchScrolling();
    void testLongOutput();

private:
    QString screen() const;
    QPoint cellCenter(int column, int line) const;
    void type(const QString& text);
    bool waitForScreen(const QString& text);

    std::unique_ptr<MainWindow> _window;
    Session* _session = nullptr;
    TerminalDisplay* _sceneDisplay = nullptr;
    TerminalView* _view = nullptr;
    std::unique_ptr<QWidget> _facadeHost;
    std::unique_ptr<QQmlEngine> _engine;
    std::unique_ptr<QQuickWindow> _quickWindow;
    QQuickItem* _pane = nullptr;
    QStringList _qmlWarnings;
};

void TerminalViewTest::initMain()
{
    // Before the platform theme reads them: widgets and Kirigami then take one colour scheme,
    // also for symbolic icons.
    TestTheme::setUp();
}

void TerminalViewTest::initTestCase()
{
    QApplication::setCursorFlashTime(0);
}

void TerminalViewTest::init()
{
    _window = std::make_unique<MainWindow>();
    ViewManager* manager = _window->viewManager();
    const int id = manager->newSession(manager->defaultProfile(), QDir::tempPath());
    _session = SessionManager::instance()->idToSession(id);
    QVERIFY(_session);
    _window->resize(800, 500);
    _window->show();
    QVERIFY(QTest::qWaitForWindowExposed(_window.get()));
    const QSize size = _session->views().at(0)->size();

    // The QML window's facades live in a window that is visible to Qt but never painted.
    _facadeHost = std::make_unique<QWidget>();
    _facadeHost->setAttribute(Qt::WA_DontShowOnScreen);
    _facadeHost->setUpdatesEnabled(false);
    _facadeHost->resize(size);
    _facadeHost->show();
    {
        TerminalView::SceneModeGuard sceneMode;
        _sceneDisplay = manager->createView(_session);
    }
    _sceneDisplay->setParent(_facadeHost.get());
    _sceneDisplay->show();
    // Outside a splitter nothing applies the header bar rule; one terminal hides it.
    _sceneDisplay->headerBar()->setVisible(false);
    _view = TerminalViewRegistry::view(_sceneDisplay);
    QVERIFY(_view && _view->sceneMode());

    _engine = std::make_unique<QQmlEngine>();
    connect(_engine.get(), &QQmlEngine::warnings, this, [this](const QList<QQmlError>& warnings) {
        for (const QQmlError& warning : warnings) {
            _qmlWarnings.append(warning.toString());
        }
    });
    QQmlComponent component(_engine.get(), QStringLiteral("org.sonicde.terminal"), QStringLiteral("TerminalPane"));
    _pane = qobject_cast<QQuickItem*>(component.createWithInitialProperties({{QStringLiteral("view"), QVariant::fromValue(_view)}}));
    QVERIFY2(_pane, qPrintable(component.errorString()));
    _quickWindow = std::make_unique<QQuickWindow>();
    _pane->setParentItem(_quickWindow->contentItem());
    _pane->setSize(size);
    _quickWindow->resize(size);
    _quickWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(_quickWindow.get()));
    QTRY_COMPARE(_sceneDisplay->size(), size);
    QVERIFY(waitForScreen(QStringLiteral("$")) || !screen().trimmed().isEmpty());
}

void TerminalViewTest::cleanup()
{
    delete _pane;
    _pane = nullptr;
    _quickWindow.reset();
    _engine.reset();
    _facadeHost.reset();
    _window.reset();
    _qmlWarnings.clear();
}

QString TerminalViewTest::screen() const
{
    return _session ? _session->getAllDisplayedText() : QString();
}

bool TerminalViewTest::waitForScreen(const QString& text)
{
    return QTest::qWaitFor(
        [this, &text] {
            return screen().contains(text);
        },
        5000);
}

void TerminalViewTest::type(const QString& text)
{
    _view->forceActiveFocus();
    for (const QChar c : text) {
        QTest::keyClick(_quickWindow.get(), c.toLatin1());
    }
    QTest::keyClick(_quickWindow.get(), Qt::Key_Return);
}

QPoint TerminalViewTest::cellCenter(int column, int line) const
{
    const int width = _sceneDisplay->terminalFont()->fontWidth();
    const int height = _sceneDisplay->terminalFont()->fontHeight();
    const QPoint local = _sceneDisplay->contentRect().topLeft() + QPoint(column * width + width / 2, line * height + height / 2);
    return _view->mapToScene(local).toPoint();
}

void TerminalViewTest::testQmlLoadsWithoutWarnings()
{
    QVERIFY2(_qmlWarnings.isEmpty(), qPrintable(_qmlWarnings.join(u'\n')));
}

void TerminalViewTest::testHeaderBarInput()
{
    TerminalHeaderBar* header = _sceneDisplay->headerBar();
    header->setVisible(true);
    QTRY_VERIFY(header->isVisible() && header->width() > 0);
    // By name: the class isn't exported.
    QSignalSpy expand(header, SIGNAL(requestToggleExpansion()));
    QSignalSpy moveToTab(header, SIGNAL(requestMoveToNewTab()));
    // The expand button is the checkable one, the close button is named.
    QPoint expandButton;
    QPoint moveButton;
    for (const QToolButton* button : header->findChildren<QToolButton*>()) {
        const QPoint centre = header->mapTo(_sceneDisplay, button->geometry().center());
        if (button->isCheckable()) {
            expandButton = centre;
        } else if (button->objectName().isEmpty()) {
            moveButton = centre;
        }
    }
    QTest::mouseClick(_quickWindow.get(), Qt::LeftButton, {}, expandButton);
    QCOMPARE(expand.count(), 1);
    // A double click on the bar is the expand button's click.
    QTest::qWait(QGuiApplication::styleHints()->mouseDoubleClickInterval() + 100);
    QTest::mouseDClick(_quickWindow.get(), Qt::LeftButton, {}, header->mapTo(_sceneDisplay, QPoint(header->width() / 3, header->height() / 2)));
    QCOMPARE(expand.count(), 2);
    // Last: the view manager moves the display out of this pane.
    QTest::mouseClick(_quickWindow.get(), Qt::LeftButton, {}, moveButton);
    QCOMPARE(moveToTab.count(), 1);
}

void TerminalViewTest::testFindBar()
{
    type(QStringLiteral("printf '\\033[2J\\033[Halpha beta gamma\\n'"));
    QVERIFY(waitForScreen(QStringLiteral("alpha beta gamma")));
    _view->sessionController()->actionCollection()->action(KStandardActions::name(KStandardActions::Find))->trigger();
    IncrementalSearchBar* sceneBar = _sceneDisplay->searchBar();
    QTRY_VERIFY(sceneBar->isVisible());

    // The bar focuses its line edit, which is the QML text field here.
    QTRY_VERIFY(_quickWindow->activeFocusItem() && _quickWindow->activeFocusItem() != _view);
    QVERIFY(_view->partHasFocus());

    // Typing edits the real line edit; the search marks the match.
    for (const QChar c : QStringLiteral("beta")) {
        QTest::keyClick(_quickWindow.get(), c.toLatin1());
    }
    QCOMPARE(sceneBar->searchText(), QStringLiteral("beta"));
    QTRY_VERIFY(sceneBar->findChild<QLineEdit*>()->styleSheet().contains(QLatin1String("background-color")));

    QSignalSpy findNext(sceneBar, &IncrementalSearchBar::findNextClicked);
    QTest::keyClick(_quickWindow.get(), Qt::Key_Return);
    QCOMPARE(findNext.count(), 1);
    QTest::keyClick(_quickWindow.get(), Qt::Key_Escape);
    QTRY_VERIFY(!sceneBar->isVisible());
}

void TerminalViewTest::testLabels()
{
    const auto shownLabels = [this] {
        int shown = 0;
        for (const QLabel* label : _sceneDisplay->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
            shown += label->isVisible() ? 1 : 0;
        }
        return shown;
    };
    // The "Size:" notification of the first layout stays up for a second.
    QTRY_COMPARE_WITH_TIMEOUT(shownLabels(), 0, 5000);
    _sceneDisplay->setHoverLinkIndicator(QStringLiteral("https://example.org/a/link"));
    _sceneDisplay->showNotification(QStringLiteral("Size: 80 x 24"));
    QCOMPARE(shownLabels(), 2);

    // The notification hides itself; the link label goes with an empty text.
    _sceneDisplay->setHoverLinkIndicator(QString());
    QTRY_VERIFY_WITH_TIMEOUT(_pane->findChildren<QQuickItem*>().size() > 0, 100);
    QTRY_COMPARE_WITH_TIMEOUT(shownLabels(), 0, 3000);
}

void TerminalViewTest::testMessages()
{
    _sceneDisplay->updateReadOnlyState(true);
    _sceneDisplay->outputSuspended(true);
    const auto messagesOf = [](TerminalDisplay* display) {
        QList<KMessageWidget*> shown;
        for (KMessageWidget* message : display->findChildren<KMessageWidget*>(Qt::FindDirectChildrenOnly)) {
            if (message->isVisible() && !message->isShowAnimationRunning()) {
                shown.append(message);
            }
        }
        return shown;
    };
    QTRY_COMPARE(messagesOf(_sceneDisplay).size(), 2);

    // The close button in QML closes the real message.
    KMessageWidget* readOnly = nullptr;
    for (KMessageWidget* message : messagesOf(_sceneDisplay)) {
        if (!message->icon().isNull()) {
            readOnly = message;
        }
    }
    QVERIFY(readOnly);
    const QToolButton* close = readOnly->findChildren<QToolButton*>().constLast();
    QTest::mouseClick(_quickWindow.get(), Qt::LeftButton, {}, close->mapTo(_sceneDisplay, close->rect().center()));
    QTRY_VERIFY(!readOnly->isVisible());
}

void TerminalViewTest::testAccessibility()
{
    type(QStringLiteral("printf '\\033[2J\\033[Halpha beta gamma\\n'"));
    QVERIFY(waitForScreen(QStringLiteral("alpha beta gamma")));
    QTest::qWait(200);
    QAccessibleInterface* scene = QAccessible::queryAccessibleInterface(_view);
    QVERIFY(scene);
    QCOMPARE(scene->role(), QAccessible::Terminal);
    QVERIFY(scene->state().multiLine);
    QVERIFY(scene->textInterface());
    QVERIFY(scene->textInterface()->text(0, 16).startsWith(QLatin1String("alpha beta gamma")));
    // In the window's tree, where Qt Quick puts accessible items.
    QAccessibleInterface* window = QAccessible::queryAccessibleInterface(_quickWindow.get());
    QVERIFY(window);
    QCOMPARE(scene->parent(), window);
    bool listed = false;
    for (int i = 0; i < window->childCount(); ++i) {
        listed = listed || window->child(i) == scene;
    }
    QVERIFY(listed);
    QCOMPARE(scene->rect().size(), _sceneDisplay->size());
}

void TerminalViewTest::testResizeChangesTheTerminalSize()
{
    const int columns = _sceneDisplay->columns();
    _pane->setWidth(_pane->width() - 10 * _sceneDisplay->terminalFont()->fontWidth());
    QTRY_COMPARE(_sceneDisplay->width(), int(_pane->width()));
    QTRY_VERIFY(_sceneDisplay->columns() < columns);
}

void TerminalViewTest::testResizeRepaintsTheWholeTerminal()
{
    // As a split: the splitter resizes the facade first, the pane follows afterwards.
    type(QStringLiteral("clear"));
    QTest::qWait(500);
    const QSize half(int(_pane->width()), int(_pane->height()) / 2);
    const QPoint background(_sceneDisplay->contentRect().left() + 2, half.height() - 10);
    const QColor expected = _quickWindow->grabWindow().pixelColor(background);
    _sceneDisplay->resize(half);
    QTest::qWait(300);
    // In the same frame the cursor line repaints, as the display's update after a resize.
    _pane->setSize(half);
    _view->update(QRegion(_sceneDisplay->contentRect().left(), _sceneDisplay->contentRect().top(), _sceneDisplay->contentRect().width(),
        _sceneDisplay->terminalFont()->fontHeight()));
    QTRY_COMPARE(_sceneDisplay->size(), half);
    QTest::qWait(300);
    QCOMPARE(_quickWindow->grabWindow().pixelColor(background), expected);
}

void TerminalViewTest::testKeysReachTheShell()
{
    type(QStringLiteral("stty -ixon -echo; exec cat -v"));
    QTest::qWait(300);
    const auto lastLine = [this] {
        const QStringList lines = _session->getAllDisplayedTextList(true);
        return lines.isEmpty() ? QString() : lines.last();
    };

    _view->forceActiveFocus();
    QTest::keyClick(_quickWindow.get(), Qt::Key_Q, Qt::ControlModifier);
    QTest::keyClick(_quickWindow.get(), Qt::Key_Tab);
    QTest::keyClick(_quickWindow.get(), Qt::Key_Left);
    QTest::keyClick(_quickWindow.get(), Qt::Key_Return);
    // QTest's Ctrl+Q carries the text "q"; cat -v leaves the tab as it is.
    QTRY_VERIFY(lastLine().contains(QLatin1String("^[[D")));
    QVERIFY2(lastLine().startsWith(QLatin1Char('q')), qPrintable(lastLine()));
    // Tab is the terminal's, not the focus chain's.
    QCOMPARE(_quickWindow->activeFocusItem(), _view);
}

void TerminalViewTest::testSelection()
{
    type(QStringLiteral("printf '\\033[2J\\033[Halpha beta gamma\\n'"));
    QVERIFY(waitForScreen(QStringLiteral("alpha beta gamma")));
    QTest::qWait(200);
    QWindow* window = _quickWindow.get();
    const auto selected = [this] {
        return _sceneDisplay->screenWindow()->selectedText(Screen::PlainText);
    };
    QTest::mousePress(window, Qt::LeftButton, {}, cellCenter(0, 0));
    QTest::mouseMove(window, cellCenter(2, 0));
    QTest::mouseMove(window, cellCenter(4, 0));
    QTest::mouseRelease(window, Qt::LeftButton, {}, cellCenter(4, 0));
    QVERIFY2(selected().contains(QLatin1String("lpha")), qPrintable(selected()));
    QTest::qWait(QGuiApplication::styleHints()->mouseDoubleClickInterval() + 100);
    QTest::mouseDClick(window, Qt::LeftButton, {}, cellCenter(7, 0));
    QCOMPARE(selected(), QStringLiteral("beta"));
    QTest::mouseClick(window, Qt::LeftButton, {}, cellCenter(7, 0));
    QVERIFY(selected().contains(QLatin1String("alpha beta gamma")));
}

void TerminalViewTest::testContextMenu()
{
    QSignalSpy configure(_sceneDisplay, &TerminalDisplay::configureRequest);
    // The session's context menu runs its own event loop.
    QTimer::singleShot(200, this, [] {
        if (QWidget* menu = QApplication::activePopupWidget()) {
            menu->close();
        }
    });
    QContextMenuEvent event(QContextMenuEvent::Mouse, cellCenter(1, 0), _quickWindow->mapToGlobal(cellCenter(1, 0)));
    QCoreApplication::sendEvent(_view, &event);
    QCOMPARE(configure.count(), 1);
}

void TerminalViewTest::testScrollBarInput()
{
    type(QStringLiteral("seq 1 300"));
    QVERIFY(waitForScreen(QStringLiteral("300")));
    QTest::qWait(200);
    QWindow* window = _quickWindow.get();
    TerminalScrollBar* scrollBar = _sceneDisplay->scrollBar();
    QTRY_VERIFY(scrollBar->maximum() > 0);

    const QRect bar = scrollBar->geometry();
    const auto handle = [scrollBar] {
        QStyleOptionSlider option;
        option.initFrom(scrollBar);
        option.subControls = QStyle::SC_All;
        option.orientation = scrollBar->orientation();
        option.minimum = scrollBar->minimum();
        option.maximum = scrollBar->maximum();
        option.sliderPosition = scrollBar->sliderPosition();
        option.sliderValue = scrollBar->value();
        option.singleStep = scrollBar->singleStep();
        option.pageStep = scrollBar->pageStep();
        return scrollBar->style()->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarSlider, scrollBar);
    };
    scrollBar->setValue(0);

    QTest::mouseClick(window, Qt::LeftButton, {}, bar.topLeft() + QPoint(bar.width() / 2, bar.height() - bar.width() / 2));
    QVERIFY(scrollBar->value() > 0);
    int value = scrollBar->value();
    QTest::mouseClick(window, Qt::LeftButton, {}, bar.topLeft() + QPoint(bar.width() / 2, bar.height() * 3 / 4));
    QVERIFY(scrollBar->value() > value);

    QSignalSpy moved(scrollBar, &QAbstractSlider::sliderMoved);
    const QPoint grab = bar.topLeft() + handle().center();
    QTest::mousePress(window, Qt::LeftButton, {}, grab);
    QTest::mouseMove(window, grab - QPoint(0, 20));
    QTest::mouseMove(window, grab - QPoint(0, 60));
    QTest::mouseRelease(window, Qt::LeftButton, {}, grab - QPoint(0, 60));
    QVERIFY(moved.count() > 0);

    scrollBar->setValue(scrollBar->maximum() / 2);
    value = scrollBar->value();
    const QPointF position = bar.topLeft() + QPoint(bar.width() / 2, bar.height() / 2);
    QWheelEvent wheel(position, window->mapToGlobal(position), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(window, &wheel);
    QVERIFY(scrollBar->value() < value);

    // A double click adds a marker.
    QSignalSpy markers(scrollBar, &TerminalScrollBar::overlaysChanged);
    QTest::mouseDClick(window, Qt::LeftButton, {}, bar.topLeft() + QPoint(bar.width() / 2, bar.height() / 3));
    QVERIFY(markers.count() > 0);
    QTest::qWait(QGuiApplication::styleHints()->mouseDoubleClickInterval() + 100);
}

void TerminalViewTest::testTouchScrolling()
{
    type(QStringLiteral("seq 1 300"));
    QVERIFY(waitForScreen(QStringLiteral("300")));
    TerminalScrollBar* bar = _sceneDisplay->scrollBar();
    QTRY_VERIFY(bar->value() > 0 && bar->value() == bar->maximum());
    const int bottom = bar->value();

    // Dragging the content down shows older lines, as QScroller did on the facade.
    static QPointingDevice* touchScreen = QTest::createTouchDevice(QInputDevice::DeviceType::TouchScreen);
    const int x = _pane->width() / 2;
    QTest::touchEvent(_quickWindow.get(), touchScreen).press(0, QPoint(x, 60));
    for (int y = 80; y <= 300; y += 20) {
        QTest::qWait(10);
        QTest::touchEvent(_quickWindow.get(), touchScreen).move(0, QPoint(x, y));
    }
    QTest::touchEvent(_quickWindow.get(), touchScreen).release(0, QPoint(x, 300));
    QTRY_VERIFY(bar->value() < bottom);
    QTRY_COMPARE(QScroller::scroller(_view)->state(), QScroller::Inactive);
}

void TerminalViewTest::testLongOutput()
{
    type(QStringLiteral("for i in $(seq 1 3000); do printf '\\033]133;A\\007line %s\\n' $i; done"));
    QVERIFY(waitForScreen(QStringLiteral("line 3000")));
    QTest::qWait(200);
    QVERIFY(_view->isVisible());
}

QTEST_MAIN(TerminalViewTest)

#include "TerminalViewTest.moc"
