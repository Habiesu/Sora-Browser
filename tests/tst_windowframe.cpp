#include <QtTest>
#include <QToolButton>
#include "ui/TabStrip.h"
#include "ui/TitleBar.h"
#include "ui/WindowFrame.h"

using sora::WindowHit;
using sora::windowHitTest;

class TestWindowFrame : public QObject {
    Q_OBJECT
private slots:
    void edgesWinWhenNormal()
    {
        const QSize size(800, 600);
        const auto hit = [&](QPoint p) { return windowHitTest(p, size, 6, false, WindowHit::Caption); };
        QCOMPARE(hit({0, 300}), WindowHit::Left);
        QCOMPARE(hit({799, 300}), WindowHit::Right);
        QCOMPARE(hit({400, 0}), WindowHit::Top);
        QCOMPARE(hit({400, 599}), WindowHit::Bottom);
        QCOMPARE(hit({2, 2}), WindowHit::TopLeft);
        QCOMPARE(hit({797, 2}), WindowHit::TopRight);
        QCOMPARE(hit({2, 597}), WindowHit::BottomLeft);
        QCOMPARE(hit({797, 597}), WindowHit::BottomRight);
        QCOMPARE(hit({400, 20}), WindowHit::Caption);
    }
    void noEdgesWhenMaximized()
    {
        const QSize size(800, 600);
        QCOMPARE(windowHitTest({0, 300}, size, 6, true, WindowHit::Client), WindowHit::Client);
        QCOMPARE(windowHitTest({400, 0}, size, 6, true, WindowHit::Caption), WindowHit::Caption);
    }
    void titleBarRegions()
    {
        sora::TitleBar bar;
        bar.tabs()->addTab("A");
        bar.tabs()->addTab("B");
        bar.resize(1000, 40);
        bar.show();
        QCOMPARE(bar.hitAt(bar.closeButton()->geometry().center()), WindowHit::Close);
        QCOMPARE(bar.hitAt(bar.maximizeButton()->geometry().center()), WindowHit::Maximize);
        QCOMPARE(bar.hitAt(bar.minimizeButton()->geometry().center()), WindowHit::Minimize);
        const QPoint tab = bar.tabs()->mapTo(&bar, bar.tabs()->tabRect(0).center());
        QCOMPARE(bar.hitAt(tab), WindowHit::Client);
        const auto* plus = bar.tabs()->findChild<QToolButton*>("NewTabButton");
        const QPoint plusPoint = plus->mapTo(&bar, plus->rect().center());
        QCOMPARE(bar.hitAt(plusPoint), WindowHit::Client);
        const int emptyX = (plusPoint.x() + bar.minimizeButton()->x()) / 2;
        QCOMPARE(bar.hitAt({emptyX, 20}), WindowHit::Caption);
    }
    void buttonsSignal()
    {
        sora::TitleBar bar;
        bar.resize(1000, 40);
        bar.show();
        QSignalSpy min(&bar, &sora::TitleBar::minimizeRequested);
        QSignalSpy max(&bar, &sora::TitleBar::maximizeToggleRequested);
        QSignalSpy close(&bar, &sora::TitleBar::closeRequested);
        QTest::mouseClick(bar.minimizeButton(), Qt::LeftButton);
        QTest::mouseClick(bar.maximizeButton(), Qt::LeftButton);
        QTest::mouseClick(bar.closeButton(), Qt::LeftButton);
        QCOMPARE(min.count(), 1);
        QCOMPARE(max.count(), 1);
        QCOMPARE(close.count(), 1);
    }
};

QTEST_MAIN(TestWindowFrame)
#include "tst_windowframe.moc"
