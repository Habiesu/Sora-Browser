#include <QtTest>
#include <QToolButton>
#include "ui/TabStrip.h"

using sora::TabStrip;

static QToolButton* plusOf(TabStrip& bar) { return bar.findChild<QToolButton*>("NewTabButton"); }

class TestTabStrip : public QObject {
    Q_OBJECT
private slots:
    void plusRequestsNewTab()
    {
        TabStrip bar;
        bar.addTab("A");
        bar.resize(1200, 40);
        bar.show();
        QSignalSpy spy(&bar, &TabStrip::newTabRequested);
        QVERIFY(plusOf(bar));
        QTest::mouseClick(plusOf(bar), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }
    void middleClickCloses()
    {
        TabStrip bar;
        bar.addTab("A"); bar.addTab("B");
        bar.resize(1200, 40);
        bar.show();
        QSignalSpy spy(&bar, &QTabBar::tabCloseRequested);
        QTest::mouseClick(&bar, Qt::MiddleButton, {}, bar.tabRect(1).center());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 1);
    }
    void closeButtonCloses()
    {
        TabStrip bar;
        bar.addTab("A"); bar.addTab("B");
        bar.resize(1200, 40);
        bar.show();
        QSignalSpy spy(&bar, &QTabBar::tabCloseRequested);
        auto* close = qobject_cast<QToolButton*>(bar.tabButton(1, QTabBar::RightSide));
        QVERIFY(close);
        QTest::mouseClick(close, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 1);
    }
    void widthIsClamped()
    {
        TabStrip bar;
        bar.resize(1200, 40);
        bar.show();
        bar.addTab("A"); bar.addTab("B");
        QCOMPARE(bar.tabRect(0).width(), 220);
        for (int i = 0; i < 28; ++i)
            bar.addTab("x");
        QCOMPARE(bar.tabRect(0).width(), 60);
    }
    void plusStaysVisible()
    {
        TabStrip bar;
        bar.resize(1200, 40);
        bar.show();
        for (int i = 0; i < 30; ++i)
            bar.addTab("x");
        QVERIFY(bar.rect().contains(plusOf(bar)->geometry()));
    }
};

QTEST_MAIN(TestTabStrip)
#include "tst_tabstrip.moc"
