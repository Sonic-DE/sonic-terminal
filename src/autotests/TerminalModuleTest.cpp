/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// A program with no link to the terminal's libraries uses the QML module as Ecco does: found
// on the import path, in a QQuickWidget, through its API by name. It keeps its own settings
// and its own quitting.

#include <KActionCollection>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KXmlGuiWindow>

#include <QAction>
#include <QApplication>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QWindow>

class TerminalModuleTest : public QObject {
    Q_OBJECT

public:
    static void initMain()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

private Q_SLOTS:
    void init();
    void cleanup();
    void testUsesTheTerminalsXml();
    void testFollowsTheItem();
    void testMenusAreTransientForTheHost();
    void testShortcutsNeedTheFocus();
    void testShortcutsSaveInTheTerminalsConfig();
    void testShellRunsAndEnds();
    void testHostSettingsAreLeftAlone();
    void testEveryFileNamesTheDesktopStyle();
    void testDialogsKeepTheDesktopStyle();
    void testStaysWhileTheHostKeepsRunning();
    void testDoesNotKeepTheHostRunning();

private:
    QObject* root() const;
    KXmlGuiWindow* compat() const;
    QRect itemRect() const;

    QPointer<QMainWindow> _host;
    QPointer<QQuickWidget> _quick;
    QPointer<QLineEdit> _other;
};

void TerminalModuleTest::init()
{
    _host = new QMainWindow;
    auto* central = new QWidget;
    auto* layout = new QHBoxLayout(central);
    _other = new QLineEdit;
    layout->addWidget(_other);
    _quick = new QQuickWidget;
    _quick->setFocusPolicy(Qt::StrongFocus);
    _quick->setResizeMode(QQuickWidget::SizeRootObjectToView);
    _quick->engine()->addImportPath(QStringLiteral(TERMINAL_QML_DIR));
    _quick->loadFromModule("org.sonicde.terminal.app", "TerminalArea");
    QCOMPARE(_quick->status(), QQuickWidget::Ready);
    layout->addWidget(_quick, 1);
    _host->setCentralWidget(central);
    _host->setGeometry(100, 100, 900, 500);
    _host->show();
    _host->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(_host));
    _quick->setFocus();
    QVERIFY(compat());
    QTRY_VERIFY(_quick->rootObject()->hasActiveFocus());
}

void TerminalModuleTest::cleanup()
{
    delete _host;
}

QObject* TerminalModuleTest::root() const
{
    return _quick ? _quick->rootObject() : nullptr;
}

KXmlGuiWindow* TerminalModuleTest::compat() const
{
    return root() ? qobject_cast<KXmlGuiWindow*>(root()->property("compatWindow").value<QObject*>()) : nullptr;
}

QRect TerminalModuleTest::itemRect() const
{
    return QRect(_quick->mapToGlobal(QPoint(0, 0)), _quick->size());
}

void TerminalModuleTest::testUsesTheTerminalsXml()
{
    QCOMPARE(compat()->componentName(), QStringLiteral("konsole"));
    QVERIFY2(compat()->xmlFile().endsWith(QLatin1String("konsoleui.rc")), qPrintable(compat()->xmlFile()));
}

void TerminalModuleTest::testFollowsTheItem()
{
    QTRY_COMPARE(compat()->geometry(), itemRect());
    _host->resize(1000, 600);
    QTRY_COMPARE(compat()->geometry(), itemRect());
    _host->move(_host->x() + 40, _host->y() + 30);
    QTRY_COMPARE(compat()->geometry().topLeft(), itemRect().topLeft());
    QTRY_COMPARE(compat()->windowHandle()->geometry().topLeft(), itemRect().topLeft());
    // Narrower than the compat window would like to be: it stays the item's size.
    _host->resize(300, 150);
    QTRY_COMPARE(compat()->geometry(), itemRect());
}

void TerminalModuleTest::testMenusAreTransientForTheHost()
{
    QMenu menu(compat());
    menu.addAction(QStringLiteral("item"));
    menu.popup(itemRect().center());
    QTRY_VERIFY(menu.windowHandle());
    QTRY_COMPARE(menu.windowHandle()->transientParent(), _host->windowHandle());
    menu.close();
}

