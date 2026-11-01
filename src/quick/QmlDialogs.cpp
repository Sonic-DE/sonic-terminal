/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "QmlDialogs.h"

#include "CompatWindows.h"

#include <Sonic/Logging/Log>

#include <KLocalizedQmlContext>

#include <QCoreApplication>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QWidget>

#include <algorithm>

namespace Konsole {
namespace QmlDialogs {
    QQmlEngine* engine()
    {
        // Qt Quick settings must precede the first QML.
        static QQmlEngine* engine = [] {
            if (QQuickStyle::name().isEmpty()) {
                QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
            }
            // QML text renders like the widgets' (only affects items created afterwards).
            QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
            auto* engine = new QQmlEngine(qApp);
            KLocalization::setupLocalizedContext(engine);
            QObject::connect(engine, &QQmlEngine::warnings, engine, [](const QList<QQmlError>& warnings) {
                for (const QQmlError& warning : warnings) {
                    sonicWarning() << "qml:" << warning.toString();
                }
            });
            return engine;
        }();
        return engine;
    }

    QQuickWindow* open(const QString& type, QWidget* opener, const QVariantMap& properties, Qt::WindowModality modality)
    {
        QQmlComponent component(engine(), QStringLiteral("org.sonicde.terminal"), type);
        QObject* object = component.createWithInitialProperties(properties);
        auto* window = qobject_cast<QQuickWindow*>(object);
        if (!window) {
            sonicCritical() << "dialog" << type << "didn't load:" << component.errorString();
            delete object;
            return nullptr;
        }
        QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
        QWidget* openerWindow = opener ? opener->window() : nullptr;
        if (openerWindow && !CompatWindows::contains(openerWindow)) {
            // A KPart host's window: its native window may not exist yet.
            openerWindow->winId();
        }
        QWindow* parent = openerWindow ? CompatWindows::shownWindow(openerWindow) : nullptr;
        window->setTransientParent(parent);
        window->setModality(modality);
        if (parent) {
            // QDialog::adjustPosition(): centred on the parent, inside its screen.
            QRect frame(QPoint(), window->size());
            frame.moveCenter(parent->frameGeometry().center());
            if (QScreen* screen = parent->screen()) {
                const QRect area = screen->availableGeometry();
                frame.moveLeft(std::clamp(frame.left(), area.left(), std::max(area.left(), area.right() - frame.width())));
                frame.moveTop(std::clamp(frame.top(), area.top(), std::max(area.top(), area.bottom() - frame.height())));
            }
            window->setPosition(frame.topLeft());
        }
        QObject::connect(window, &QWindow::visibleChanged, window, [window](bool visible) {
            if (!visible) {
                window->deleteLater();
            }
        });
        sonicDebug() << "dialog" << type << "opened, transient for" << parent;
        window->setVisible(true);
        return window;
    }

    namespace {
        // A QML signal reaches only slots by name: this one runs the callback.
        class AcceptedRelay : public QObject {
            Q_OBJECT
        public:
            AcceptedRelay(QObject* parent, QObject* context, const std::function<void()>& callback)
                : QObject(parent)
                , _context(context)
                , _callback(callback)
            {
            }

        public Q_SLOTS:
            void fire()
            {
                if (_context) {
                    _callback();
                }
            }

        private:
            QPointer<QObject> _context;
            std::function<void()> _callback;
        };
    }

    void onAccepted(QQuickWindow* dialog, QObject* context, const std::function<void()>& callback)
    {
        auto* relay = new AcceptedRelay(dialog, context, callback);
        QObject::connect(dialog, SIGNAL(accepted()), relay, SLOT(fire()));
    }
}
}

#include "QmlDialogs.moc"
