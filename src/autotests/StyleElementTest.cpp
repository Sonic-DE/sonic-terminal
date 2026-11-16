/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Widget styles aren't thread-safe (some start animations for the option's styleObject), so
// StyleElement must call the style on the GUI thread even with a threaded scene graph.

#include "../quick/StyleElement.h"

#include <QApplication>
#include <QAtomicInt>
#include <QProxyStyle>
#include <QQuickWindow>
#include <QTest>
#include <QThread>

using namespace Konsole;

namespace {
class ThreadRecordingStyle : public QProxyStyle {
public:
    QAtomicInt calls;
    QAtomicInt offThreadCalls;

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
        const QWidget* widget = nullptr) const override
    {
        record();
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
        const QWidget* widget = nullptr) const override
    {
        record();
        QProxyStyle::drawControl(element, option, painter, widget);
    }

private:
    void record() const
    {
        auto* self = const_cast<ThreadRecordingStyle*>(this);
        self->calls.ref();
        if (QThread::currentThread() != qApp->thread()) {
            self->offThreadCalls.ref();
        }
    }
};
}

class StyleElementTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testStyleCalledOnGuiThread_data();
    void testStyleCalledOnGuiThread();
};

void StyleElementTest::testStyleCalledOnGuiThread_data()
{
    QTest::addColumn<QString>("element");
    QTest::addColumn<int>("shape");
    QTest::newRow("lineedit") << QStringLiteral("lineedit") << 0;
    QTest::newRow("splitter") << QStringLiteral("splitter") << int(Qt::Horizontal);
    QTest::newRow("tabbarbase") << QStringLiteral("tabbarbase") << 1;
}

void StyleElementTest::testStyleCalledOnGuiThread()
{
    QFETCH(QString, element);
    QFETCH(int, shape);

    auto* style = new ThreadRecordingStyle;
    QApplication::setStyle(style);

    QQuickWindow window;
    window.resize(200, 100);
    QAtomicPointer<QThread> renderThread;
    connect(&window, &QQuickWindow::beforeRendering, &window, [&renderThread] { renderThread.storeRelaxed(QThread::currentThread()); }, Qt::DirectConnection);

    auto* item = new StyleElement(window.contentItem());
    item->setElement(element);
    item->setShape(shape);
    item->setStylePalette(QApplication::palette());
    item->setSize(QSizeF(180, 30));
    window.show();

    QTRY_VERIFY(style->calls.loadRelaxed() > 0);
    QTRY_VERIFY(renderThread.loadRelaxed() != nullptr);
    QVERIFY2(renderThread.loadRelaxed() != qApp->thread(), "the scene graph must render on its own thread");

    // A change draws the element again.
    const int before = style->calls.loadRelaxed();
    item->setHover(true);
    QTRY_VERIFY(style->calls.loadRelaxed() > before);

    QCOMPARE(style->offThreadCalls.loadRelaxed(), 0);

    // What the style drew reaches the window.
    window.setColor(Qt::magenta);
    const QImage shot = window.grabWindow();
    const QRect area = item->mapRectToScene(item->boundingRect()).toRect();
    bool drawn = false;
    for (int y = area.top(); y <= area.bottom() && !drawn; ++y) {
        for (int x = area.left(); x <= area.right() && !drawn; ++x) {
            drawn = shot.pixel(x, y) != QColor(Qt::magenta).rgb();
        }
    }
    QVERIFY(drawn);
}

int main(int argc, char** argv)
{
    // The threaded render loop, as on a desktop with OpenGL; the software one runs anywhere.
    qputenv("QSG_RENDER_LOOP", "threaded");
    qputenv("QT_QUICK_BACKEND", "software");
    QApplication app(argc, argv);
    StyleElementTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "StyleElementTest.moc"