void TerminalModuleTest::testShortcutsNeedTheFocus()
{
    QObject* collectionObject = nullptr;
    QVERIFY(QMetaObject::invokeMethod(root(), "sessionActionCollection", Q_RETURN_ARG(QObject*, collectionObject)));
    auto* collection = qobject_cast<KActionCollection*>(collectionObject);
    QVERIFY(collection);
    auto* action = new QAction(collection);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F9));
    collection->addAction(QStringLiteral("test_action"), action);
    QSignalSpy triggered(action, &QAction::triggered);

    QTest::qWait(50);
    QTest::keyClick(_host->windowHandle(), Qt::Key_F9, Qt::ControlModifier | Qt::AltModifier);
    QTRY_COMPARE(triggered.count(), 1);

    _other->setFocus();
    QTRY_VERIFY(!root()->property("activeFocus").toBool());
    QTest::qWait(50);
    QTest::keyClick(_host->windowHandle(), Qt::Key_F9, Qt::ControlModifier | Qt::AltModifier);
    QTest::qWait(50);
    QCOMPARE(triggered.count(), 1);
}

void TerminalModuleTest::testShortcutsSaveInTheTerminalsConfig()
{
    QObject* collectionObject = nullptr;
    QVERIFY(QMetaObject::invokeMethod(root(), "sessionActionCollection", Q_RETURN_ARG(QObject*, collectionObject)));
    auto* collection = qobject_cast<KActionCollection*>(collectionObject);
    QVERIFY(collection);
    QAction* action = collection->action(QStringLiteral("edit_paste"));
    QVERIFY(action);
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F8));
    QVERIFY(QMetaObject::invokeMethod(root(), "saveShortcuts"));

    const QStringList keys = KConfigGroup(KSharedConfig::openConfig(QStringLiteral("konsolerc")), QStringLiteral("ActionShortcuts")).keyList();
    QVERIFY2(keys.filter(QStringLiteral("/edit_paste")).size() == 1, qPrintable(keys.join(QLatin1Char(','))));
    QVERIFY(!KSharedConfig::openConfig()->hasGroup(QStringLiteral("ActionShortcuts")));
}

void TerminalModuleTest::testShellRunsAndEnds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QFileInfo(dir.path()).canonicalFilePath();

    // A host asks right after loading, as with the part: later, the shell has already started.
    QQuickWidget quick;
    quick.setResizeMode(QQuickWidget::SizeRootObjectToView);
    quick.engine()->addImportPath(QStringLiteral(TERMINAL_QML_DIR));
    quick.loadFromModule("org.sonicde.terminal.app", "TerminalArea");
    QCOMPARE(quick.status(), QQuickWidget::Ready);
    QObject* area = quick.rootObject();
    QSignalSpy finished(area, SIGNAL(finished()));
    QVERIFY(QMetaObject::invokeMethod(area, "showShellInDir", Q_ARG(QString, path)));
    quick.resize(600, 300);
    quick.show();

    QString cwd;
    const auto workingDirectory = [&] {
        QMetaObject::invokeMethod(area, "currentWorkingDirectory", Q_RETURN_ARG(QString, cwd));
        return cwd;
    };
    QTRY_COMPARE_WITH_TIMEOUT(workingDirectory(), path, 10000);

    QVERIFY(QMetaObject::invokeMethod(area, "sendInput", Q_ARG(QString, QStringLiteral("exit\n"))));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
    QVERIFY(!area->property("compatWindow").value<QObject*>());
    // What holds the terminal is the host's; it goes on.
    QCOMPARE(quick.status(), QQuickWidget::Ready);
}

void TerminalModuleTest::testHostSettingsAreLeftAlone()
{
    QVERIFY(!compat()->autoSaveSettings());
    KConfigGroup state(KSharedConfig::openStateConfig(), QStringLiteral("MainWindow"));
    KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("MainWindow"));
    state.writeEntry("sentinel", 1);
    config.writeEntry("sentinel", 1);
    const QStringList stateKeys = state.keyList();
    const QStringList configKeys = config.keyList();

    _host->resize(1000, 600);
    QTRY_COMPARE(compat()->geometry(), itemRect());
    delete _host;
    QCoreApplication::processEvents();

    QCOMPARE(state.keyList(), stateKeys);
    QCOMPARE(config.keyList(), configKeys);
    state.deleteEntry("sentinel");
    config.deleteEntry("sentinel");
}

