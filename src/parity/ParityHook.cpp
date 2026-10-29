// SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ParityHook.h"

#include "MainWindow.h"
#include "ViewManager.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "terminalDisplay/TerminalFonts.h"

#include <KActionCollection>
#include <KPageDialog>
#include <KPageView>
#include <KXMLGUIFactory>

#include <QAbstractItemView>
#include <QAccessible>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QTest>
#include <QTimer>

#include <Sonic/Logging/Log>

namespace Konsole::ParityHook {
namespace {
    class Backdrop : public QWidget {
    public:
        Backdrop(const QString& fill, const QRect& geometry)
            : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnBottomHint | Qt::WindowDoesNotAcceptFocus)
            , _fill(fill)
        {
            setAttribute(Qt::WA_ShowWithoutActivating);
            setGeometry(geometry);
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);
            if (_fill != QLatin1String("checker")) {
                painter.fillRect(rect(), QColor(_fill));
                return;
            }
            const int cell = 8;
            for (int y = 0; y < height(); y += cell) {
                for (int x = 0; x < width(); x += cell) {
                    painter.fillRect(x, y, cell, cell, ((x + y) / cell) % 2 ? Qt::white : Qt::black);
                }
            }
        }

    private:
        QString _fill;
    };

    class Runner : public QObject {
    public:
        explicit Runner(const QString& scriptPath)
        {
            QFile file(scriptPath);
            if (!file.open(QIODevice::ReadOnly)) {
                sonicCritical() << "parity: cannot read script" << scriptPath;
                return;
            }
            for (const QByteArray& line : file.readAll().split('\n')) {
                if (!line.trimmed().isEmpty() && !line.trimmed().startsWith('#')) {
                    _steps.append(QJsonDocument::fromJson(line).object());
                }
            }
            sonicInfo() << "parity: loaded" << _steps.size() << "steps from" << scriptPath;
        }

        void next(int delayMs = 0)
        {
            QTimer::singleShot(delayMs, this, [this] {
                run();
            });
        }

    private:
        MainWindow* window(int index = -1)
        {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (auto* mainWindow = qobject_cast<MainWindow*>(widget); mainWindow && mainWindow->isVisible() && !_windows.contains(mainWindow)) {
                    _windows.append(mainWindow);
                }
            }
            _windows.removeAll(nullptr);
            if (index < 0) {
                index = _currentWindow;
            }
            return index < _windows.size() ? _windows.at(index).data() : nullptr;
        }

        SessionController* controller()
        {
            MainWindow* mainWindow = window();
            return mainWindow ? mainWindow->viewManager()->activeViewController() : nullptr;
        }

        TerminalDisplay* view()
        {
            SessionController* active = controller();
            return active ? active->view().data() : nullptr;
        }

        QAction* findAction(const QString& id)
        {
            MainWindow* mainWindow = window();
            if (!mainWindow) {
                return nullptr;
            }
            const auto clients = mainWindow->guiFactory()->clients();
            for (KXMLGUIClient* client : clients) {
                if (QAction* action = client->actionCollection()->action(id)) {
                    return action;
                }
            }
            return nullptr;
        }

        void trace(const QString& line)
        {
            QFile file(QDir(_output).filePath(QStringLiteral("trace.txt")));
            if (file.open(QIODevice::Append | QIODevice::Text)) {
                file.write(line.toUtf8() + '\n');
            }
        }

        void save(const QPixmap& pixmap, const QString& name)
        {
            const QString path = QDir(_output).filePath(name + QStringLiteral(".png"));
            if (!pixmap.toImage().convertToFormat(QImage::Format_ARGB32).save(path)) {
                sonicWarning() << "parity: cannot save" << path;
            }
        }

        void saveScreen(const QString& name)
        {
            save(QGuiApplication::primaryScreen()->grabWindow(0), name + QStringLiteral(".screen"));
        }

        void dumpMenu(const QMenu* menu, const QString& indent)
        {
            for (QAction* action : menu->actions()) {
                if (!action->isVisible()) {
                    continue;
                }
                if (action->isSeparator()) {
                    trace(indent + QStringLiteral("----") + (action->text().isEmpty() ? QString() : QStringLiteral(" ") + action->text()));
                    continue;
                }
                trace(indent + QStringLiteral("%1 [%2]%3%4").arg(action->text(), action->shortcut().toString(QKeySequence::PortableText)).arg(action->isEnabled() ? QString() : QStringLiteral(" disabled")).arg(action->isCheckable() ? (action->isChecked() ? QStringLiteral(" checked") : QStringLiteral(" unchecked")) : QString()));
                if (action->menu()) {
                    dumpMenu(action->menu(), indent + QStringLiteral("  "));
                }
            }
        }

        void capturePopup(const QString& name, int remainingMs)
        {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) {
                if (remainingMs <= 0) {
                    return fail(QStringLiteral("no popup menu for ") + name);
                }
                QTimer::singleShot(50, this, [this, name, remainingMs] {
                    capturePopup(name, remainingMs - 50);
                });
                return;
            }
            QTimer::singleShot(300, this, [this, menu = QPointer<QMenu>(menu), name] {
                if (!menu) {
                    return fail(QStringLiteral("popup closed early: ") + name);
                }
                save(menu->grab(), QStringLiteral("menu-") + name);
                saveScreen(QStringLiteral("menu-") + name);
                trace(QStringLiteral("menu ") + name);
                dumpMenu(menu, QStringLiteral("  "));
                menu->close();
                next(200);
            });
        }

        QPoint cellCenter(TerminalDisplay* display, int column, int line) const
        {
            const int width = display->terminalFont()->fontWidth();
            const int height = display->terminalFont()->fontHeight();
            return display->contentRect().topLeft() + QPoint(column * width + width / 2, line * height + height / 2);
        }

        static Qt::KeyboardModifiers modifiers(const QJsonObject& step)
        {
            Qt::KeyboardModifiers result;
            const QString names = step.value(QStringLiteral("modifiers")).toString();
            if (names.contains(QLatin1String("ctrl"))) {
                result |= Qt::ControlModifier;
            }
            if (names.contains(QLatin1String("alt"))) {
                result |= Qt::AltModifier;
            }
            if (names.contains(QLatin1String("shift"))) {
                result |= Qt::ShiftModifier;
            }
            return result;
        }

        void sendMouse(const QJsonObject& step)
        {
            TerminalDisplay* display = view();
            if (!display) {
                return fail(QStringLiteral("mouse: no active view"));
            }
            static const QHash<QString, QEvent::Type> types = {{QStringLiteral("press"), QEvent::MouseButtonPress},
                {QStringLiteral("move"), QEvent::MouseMove},
                {QStringLiteral("release"), QEvent::MouseButtonRelease},
                {QStringLiteral("double"), QEvent::MouseButtonDblClick}};
            const QEvent::Type type = types.value(step.value(QStringLiteral("event")).toString(), QEvent::None);
            const QPoint local = cellCenter(display, step.value(QStringLiteral("column")).toInt(), step.value(QStringLiteral("line")).toInt());
            const Qt::MouseButton button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
            const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
            QMouseEvent event(type, local, display->mapToGlobal(local), button, buttons, modifiers(step));
            QCoreApplication::sendEvent(display, &event);
            next(step.value(QStringLiteral("delay")).toInt(50));
        }

        void sendText(const QString& text, bool toFocus)
        {
            QWidget* target = toFocus ? QApplication::focusWidget() : view();
            if (!target) {
                return fail(QStringLiteral("type: no target"));
            }
            for (const QChar character : text) {
                const int key = character == u'\r' ? Qt::Key_Return : 0;
                const QString keyText = character == u'\r' ? QString() : QString(character);
                QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, keyText);
                QCoreApplication::sendEvent(target, &press);
                QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, keyText);
                QCoreApplication::sendEvent(target, &release);
            }
            next(50);
        }

        void sendKey(const QString& sequence)
        {
            const QKeySequence keys = QKeySequence::fromString(sequence, QKeySequence::PortableText);
            if (keys.isEmpty()) {
                return fail(QStringLiteral("bad key sequence ") + sequence);
            }
            QWidget* target = QApplication::focusWidget() ? QApplication::focusWidget() : window();
            if (!target || !target->window()->windowHandle()) {
                return fail(QStringLiteral("key: no window"));
            }
            // QTest's window path goes through the window system, where shortcuts are matched.
            QTest::keyClick(target->window()->windowHandle(), keys[0].key(), keys[0].keyboardModifiers());
            next(300);
        }

        void waitText(const QString& text, int remainingMs)
        {
            SessionController* active = controller();
            if (active && active->session() && active->session()->getAllDisplayedText().contains(text)) {
                return next();
            }
            if (remainingMs <= 0) {
                const QString shown = active && active->session() ? active->session()->getAllDisplayedText() : QStringLiteral("<no session>");
                return fail(QStringLiteral("waitText timed out: ") + text + QStringLiteral("\nscreen:\n") + shown);
            }
            QTimer::singleShot(50, this, [this, text, remainingMs] {
                waitText(text, remainingMs - 50);
            });
        }

        QDialog* visibleDialog() const
        {
            if (auto* modal = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                return modal;
            }
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (auto* dialog = qobject_cast<QDialog*>(widget); dialog && dialog->isVisible()) {
                    return dialog;
                }
            }
            return nullptr;
        }

        void captureDialog(const QString& name, bool close, int remainingMs)
        {
            QDialog* dialog = visibleDialog();
            if (!dialog) {
                if (remainingMs <= 0) {
                    return fail(QStringLiteral("no dialog visible for ") + name);
                }
                QTimer::singleShot(100, this, [this, name, close, remainingMs] {
                    captureDialog(name, close, remainingMs - 100);
                });
                return;
            }
            trace(QStringLiteral("dialog ") + name + QStringLiteral(" ") + dialog->windowTitle());
            QList<QModelIndex> pages;
            if (auto* view = dialog->findChild<KPageView*>()) {
                QList<QModelIndex> pending = {QModelIndex()};
                while (!pending.isEmpty()) {
                    const QModelIndex parent = pending.takeFirst();
                    for (int row = 0; row < view->model()->rowCount(parent); ++row) {
                        const QModelIndex index = view->model()->index(row, 0, parent);
                        pages.append(index);
                        pending.append(index);
                    }
                }
            }
            capturePages(QPointer<QDialog>(dialog), name, pages, close);
        }

        void capturePages(QPointer<QDialog> dialog, const QString& name, QList<QModelIndex> pages, bool close)
        {
            QTimer::singleShot(400, this, [this, dialog, name, pages, close]() mutable {
                if (!dialog) {
                    return fail(QStringLiteral("dialog closed early: ") + name);
                }
                QString suffix;
                if (auto* view = dialog->findChild<KPageView*>(); view && !pages.isEmpty()) {
                    const QModelIndex page = pages.takeFirst();
                    view->setCurrentPage(page);
                    suffix = QStringLiteral("-") + page.data(Qt::DisplayRole).toString().remove(u'&').replace(u' ', u'_');
                    trace(QStringLiteral("  page ") + page.data(Qt::DisplayRole).toString());
                    QTimer::singleShot(300, this, [this, dialog, name, pages, close, suffix] {
                        if (!dialog) {
                            return fail(QStringLiteral("dialog closed early: ") + name);
                        }
                        save(dialog->grab(), QStringLiteral("dialog-") + name + suffix);
                        saveScreen(QStringLiteral("dialog-") + name + suffix);
                        if (!pages.isEmpty()) {
                            return capturePages(dialog, name, pages, close);
                        }
                        if (close) {
                            dialog->reject();
                        }
                        next(300);
                    });
                    return;
                }
                save(dialog->grab(), QStringLiteral("dialog-") + name);
                saveScreen(QStringLiteral("dialog-") + name);
                if (close) {
                    dialog->reject();
                }
                next(300);
            });
        }

        void writeFile(const QString& name, const QString& text)
        {
            QFile file(QDir(_output).filePath(name));
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(text.toUtf8());
            }
            trace(QStringLiteral("wrote ") + name);
        }

        void introspect(const QStringList& paths)
        {
            QDBusConnection bus = QDBusConnection::sessionBus();
            const QString self = bus.baseService();
            QStringList names;
            const QStringList all = bus.interface()->registeredServiceNames().value();
            for (const QString& name : all) {
                if (!name.startsWith(u':') && bus.interface()->serviceOwner(name).value() == self) {
                    names.append(QString(name).replace(QString::number(QCoreApplication::applicationPid()), QStringLiteral("<pid>")));
                }
            }
            names.sort();
            QString text = QStringLiteral("names: ") + names.join(QStringLiteral(", ")) + u'\n';
            for (const QString& path : paths) {
                QDBusMessage call = QDBusMessage::createMethodCall(self, path, QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"));
                const QDBusMessage reply = bus.call(call);
                text += QStringLiteral("== ") + path + u'\n'
                    + (reply.type() == QDBusMessage::ReplyMessage ? reply.arguments().value(0).toString() : QStringLiteral("ERROR ") + reply.errorName()) + u'\n';
            }
            writeFile(QStringLiteral("dbus.txt"), text);
            next();
        }

        void xprop(const QString& name)
        {
            MainWindow* mainWindow = window();
            if (!mainWindow) {
                return fail(QStringLiteral("xprop: no window"));
            }
            static const QStringList perRun = {QStringLiteral("_NET_WM_PID"), QStringLiteral("_NET_WM_USER_TIME"), QStringLiteral("WM_CLIENT_LEADER"),
                QStringLiteral("_NET_WM_SYNC_REQUEST_COUNTER"), QStringLiteral("_KDE_NET_WM_USER_CREATION_TIME"),
                QStringLiteral("WM_CLIENT_MACHINE"), QStringLiteral("_NET_WM_USER_TIME_WINDOW"),
                QStringLiteral("_NET_WM_ICON"), QStringLiteral("WM_COMMAND")};
            QProcess process;
            process.start(QStringLiteral("xprop"), {QStringLiteral("-id"), QString::number(mainWindow->winId())});
            process.waitForFinished(5000);
            QStringList lines;
            const QStringList output = QString::fromUtf8(process.readAllStandardOutput()).split(u'\n');
            for (const QString& line : output) {
                const QString key = line.section(u'(', 0, 0).section(u':', 0, 0).trimmed();
                if (line.startsWith(u'\t') || perRun.contains(key)) {
                    continue;
                }
                lines.append(QString(line).replace(QRegularExpression(QStringLiteral("0x[0-9a-f]+")), QStringLiteral("0xID")));
            }
            writeFile(QStringLiteral("xprop-") + name + QStringLiteral(".txt"), lines.join(u'\n') + u'\n');
            next();
        }

        void accessible(const QString& name)
        {
            TerminalDisplay* display = view();
            QAccessibleInterface* interface = display ? QAccessible::queryAccessibleInterface(display) : nullptr;
            if (!interface) {
                return fail(QStringLiteral("accessible: no interface"));
            }
            QString text = QStringLiteral("role %1\nname %2\ndescription %3\n")
                               .arg(int(interface->role()))
                               .arg(interface->text(QAccessible::Name), interface->text(QAccessible::Description));
            if (QAccessibleTextInterface* textInterface = interface->textInterface()) {
                int start = 0;
                int end = 0;
                textInterface->selection(0, &start, &end);
                text += QStringLiteral("characters %1\ncursor %2\nselections %3 (%4-%5)\ntext:\n%6\n")
                            .arg(textInterface->characterCount())
                            .arg(textInterface->cursorPosition())
                            .arg(textInterface->selectionCount())
                            .arg(start)
                            .arg(end)
                            .arg(textInterface->text(0, textInterface->characterCount()));
            }
            writeFile(QStringLiteral("accessible-") + name + QStringLiteral(".txt"), text);
            next();
        }

        void environment(const QString& fileName, int remainingMs)
        {
            QFile file(QDir::home().filePath(fileName));
            if (!file.exists() || file.size() == 0) {
                if (remainingMs <= 0) {
                    return fail(QStringLiteral("env: no ") + fileName);
                }
                QTimer::singleShot(100, this, [this, fileName, remainingMs] {
                    environment(fileName, remainingMs - 100);
                });
                return;
            }
            static const QStringList kept = {QStringLiteral("TERM"), QStringLiteral("COLORTERM"), QStringLiteral("SHELL_SESSION_ID"),
                QStringLiteral("WINDOWID"), QStringLiteral("FLATPAK_TTY_PROGRESS"), QStringLiteral("PS1"),
                QStringLiteral("LANGUAGE")};
            static const QStringList masked = {QStringLiteral("SHELL_SESSION_ID"), QStringLiteral("WINDOWID"), QStringLiteral("KONSOLE_DBUS_SERVICE"),
                QStringLiteral("KONSOLE_DBUS_ACTIVATION_COOKIE")};
            file.open(QIODevice::ReadOnly | QIODevice::Text);
            QStringList lines;
            const QStringList all = QString::fromUtf8(file.readAll()).split(u'\n');
            for (const QString& line : all) {
                const QString key = line.section(u'=', 0, 0);
                if (!key.startsWith(QLatin1String("KONSOLE_")) && !kept.contains(key)) {
                    continue;
                }
                lines.append(masked.contains(key) ? key + (line.section(u'=', 1).isEmpty() ? QStringLiteral("=") : QStringLiteral("=<set>")) : line);
            }
            writeFile(QStringLiteral("env-masked.txt"), lines.join(u'\n') + u'\n');
            next();
        }

        void describe(const QString& name)
        {
            window();
            QString text;
            for (int i = 0; i < _windows.size(); ++i) {
                MainWindow* mainWindow = _windows.at(i);
                const auto sessions = mainWindow->viewManager()->sessions();
                text += QStringLiteral("window %1: %2 sessions, menubar %3, state %4\n")
                            .arg(i)
                            .arg(sessions.size())
                            .arg(mainWindow->menuBar()->isVisible() ? QStringLiteral("shown") : QStringLiteral("hidden"))
                            .arg(int(mainWindow->windowState()));
                QStringList lines;
                for (Session* session : sessions) {
                    lines.append(QStringLiteral("  session %1 | %2 | %3\n")
                            .arg(session->title(Session::NameRole), session->program(), QString(session->currentWorkingDirectory()).replace(QDir::homePath(), QStringLiteral("~"))));
                }
                lines.sort();
                text += lines.join(QString());
            }
            writeFile(QStringLiteral("describe-") + name + QStringLiteral(".txt"), text);
            next();
        }

        void fail(const QString& message)
        {
            sonicCritical() << "parity:" << message;
            trace(QStringLiteral("FAIL ") + message);
            QCoreApplication::exit(3);
        }

        void run()
        {
            if (_index >= _steps.size()) {
                sonicInfo() << "parity: script finished";
                QCoreApplication::exit(0);
                return;
            }
            const QJsonObject step = _steps.at(_index++);
            const QString op = step.value(QStringLiteral("op")).toString();
            sonicDebug() << "parity: step" << _index << op;

            if (op == QLatin1String("output")) {
                _output = step.value(QStringLiteral("dir")).toString();
                QDir().mkpath(_output);
                return next();
            }
            if (op == QLatin1String("waitWindow")) {
                const int index = step.value(QStringLiteral("window")).toInt(0);
                if (!window(index) || !window(index)->viewManager()->activeViewController()) {
                    --_index;
                    return next(100);
                }
                return next(300);
            }
            if (op == QLatin1String("useWindow")) {
                _currentWindow = step.value(QStringLiteral("window")).toInt();
                if (MainWindow* mainWindow = window()) {
                    mainWindow->activateWindow();
                }
                return next(300);
            }
            if (op == QLatin1String("wait")) {
                return next(step.value(QStringLiteral("ms")).toInt());
            }
            if (op == QLatin1String("geometry")) {
                if (MainWindow* mainWindow = window()) {
                    mainWindow->move(step.value(QStringLiteral("x")).toInt(), step.value(QStringLiteral("y")).toInt());
                    mainWindow->resize(step.value(QStringLiteral("width")).toInt(), step.value(QStringLiteral("height")).toInt());
                }
                return next(300);
            }
            if (op == QLatin1String("windowState")) {
                if (MainWindow* mainWindow = window()) {
                    mainWindow->setWindowState(step.value(QStringLiteral("state")).toString() == QLatin1String("maximized") ? Qt::WindowMaximized
                                                                                                                            : Qt::WindowNoState);
                }
                return next(800);
            }
            if (op == QLatin1String("backdrop")) {
                const QString fill = step.value(QStringLiteral("fill")).toString();
                if (fill.isEmpty()) {
                    qDeleteAll(_backdrops);
                    _backdrops.clear();
                    return next(300);
                }
                auto* backdrop = new Backdrop(fill, QGuiApplication::primaryScreen()->geometry());
                backdrop->show();
                _backdrops.append(backdrop);
                return next(500);
            }
            if (op == QLatin1String("type")) {
                return sendText(step.value(QStringLiteral("text")).toString(), step.value(QStringLiteral("target")).toString() == QLatin1String("focus"));
            }
            if (op == QLatin1String("key")) {
                return sendKey(step.value(QStringLiteral("sequence")).toString());
            }
            if (op == QLatin1String("waitText")) {
                return waitText(step.value(QStringLiteral("text")).toString(), step.value(QStringLiteral("timeout")).toInt(5000));
            }
            if (op == QLatin1String("mouse")) {
                return sendMouse(step);
            }
            if (op == QLatin1String("selection")) {
                TerminalDisplay* display = view();
                const QString text = display && display->screenWindow() ? display->screenWindow()->selectedText(Screen::PreserveLineBreaks) : QString();
                trace(QStringLiteral("selection %1: %2").arg(step.value(QStringLiteral("label")).toString(), text));
                return next();
            }
            if (op == QLatin1String("action")) {
                QAction* action = findAction(step.value(QStringLiteral("id")).toString());
                if (!action) {
                    return fail(QStringLiteral("no action ") + step.value(QStringLiteral("id")).toString());
                }
                // Deferred, because some actions open modal dialogs and don't return until closed.
                QTimer::singleShot(0, action, &QAction::trigger);
                return next(step.value(QStringLiteral("delay")).toInt(500));
            }
            if (op == QLatin1String("introspect")) {
                return introspect(step.value(QStringLiteral("paths")).toVariant().toStringList());
            }
            if (op == QLatin1String("xprop")) {
                return xprop(step.value(QStringLiteral("name")).toString());
            }
            if (op == QLatin1String("accessible")) {
                return accessible(step.value(QStringLiteral("name")).toString());
            }
            if (op == QLatin1String("env")) {
                return environment(step.value(QStringLiteral("file")).toString(), step.value(QStringLiteral("timeout")).toInt(5000));
            }
            if (op == QLatin1String("homeFile")) {
                QFile file(QDir::home().filePath(step.value(QStringLiteral("file")).toString()));
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    return fail(QStringLiteral("homeFile: cannot read ") + file.fileName());
                }
                writeFile(step.value(QStringLiteral("file")).toString().replace(u'/', u'_') + QStringLiteral(".txt"), QString::fromUtf8(file.readAll()));
                return next();
            }
            if (op == QLatin1String("describe")) {
                return describe(step.value(QStringLiteral("name")).toString());
            }
            if (op == QLatin1String("listActions")) {
                if (MainWindow* mainWindow = window()) {
                    const auto clients = mainWindow->guiFactory()->clients();
                    for (KXMLGUIClient* client : clients) {
                        const auto actions = client->actionCollection()->actions();
                        for (QAction* action : actions) {
                            trace(QStringLiteral("action %1 \"%2\"").arg(action->objectName(), action->text()));
                        }
                    }
                }
                return next();
            }
            if (op == QLatin1String("capture")) {
                const QString name = step.value(QStringLiteral("name")).toString();
                if (MainWindow* mainWindow = window()) {
                    save(mainWindow->grab(), name + QStringLiteral(".client"));
                }
                saveScreen(name);
                trace(QStringLiteral("capture ") + name);
                return next();
            }
            if (op == QLatin1String("menu")) {
                MainWindow* mainWindow = window();
                const QString name = step.value(QStringLiteral("name")).toString();
                auto* menu = mainWindow ? qobject_cast<QMenu*>(mainWindow->guiFactory()->container(name, mainWindow)) : nullptr;
                if (!menu) {
                    // Some menus (Bookmarks) are actions with their own menu, not KXmlGui containers.
                    QAction* action = findAction(name);
                    menu = action ? action->menu() : nullptr;
                }
                if (!menu) {
                    return fail(QStringLiteral("no menu ") + name);
                }
                const QRect item = mainWindow->menuBar()->actionGeometry(menu->menuAction());
                menu->popup(mainWindow->menuBar()->isVisible() ? mainWindow->menuBar()->mapToGlobal(item.bottomLeft()) : mainWindow->mapToGlobal(QPoint(0, 0)));
                return capturePopup(name, 2000);
            }
            if (op == QLatin1String("popup")) {
                return capturePopup(step.value(QStringLiteral("name")).toString(), step.value(QStringLiteral("timeout")).toInt(2000));
            }
            if (op == QLatin1String("contextMenu")) {
                TerminalDisplay* display = view();
                if (!display) {
                    return fail(QStringLiteral("contextMenu: no active view"));
                }
                const QPoint local = cellCenter(display, step.value(QStringLiteral("column")).toInt(), step.value(QStringLiteral("line")).toInt());
                // Posted: the context menu runs its own event loop until it's closed.
                QCoreApplication::postEvent(display, new QContextMenuEvent(QContextMenuEvent::Mouse, local, display->mapToGlobal(local), modifiers(step)));
                return capturePopup(step.value(QStringLiteral("name")).toString(), 2000);
            }
            if (op == QLatin1String("dialog")) {
                // Some lists (notification events) come in hash order; sort them so runs compare.
                if (step.value(QStringLiteral("sortColumn")).isDouble()) {
                    if (QDialog* dialog = visibleDialog()) {
                        const auto views = dialog->findChildren<QAbstractItemView*>();
                        for (QAbstractItemView* itemView : views) {
                            itemView->model()->sort(step.value(QStringLiteral("sortColumn")).toInt(), Qt::AscendingOrder);
                        }
                    }
                }
                return captureDialog(step.value(QStringLiteral("name")).toString(), step.value(QStringLiteral("close")).toBool(true), 3000);
            }
            if (op == QLatin1String("quit")) {
                sonicInfo() << "parity: quit";
                qDeleteAll(_backdrops);
                QCoreApplication::exit(0);
                return;
            }
            fail(QStringLiteral("unknown op ") + op);
        }

        QList<QJsonObject> _steps;
        QList<QPointer<MainWindow>> _windows;
        QList<Backdrop*> _backdrops;
        int _index = 0;
        int _currentWindow = 0;
        QString _output = QDir::currentPath();
    };
}

QCommandLineOption commandLineOption()
{
    return QCommandLineOption(QStringLiteral("parity-script"), QStringLiteral("Run a parity test script (testing only)."), QStringLiteral("file"));
}

void start(const QString& scriptPath)
{
    QApplication::setCursorFlashTime(0);
    auto* runner = new Runner(scriptPath);
    runner->setParent(qApp);
    runner->next();
}
}
