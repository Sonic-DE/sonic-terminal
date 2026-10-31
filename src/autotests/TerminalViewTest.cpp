/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// A terminal shown by a TerminalPane in a QML scene must look and behave as the same
// session shown by the widget facade: one session, both views, same size.

#include "../MainWindow.h"
#include "../ViewManager.h"
#include "ParityImages.h"
#include "TestTheme.h"

#include "../quick/HeaderBarAdapter.h"
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
#include <QDBusConnection>
#include <QDBusMessage>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScroller>
#include <QSignalSpy>
#include <QSplitter>
#include <QStandardPaths>
#include <QStyleHints>
#include <QStyleOptionSlider>
#include <QTabBar>
#include <QTest>
#include <QTimer>
#include <QToolButton>

#include <functional>
#include <memory>

using namespace Konsole;

class TerminalViewTest : public QObject {
    Q_OBJECT

public:
    static void initMain();
    static void writeGlobals(const QString& colorScheme, const QString& widgetStyle);
    static void announceThemeChange(int change);

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testQmlLoadsWithoutWarnings();
    void testLooksLikeTheWidget();
    void testHeaderBarLooksLikeTheWidget();
    void testHeaderBarInput();
    void testFindBar();
    void testLabels();
    void testMessages();
    void testAccessibility();
    void testThemeChangeWhileRunning();
    void testResizeChangesTheTerminalSize();
    void testResizeRepaintsTheWholeTerminal();
    void testKeysReachTheShell();
    void testSelection();
    void testContextMenu();
    void testScrollBarInput();
    void testTouchScrolling();
    void testLongOutput();

private:
    // Empty when the QML pane looks like the widget display (the parity rule).
    QString compareWithWidget(const QList<QRect>& overlays, const QList<QRect>& text);
    QString screen() const;
    QPoint cellCenter(int column, int line) const;
    void type(const QString& text);
    bool waitForScreen(const QString& text);
    bool waitForLabelsHidden();

    std::unique_ptr<MainWindow> _window;
    Session* _session = nullptr;
    TerminalDisplay* _widgetDisplay = nullptr;
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
    // The parity runs' settings (tools/parity/run.py), before the platform theme reads them:
    // widgets and Kirigami then take one colour scheme, also for symbolic icons.
    TestTheme::setUp();
}

void TerminalViewTest::writeGlobals(const QString& colorScheme, const QString& widgetStyle)
{
    TestTheme::writeGlobals(colorScheme, widgetStyle);
}

void TerminalViewTest::announceThemeChange(int change)
{
    // The settings pages' signal, which the KDE platform theme listens to
    // (KHintsSettings::slotNotifyChange(): 0 palette, 2 widget style).
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/KGlobalSettings"), QStringLiteral("org.kde.KGlobalSettings"), QStringLiteral("notifyChange"));
    message << change << 0;
    QDBusConnection::sessionBus().send(message);
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
    _widgetDisplay = _session->views().at(0);

    // The QML window's facades live in a window that is visible to Qt but never painted.
    _facadeHost = std::make_unique<QWidget>();
    _facadeHost->setAttribute(Qt::WA_DontShowOnScreen);
    _facadeHost->setUpdatesEnabled(false);
    _facadeHost->resize(_widgetDisplay->size());
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
    _pane->setSize(_widgetDisplay->size());
    _quickWindow->resize(_widgetDisplay->size());
    _quickWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(_quickWindow.get()));
    QTRY_COMPARE(_sceneDisplay->size(), _widgetDisplay->size());
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

