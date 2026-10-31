/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The model-driven splits must behave like ViewSplitter's own rules, which a ViewSplitter
// made outside a model still follows: both trees get the same random operations and must
// agree after every step.

#include "../model/PaneModel.h"
#include "../colorscheme/ColorScheme.h"
#include "../colorscheme/ColorSchemeManager.h"
#include "../terminalDisplay/TerminalDisplay.h"
#include "../widgets/PaneWidgetView.h"
#include "../widgets/ViewContainer.h"
#include "../widgets/ViewSplitter.h"

#include <QHBoxLayout>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTest>

using namespace Konsole;

class PaneModelTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testSameAsViewSplitter_data();
    void testSameAsViewSplitter();
};

namespace {
TerminalDisplay* newDisplay()
{
    // A shown display needs a color scheme wallpaper (applyProfile would need a session).
    auto* display = new TerminalDisplay(nullptr);
    display->setWallpaper(ColorSchemeManager::instance()->defaultColorScheme()->wallpaper());
    return display;
}

struct Side {
    QWidget* host = nullptr;
    TabbedViewContainer* container = nullptr;
    ViewSplitter* top = nullptr;
    QList<QPointer<TerminalDisplay>> displays;

    int indexOf(const TerminalDisplay* display) const
    {
        for (int i = 0; i < displays.size(); ++i) {
            if (displays.at(i) == display) {
                return i;
            }
        }
        return -1;
    }
};

Side makeSide(bool managed)
{
    Side side;
    side.host = new QWidget;
    side.host->setAttribute(Qt::WA_DontShowOnScreen);
    // Layout only: bare displays (no session) can't paint.
    side.host->setUpdatesEnabled(false);
    side.host->resize(900, 600);
    auto* layout = new QHBoxLayout(side.host);
    layout->setContentsMargins(0, 0, 0, 0);
    side.container = new TabbedViewContainer(nullptr, side.host);
    // Its tab presentation needs sessions, which these bare displays don't have.
    QObject::disconnect(side.container, &QTabWidget::currentChanged, side.container, nullptr);
    layout->addWidget(side.container);
    auto* first = newDisplay();
    side.displays.append(first);
    if (managed) {
        PaneModel* model = side.container->paneModel();
        PaneNode* top = model->createSplit();
        side.top = PaneWidgetView::splitterOf(top);
        top->addDisplay(first, Qt::Horizontal);
        model->insertTab(-1, top);
    } else {
        side.top = new ViewSplitter();
        side.top->addTerminalDisplay(first, Qt::Horizontal);
        side.container->insertTab(-1, side.top, QString());
    }
    side.host->show();
    return side;
}

void settle()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

// The layout with split ids renumbered by first appearance and displays by their index.
QString normalized(const Side& side)
{
    QString layout = side.top->getChildWidgetsLayout();
    QHash<QString, int> splits;
    QString result;
    static const QRegularExpression token(QStringLiteral(R"(\((\d+)\)|(\d+))"));
    int last = 0;
    for (auto it = token.globalMatch(layout); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        result += layout.mid(last, match.capturedStart() - last);
        if (!match.captured(1).isEmpty()) {
            const int number = splits.value(match.captured(1), splits.size());
            splits.insert(match.captured(1), number);
            result += QStringLiteral("(s%1)").arg(number);
        } else {
            int index = -1;
            for (int i = 0; i < side.displays.size(); ++i) {
                if (side.displays.at(i) && QString::number(side.displays.at(i)->id()) == match.captured(2)) {
                    index = i;
                }
            }
            result += QStringLiteral("d%1").arg(index);
        }
        last = match.capturedEnd();
    }
    return result + layout.mid(last);
}

QString sizes(QSplitter* splitter)
{
    QStringList parts;
    for (int size : splitter->sizes()) {
        parts.append(QString::number(size));
    }
    QString result = QLatin1Char('<') + parts.join(u',') + QLatin1Char('>');
    for (int i = 0; i < splitter->count(); ++i) {
        if (auto* child = qobject_cast<QSplitter*>(splitter->widget(i))) {
            result += sizes(child);
        }
    }
    return result;
}

QString state(const Side& side)
{
    QStringList order;
    const auto found = side.top->findChildren<TerminalDisplay*>();
    for (TerminalDisplay* display : found) {
        order.append(QString::number(side.indexOf(display)) + (display->isVisibleTo(side.top) ? QString() : QStringLiteral("h")));
    }
    return normalized(side) + QStringLiteral(" order ") + order.join(u',') + QStringLiteral(" sizes ") + sizes(side.top)
        + QStringLiteral(" active ") + QString::number(side.indexOf(side.top->activeTerminalDisplay()))
        + QStringLiteral(" maximized ") + QString::number(side.top->terminalMaximized());
}
}

void PaneModelTest::testSameAsViewSplitter_data()
{
    QTest::addColumn<quint32>("seed");
    for (quint32 seed = 1; seed <= 40; ++seed) {
        QTest::newRow(qPrintable(QStringLiteral("seed %1").arg(seed))) << seed;
    }
}

void PaneModelTest::testSameAsViewSplitter()
{
    QFETCH(quint32, seed);
    QRandomGenerator random(seed);
    Side managed = makeSide(true);
    Side legacy = makeSide(false);
    settle();
    QCOMPARE(state(managed), state(legacy));

    for (int step = 0; step < 30; ++step) {
        const int live = managed.displays.size();
        const int pick = random.bounded(live);
        // Focus decides where a split goes; both sides focus the same display.
        managed.displays.at(pick)->setFocus();
        legacy.displays.at(pick)->setFocus();
        const int op = random.bounded(10);
        QString what;
        if (op < 5 || live == 1) {
            const auto orientation = random.bounded(2) ? Qt::Horizontal : Qt::Vertical;
            const auto behavior = random.bounded(2) ? ViewSplitter::AddBehavior::AddBefore : ViewSplitter::AddBehavior::AddAfter;
            what = QStringLiteral("split %1 %2 %3").arg(pick).arg(int(orientation)).arg(int(behavior));
            for (Side* side : {&managed, &legacy}) {
                auto* display = newDisplay();
                side->displays.append(display);
                side->top->clearMaximized();
                side->top->addTerminalDisplay(display, orientation, behavior);
            }
        } else if (op < 7) {
            what = QStringLiteral("remove %1").arg(pick);
            for (Side* side : {&managed, &legacy}) {
                side->top->clearMaximized();
                delete side->displays.takeAt(pick).data();
            }
        } else if (op < 9) {
            const int index = random.bounded(managed.top->count() + 1);
            what = QStringLiteral("move %1 to %2").arg(pick).arg(index);
            for (Side* side : {&managed, &legacy}) {
                side->top->clearMaximized();
                side->top->addTerminalDisplay(side->displays.at(pick), index);
            }
        } else {
            what = QStringLiteral("maximize");
            for (Side* side : {&managed, &legacy}) {
                side->top->toggleMaximizeCurrentTerminal();
            }
        }
        settle();
        QVERIFY2(state(managed) == state(legacy),
            qPrintable(QStringLiteral("step %1 (%2):\n  model    %3\n  splitter %4").arg(step).arg(what, state(managed), state(legacy))));
    }

    delete managed.host;
    delete legacy.host;
    settle();
}

QTEST_MAIN(PaneModelTest)

#include "PaneModelTest.moc"