// The host's style applies to a bare "import QtQuick.Controls".
void TerminalModuleTest::testEveryFileNamesTheDesktopStyle()
{
    QDirIterator it(QStringLiteral(TERMINAL_QML_SOURCE_DIR), {QStringLiteral("*.qml")}, QDir::Files, QDirIterator::Subdirectories);
    int checked = 0;
    static const QRegularExpression controls(QStringLiteral("^import QtQuick\\.Controls as (\\w+)$"), QRegularExpression::MultilineOption);
    while (it.hasNext()) {
        QFile file(it.next());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(file.readAll());
        const QRegularExpressionMatch match = controls.match(text);
        QVERIFY2(match.hasMatch() || !text.contains(QLatin1String("import QtQuick.Controls")), qPrintable(file.fileName()));
        if (match.hasMatch()) {
            const QString expected = match.captured(0) + QStringLiteral("\nimport org.kde.desktop as ") + match.captured(1);
            QVERIFY2(text.contains(expected), qPrintable(file.fileName()));
            ++checked;
        }
    }
    QVERIFY(checked > 0);
}

// This test runs under another style (CMakeLists.txt).
void TerminalModuleTest::testDialogsKeepTheDesktopStyle()
{
    QCOMPARE(qgetenv("QT_QUICK_CONTROLS_STYLE"), QByteArray("Fusion"));
    QObject* collectionObject = nullptr;
    QVERIFY(QMetaObject::invokeMethod(root(), "sessionActionCollection", Q_RETURN_ARG(QObject*, collectionObject)));
    auto* collection = qobject_cast<KActionCollection*>(collectionObject);
    QVERIFY(collection);
    QAction* action = collection->action(QStringLiteral("adjust-history"));
    QVERIFY(action);
    action->trigger();

    QQuickWindow* dialog = nullptr;
    QTRY_VERIFY([&dialog] {
        const QWindowList windows = QGuiApplication::topLevelWindows();
        for (QWindow* window : windows) {
            auto* quick = qobject_cast<QQuickWindow*>(window);
            if (quick && quick->isVisible()) {
                dialog = quick;
                return true;
            }
        }
        return false;
    }());
    const QScopeGuard closeDialog([dialog] {
        dialog->close();
    });
    // By the item tree: a delegate's QObject parent is not the item holding it. Kirigami's own
    // QML follows the host's style; the imports can only choose for ours.
    int checked = 0;
    QList<QQuickItem*> items{dialog->contentItem()};
    while (!items.isEmpty()) {
        QQuickItem* item = items.takeFirst();
        items.append(item->childItems());
        const auto* background = item->inherits("QQuickAbstractButton") ? item->property("background").value<QQuickItem*>() : nullptr;
        const QString file = qmlContext(item) ? qmlContext(item)->baseUrl().toString() : QString();
        if (background && (file.contains(QLatin1String("/org/sonicde/terminal/")) || file.contains(QLatin1String("/org/kde/desktop/")))) {
            QVERIFY2(QByteArray(background->metaObject()->className()).startsWith("KQuickStyleItem"), qPrintable(file));
            ++checked;
        }
    }
    QVERIFY(checked > 0);
}

// A host that hides its windows and goes on (a tray icon) finds its terminal when it comes back.
void TerminalModuleTest::testStaysWhileTheHostKeepsRunning()
{
    QApplication::setQuitOnLastWindowClosed(false);
    QTimer::singleShot(0, _host, &QWidget::close);
    QTimer::singleShot(200, qApp, &QCoreApplication::quit);
    QCoreApplication::exec();
    QVERIFY(compat());
}

// Last: the application's event loop runs to its end here.
void TerminalModuleTest::testDoesNotKeepTheHostRunning()
{
    QVERIFY(compat()->isVisible());
    bool timedOut = false;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    connect(&watchdog, &QTimer::timeout, qApp, [&timedOut] {
        timedOut = true;
        QCoreApplication::quit();
    });
    watchdog.start(5000);
    QApplication::setQuitOnLastWindowClosed(true);
    QTimer::singleShot(0, _host, &QWidget::close);
    QCoreApplication::exec();
    QVERIFY2(!timedOut, "closing the host's last window didn't quit");
}

QTEST_MAIN(TerminalModuleTest)

#include "TerminalModuleTest.moc"