bool TerminalViewTest::waitForLabelsHidden()
{
    // The "Size:" notification of each display's first layout stays up for a second.
    return QTest::qWaitFor(
        [this] {
            for (TerminalDisplay* display : {_widgetDisplay, _sceneDisplay}) {
                const QLabel* label = TerminalViewRegistry::view(display)->resizeLabel();
                if (label && label->isVisible()) {
                    return false;
                }
            }
            return true;
        },
        5000);
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

void TerminalViewTest::testLooksLikeTheWidget()
{
    type(QStringLiteral("printf '\\033[2J\\033[Hred \\033[31mtext\\033[0m and \\033[1mbold\\033[0m'"));
    QVERIFY(waitForScreen(QStringLiteral("red text and bold")));
    QTest::qWait(300);
    QVERIFY(waitForLabelsHidden());

    const QRect scrollBar = _widgetDisplay->scrollBar()->isHidden() ? QRect() : _widgetDisplay->scrollBar()->geometry();
    const QString difference = compareWithWidget({scrollBar}, {});
    QVERIFY2(difference.isEmpty(), qPrintable(difference));
}

void TerminalViewTest::testHeaderBarLooksLikeTheWidget()
{
    // Both displays show their header bar, as a split would.
    _widgetDisplay->headerBar()->setVisible(true);
    _sceneDisplay->headerBar()->setVisible(true);
    TerminalHeaderBar* header = _widgetDisplay->headerBar();
    QTRY_COMPARE(_sceneDisplay->headerBar()->geometry(), header->geometry());
    // What is above the widget header bar decides its base line (TerminalHeaderBar::paintEvent()).
    const QPoint above = header->mapToGlobal(QPoint(10, -10));
    QWidget* widgetAbove = QApplication::widgetAt(above);
    const bool baseLine = widgetAbove && !qobject_cast<QTabBar*>(widgetAbove) && !qobject_cast<TerminalDisplay*>(widgetAbove)
        && !qobject_cast<QSplitter*>(widgetAbove) && !qobject_cast<QSplitterHandle*>(widgetAbove);
    _pane->setProperty("headerTabBarBase", baseLine);
    QTest::qWait(300);
    QVERIFY(waitForLabelsHidden());

    // The title is text: the parity text rule applies to it.
    QRect titleArea;
    for (const QLabel* label : header->findChildren<QLabel*>()) {
        if (!label->text().isEmpty()) {
            titleArea = QRect(label->mapTo(_widgetDisplay, QPoint(0, 0)), label->size());
        }
    }
    const QRect scrollBar = _widgetDisplay->scrollBar()->isHidden() ? QRect() : _widgetDisplay->scrollBar()->geometry();
    const QString difference = compareWithWidget({scrollBar}, {titleArea});
    QVERIFY2(difference.isEmpty(), qPrintable(difference));
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
    // Each display has its own controller and find bar; open both with the real action.
    SessionController* widgetController = TerminalViewRegistry::view(_widgetDisplay)->sessionController();
    SessionController* sceneController = _view->sessionController();
    const QString find = KStandardActions::name(KStandardActions::Find);
    widgetController->actionCollection()->action(find)->trigger();
    sceneController->actionCollection()->action(find)->trigger();
    IncrementalSearchBar* widgetBar = _widgetDisplay->searchBar();
    IncrementalSearchBar* sceneBar = _sceneDisplay->searchBar();
    QTRY_VERIFY(widgetBar->isVisible() && sceneBar->isVisible());
    QTRY_COMPARE(sceneBar->geometry(), widgetBar->geometry());

    // The bar focuses its line edit, which is the QML text field here.
    QTRY_VERIFY(_quickWindow->activeFocusItem() && _quickWindow->activeFocusItem() != _view);
    QVERIFY(_view->partHasFocus());
    QTest::qWait(300);
    QVERIFY(waitForLabelsHidden());
    // Compared unfocused, like the widget's line edit (its window isn't the active one).
    QQuickItem* field = _quickWindow->activeFocusItem();
    _view->forceActiveFocus();
    QTest::qWait(100);
    const QRect scrollBar = _widgetDisplay->scrollBar()->isHidden() ? QRect() : _widgetDisplay->scrollBar()->geometry();
    const QString difference = compareWithWidget({scrollBar, widgetBar->geometry()}, {});
    QVERIFY2(difference.isEmpty(), qPrintable(difference));
    field->forceActiveFocus();

    // Typing edits the real line edit; the search marks the match.
    for (const QChar c : QStringLiteral("beta")) {
        QTest::keyClick(_quickWindow.get(), c.toLatin1());
    }
    QCOMPARE(sceneBar->searchText(), QStringLiteral("beta"));
    QTRY_VERIFY(sceneBar->findChild<QLineEdit*>()->styleSheet().contains(QLatin1String("background-color")));
    // The same text and match colour in the widget's bar.
    widgetBar->setSearchText(QStringLiteral("beta"));
    QTRY_COMPARE(widgetBar->findChild<QLineEdit*>()->styleSheet(), sceneBar->findChild<QLineEdit*>()->styleSheet());
    _view->forceActiveFocus();
    QTest::qWait(300);
    const QString matched = compareWithWidget({scrollBar, widgetBar->geometry()}, {});
    QVERIFY2(matched.isEmpty(), qPrintable(matched));
    field->forceActiveFocus();

    QSignalSpy findNext(sceneBar, &IncrementalSearchBar::findNextClicked);
    QTest::keyClick(_quickWindow.get(), Qt::Key_Return);
    QCOMPARE(findNext.count(), 1);
    QTest::keyClick(_quickWindow.get(), Qt::Key_Escape);
    QTRY_VERIFY(!sceneBar->isVisible());
}

void TerminalViewTest::testLabels()
{
    QTest::qWait(300);
    QVERIFY(waitForLabelsHidden());
    for (TerminalDisplay* display : {_widgetDisplay, _sceneDisplay}) {
        display->setHoverLinkIndicator(QStringLiteral("https://example.org/a/link"));
        // The label only ever grows (setMinimumWidth()); the same widest text gives both
        // displays, whatever sizes they went through, the same label.
        display->showNotification(QStringLiteral("Size: 8000 x 2400"));
        display->showNotification(QStringLiteral("Size: 80 x 24"));
    }
    // A label is an overlay; its text gets the parity text rule (the text-hidden mask).
    QList<QRect> overlays{_widgetDisplay->scrollBar()->isHidden() ? QRect() : _widgetDisplay->scrollBar()->geometry()};
    QList<QRect> text;
    for (const QLabel* label : _widgetDisplay->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (label->isVisible()) {
            overlays.append(label->geometry());
            text.append(label->contentsRect().translated(label->pos()));
        }
    }
    QCOMPARE(overlays.size(), 3);
    QTest::qWait(100);
    const QString difference = compareWithWidget(overlays, text);
    QVERIFY2(difference.isEmpty(), qPrintable(difference));

    // The notification hides itself; the link label goes with an empty text.
    _sceneDisplay->setHoverLinkIndicator(QString());
    QTRY_VERIFY_WITH_TIMEOUT(_pane->findChildren<QQuickItem*>().size() > 0, 100);
    const auto shownLabels = [this] {
        int shown = 0;
        for (const QLabel* label : _sceneDisplay->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
            shown += label->isVisible() ? 1 : 0;
        }
        return shown;
    };
    QTRY_COMPARE_WITH_TIMEOUT(shownLabels(), 0, 3000);
}

void TerminalViewTest::testMessages()
{
    for (TerminalDisplay* display : {_widgetDisplay, _sceneDisplay}) {
        display->updateReadOnlyState(true);
        display->outputSuspended(true);
    }
    const auto messagesOf = [](TerminalDisplay* display) {
        QList<KMessageWidget*> shown;
        for (KMessageWidget* message : display->findChildren<KMessageWidget*>(Qt::FindDirectChildrenOnly)) {
            if (message->isVisible() && !message->isShowAnimationRunning()) {
                shown.append(message);
            }
        }
        return shown;
    };
    QTRY_COMPARE(messagesOf(_widgetDisplay).size(), 2);
    QTRY_COMPARE(messagesOf(_sceneDisplay).size(), 2);
    QTest::qWait(300);
    QVERIFY(waitForLabelsHidden());

    // A message is an overlay; its text gets the parity text rule.
    QList<QRect> overlays{_widgetDisplay->scrollBar()->isHidden() ? QRect() : _widgetDisplay->scrollBar()->geometry()};
    QList<QRect> text;
    for (const KMessageWidget* message : messagesOf(_widgetDisplay)) {
        QCOMPARE(message->geometry(), messagesOf(_sceneDisplay).at(messagesOf(_widgetDisplay).indexOf(message))->geometry());
        overlays.append(message->geometry());
        for (const QLabel* label : message->findChildren<QLabel*>()) {
            if (!label->text().isEmpty()) {
                text.append(QRect(label->mapTo(_widgetDisplay, QPoint(0, 0)), label->size()));
            }
        }
    }
    const QString difference = compareWithWidget(overlays, text);
    QVERIFY2(difference.isEmpty(), qPrintable(difference));

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
    QAccessibleInterface* widget = QAccessible::queryAccessibleInterface(_widgetDisplay);
    QAccessibleInterface* scene = QAccessible::queryAccessibleInterface(_view);
    QVERIFY(widget && scene);
    QCOMPARE(scene->role(), QAccessible::Terminal);
    QCOMPARE(scene->text(QAccessible::Name), widget->text(QAccessible::Name));
    QCOMPARE(scene->text(QAccessible::Value), widget->text(QAccessible::Value));
    QVERIFY(scene->state().multiLine);
    QVERIFY(scene->textInterface());
    QCOMPARE(scene->textInterface()->characterCount(), widget->textInterface()->characterCount());
    QCOMPARE(scene->textInterface()->cursorPosition(), widget->textInterface()->cursorPosition());
    QCOMPARE(scene->textInterface()->text(0, 16), widget->textInterface()->text(0, 16));
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

void TerminalViewTest::testThemeChangeWhileRunning()
{
    type(QStringLiteral("printf '\\033[2J\\033[Halpha beta gamma\\n'"));
    QVERIFY(waitForScreen(QStringLiteral("alpha beta gamma")));

    // Every part of the pane at once, the same on both displays.
    const QString find = KStandardActions::name(KStandardActions::Find);
    for (TerminalDisplay* display : {_widgetDisplay, _sceneDisplay}) {
        display->headerBar()->setVisible(true);
        display->updateReadOnlyState(true);
        display->setHoverLinkIndicator(QStringLiteral("https://example.org/a/link"));
        TerminalViewRegistry::view(display)->sessionController()->actionCollection()->action(find)->trigger();
    }
    QTRY_VERIFY(_widgetDisplay->searchBar()->isVisible() && _sceneDisplay->searchBar()->isVisible());
    for (TerminalDisplay* display : {_widgetDisplay, _sceneDisplay}) {
        display->searchBar()->setSearchText(QStringLiteral("beta"));
    }
    TerminalHeaderBar* header = _widgetDisplay->headerBar();
    QWidget* above = QApplication::widgetAt(header->mapToGlobal(QPoint(10, -10)));
    _pane->setProperty("headerTabBarBase",
        above && !qobject_cast<QTabBar*>(above) && !qobject_cast<TerminalDisplay*>(above) && !qobject_cast<QSplitter*>(above)
            && !qobject_cast<QSplitterHandle*>(above));

    const auto check = [this](const char* step) -> QString {
        // Neither terminal focused (no part of either display has focus).
        if (QWidget* focus = _window->focusWidget()) {
            focus->clearFocus();
        }
        _pane->forceActiveFocus();
        HeaderBarAdapter widgetHeader;
        widgetHeader.setView(TerminalViewRegistry::view(_widgetDisplay));
        HeaderBarAdapter sceneHeader;
        sceneHeader.setView(_view);
        if (!QTest::qWaitFor(
                [&] {
                    return !widgetHeader.focused() && !sceneHeader.focused();
                },
                2000)) {
            return QStringLiteral("%1: a header bar stayed focused (widget %2, QML %3)")
                .arg(QLatin1String(step))
                .arg(widgetHeader.focused())
                .arg(sceneHeader.focused());
        }
        const bool same = QTest::qWaitFor(
            [this] {
                return _widgetDisplay->searchBar()->findChild<QLineEdit*>()->styleSheet() == _sceneDisplay->searchBar()->findChild<QLineEdit*>()->styleSheet()
                    && _sceneDisplay->searchBar()->geometry() == _widgetDisplay->searchBar()->geometry()
                    && _sceneDisplay->headerBar()->geometry() == _widgetDisplay->headerBar()->geometry();
            },
            5000);
        if (!same) {
            return QStringLiteral("%1: the two displays' find or header bars differ").arg(QLatin1String(step));
        }
        QTest::qWait(300);
        if (!waitForLabelsHidden()) {
            return QStringLiteral("%1: a size label stayed up").arg(QLatin1String(step));
        }
        QList<QRect> overlays{_widgetDisplay->scrollBar()->isHidden() ? QRect() : _widgetDisplay->scrollBar()->geometry(),
            _widgetDisplay->searchBar()->geometry()};
        QList<QRect> text;
        for (const QLabel* label : _widgetDisplay->findChildren<QLabel*>()) {
            if (label->isVisible() && !label->text().isEmpty()) {
                text.append(QRect(label->mapTo(_widgetDisplay, QPoint(0, 0)), label->size()));
            }
        }
        for (const KMessageWidget* message : _widgetDisplay->findChildren<KMessageWidget*>()) {
            if (message->isVisible()) {
                overlays.append(message->geometry());
            }
        }
        for (const QLabel* label : _widgetDisplay->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
            if (label->isVisible()) {
                overlays.append(label->geometry());
            }
        }
        const QString difference = compareWithWidget(overlays, text);
        return difference.isEmpty() ? QString() : QStringLiteral("%1: %2").arg(QLatin1String(step), difference);
    };
    const auto waitForPalette = [](bool dark) {
        return QTest::qWaitFor(
            [dark] {
                return (qApp->palette().color(QPalette::Window).lightness() < 128) == dark;
            },
            5000);
    };
    const auto waitForStyle = [](const QString& name) {
        return QTest::qWaitFor(
            [&name] {
                return QApplication::style()->name().compare(name, Qt::CaseInsensitive) == 0;
            },
            5000);
    };

    QString difference = check("SilverLight, Silver");
    QVERIFY2(difference.isEmpty(), qPrintable(difference));

    // The colour scheme changes while the terminal runs.
    writeGlobals(QStringLiteral("SilverDark"), QStringLiteral("Silver"));
    announceThemeChange(0);
    QVERIFY(waitForPalette(true));
    difference = check("SilverDark, Silver");
    QVERIFY2(difference.isEmpty(), qPrintable(difference));

    // Then the widget style.
    writeGlobals(QStringLiteral("SilverDark"), QStringLiteral("Breeze"));
    announceThemeChange(2);
    QVERIFY(waitForStyle(QStringLiteral("breeze")));
    difference = check("SilverDark, Breeze");
    QVERIFY2(difference.isEmpty(), qPrintable(difference));

    // And back, so the other tests see their theme.
    writeGlobals(QStringLiteral("SilverLight"), QStringLiteral("Silver"));
    announceThemeChange(2);
    announceThemeChange(0);
    QVERIFY(waitForStyle(QStringLiteral("silver")));
    QVERIFY(waitForPalette(false));
    difference = check("SilverLight, Silver again");
    QVERIFY2(difference.isEmpty(), qPrintable(difference));
}

QString TerminalViewTest::compareWithWidget(const QList<QRect>& overlays, const QList<QRect>& text)
{
    const QImage widgetImage = _widgetDisplay->grab().toImage().convertToFormat(QImage::Format_ARGB32);
    const QImage sceneImage = _quickWindow->grabWindow().copy(QRect(QPoint(0, 0), _widgetDisplay->size())).convertToFormat(QImage::Format_ARGB32);
    // The cursor's look depends on focus, which only one of the two windows can have.
    const int width = _sceneDisplay->terminalFont()->fontWidth();
    const int height = _sceneDisplay->terminalFont()->fontHeight();
    const QPoint cursor = _widgetDisplay->contentRect().topLeft() + QPoint(_view->cursorPosition().x() * width, _view->cursorPosition().y() * height);
    const QRect cursorCell = QRect(cursor, QSize(width, height)).adjusted(-2, -2, 2, 2);
    return ParityImages::compare(widgetImage, sceneImage, overlays, text, {cursorCell});
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
    const auto sendKeys = [](QWindow* window) {
        QTest::keyClick(window, Qt::Key_Q, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::keyClick(window, Qt::Key_Left);
        QTest::keyClick(window, Qt::Key_Return);
    };
    const auto lastLine = [this] {
        const QStringList lines = _session->getAllDisplayedTextList(true);
        return lines.isEmpty() ? QString() : lines.last();
    };

    _widgetDisplay->setFocus();
    sendKeys(_window->windowHandle());
    QTRY_VERIFY(!lastLine().contains(QLatin1String("cat -v")));
    const QString fromWidget = lastLine();
    QVERIFY(!fromWidget.isEmpty());

    _view->forceActiveFocus();
    sendKeys(_quickWindow.get());
    QTRY_COMPARE(_session->getAllDisplayedTextList(true).count(fromWidget), 2);
    // Tab is the terminal's, not the focus chain's.
    QCOMPARE(_quickWindow->activeFocusItem(), _view);
}

void TerminalViewTest::testSelection()
{
    type(QStringLiteral("printf '\\033[2J\\033[Halpha beta gamma\\n'"));
    QVERIFY(waitForScreen(QStringLiteral("alpha beta gamma")));
    QTest::qWait(200);
    // Both views share the screen's selection; the widget's result is the expected one.
    const auto select = [this](QWindow* window, QPoint origin) {
        const int pause = QGuiApplication::styleHints()->mouseDoubleClickInterval() + 100;
        QStringList selected;
        QTest::mousePress(window, Qt::LeftButton, {}, origin + cellCenter(0, 0));
        QTest::mouseMove(window, origin + cellCenter(2, 0));
        QTest::mouseMove(window, origin + cellCenter(4, 0));
        QTest::mouseRelease(window, Qt::LeftButton, {}, origin + cellCenter(4, 0));
        selected << _sceneDisplay->screenWindow()->selectedText(Screen::PlainText);
        QTest::qWait(pause);
        QTest::mouseDClick(window, Qt::LeftButton, {}, origin + cellCenter(7, 0));
        selected << _sceneDisplay->screenWindow()->selectedText(Screen::PlainText);
        QTest::mouseClick(window, Qt::LeftButton, {}, origin + cellCenter(7, 0));
        selected << _sceneDisplay->screenWindow()->selectedText(Screen::PlainText);
        QTest::qWait(pause);
        return selected;
    };
    const QStringList fromWidget = select(_window->windowHandle(), _widgetDisplay->mapTo(_window.get(), QPoint(0, 0)));
    QCOMPARE(fromWidget.at(1), QStringLiteral("beta"));
    QVERIFY(fromWidget.at(2).contains(QLatin1String("alpha beta gamma")));
    QCOMPARE(select(_quickWindow.get(), QPoint(0, 0)), fromWidget);
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
    TerminalScrollBar* widgetBar = _widgetDisplay->scrollBar();
    TerminalScrollBar* sceneBar = _sceneDisplay->scrollBar();
    QTRY_VERIFY(widgetBar->maximum() > 0);
    QCOMPARE(sceneBar->maximum(), widgetBar->maximum());
    QCOMPARE(sceneBar->geometry(), widgetBar->geometry());

    const QRect bar = widgetBar->geometry();
    const auto handle = [](TerminalScrollBar* scrollBar) {
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
    const QPoint widgetOrigin = _widgetDisplay->mapTo(_window.get(), QPoint(0, 0));
    // Each action goes to both bars, at the same place in the bar.
    const auto both = [&](const std::function<void(QWindow*, QPoint)>& action) {
        action(_window->windowHandle(), widgetOrigin + bar.topLeft());
        action(_quickWindow.get(), bar.topLeft());
        QCOMPARE(sceneBar->value(), widgetBar->value());
    };
    widgetBar->setValue(0);
    sceneBar->setValue(0);

    both([&](QWindow* window, QPoint origin) {
        QTest::mouseClick(window, Qt::LeftButton, {}, origin + QPoint(bar.width() / 2, bar.height() - bar.width() / 2));
    });
    QVERIFY(widgetBar->value() > 0);
    both([&](QWindow* window, QPoint origin) {
        QTest::mouseClick(window, Qt::LeftButton, {}, origin + QPoint(bar.width() / 2, bar.height() * 3 / 4));
    });

    QSignalSpy widgetMoved(widgetBar, &QAbstractSlider::sliderMoved);
    QSignalSpy sceneMoved(sceneBar, &QAbstractSlider::sliderMoved);
    const QPoint handleCenter = handle(widgetBar).center();
    QCOMPARE(handle(sceneBar), handle(widgetBar));
    both([&](QWindow* window, QPoint origin) {
        const QPoint grab = origin + handleCenter;
        QTest::mousePress(window, Qt::LeftButton, {}, grab);
        QTest::mouseMove(window, grab - QPoint(0, 20));
        QTest::mouseMove(window, grab - QPoint(0, 60));
        QTest::mouseRelease(window, Qt::LeftButton, {}, grab - QPoint(0, 60));
    });
    QVERIFY(widgetMoved.count() > 0);
    QCOMPARE(sceneMoved.count(), widgetMoved.count());

    both([&](QWindow* window, QPoint origin) {
        const QPointF position = origin + QPoint(bar.width() / 2, bar.height() / 2);
        QWheelEvent wheel(position, window->mapToGlobal(position), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(window, &wheel);
    });

    // A double click adds a marker and doesn't scroll.
    QSignalSpy widgetMarkers(widgetBar, &TerminalScrollBar::overlaysChanged);
    QSignalSpy sceneMarkers(sceneBar, &TerminalScrollBar::overlaysChanged);
    both([&](QWindow* window, QPoint origin) {
        QTest::mouseDClick(window, Qt::LeftButton, {}, origin + QPoint(bar.width() / 2, bar.height() / 3));
    });
    QVERIFY(widgetMarkers.count() > 0);
    QCOMPARE(sceneMarkers.count(), widgetMarkers.count());
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
