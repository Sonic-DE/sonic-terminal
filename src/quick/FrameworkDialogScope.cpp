/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "FrameworkDialogScope.h"

#include <Sonic/Logging/Log>

#include <QApplication>
#include <QEvent>
#include <QHash>
#include <QWidget>

namespace Konsole {
namespace {
    struct OpenScope {
        const void* scope;
        QString name;
    };

    // Open scopes, innermost last; a scope attached to an object may close out of order.
    QList<OpenScope>& openScopes()
    {
        static QList<OpenScope> list;
        return list;
    }

    QHash<const QWidget*, QString>& recorded()
    {
        static QHash<const QWidget*, QString> hash;
        return hash;
    }

    void record(QWidget* widget, const QString& name)
    {
        if (recorded().contains(widget)) {
            return;
        }
        recorded().insert(widget, name);
        QObject::connect(widget, &QObject::destroyed, qApp, [widget] {
            recorded().remove(widget);
        });
        sonicDebug() << "framework dialog" << widget->metaObject()->className() << "in scope" << name;
    }

    class Recorder : public QObject {
    public:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::Show && !openScopes().isEmpty()) {
                if (auto* widget = qobject_cast<QWidget*>(watched); widget && widget->isWindow()) {
                    record(widget, openScopes().constLast().name);
                }
            }
            return false;
        }
    };

    void ensureRecorder()
    {
        static Recorder* recorder = nullptr;
        if (!recorder && qApp) {
            recorder = new Recorder;
            recorder->setParent(qApp);
            qApp->installEventFilter(recorder);
        }
    }

    void close(const void* scope)
    {
        openScopes().removeIf([scope](const OpenScope& open) {
            return open.scope == scope;
        });
    }

    // Records only what is shown while it lives: it can outlive unrelated windows' creation.
    class AttachedScope : public QObject {
    public:
        AttachedScope(QObject* lifetime, const QString& name)
            : QObject(lifetime)
        {
            ensureRecorder();
            openScopes().append({this, name});
        }

        ~AttachedScope() override
        {
            close(this);
        }
    };
}

FrameworkDialogScope::FrameworkDialogScope(const QString& name)
    : _name(name)
    , _before(QApplication::topLevelWidgets())
{
    ensureRecorder();
    openScopes().append({this, name});
}

FrameworkDialogScope::~FrameworkDialogScope()
{
    // Created here but shown later (KNewStuff's dialog shows once its data is loaded).
    const QWidgetList widgets = QApplication::topLevelWidgets();
    for (QWidget* widget : widgets) {
        if (!_before.contains(widget)) {
            record(widget, _name);
        }
    }
    close(this);
}

void FrameworkDialogScope::attach(QObject* lifetime, const QString& name)
{
    new AttachedScope(lifetime, name);
}

QString FrameworkDialogScope::owner(const QWidget* widget)
{
    for (const QWidget* current = widget; current; current = current->parentWidget()) {
        if (const auto it = recorded().constFind(current); it != recorded().constEnd()) {
            return it.value();
        }
    }
    return QString();
}
}
