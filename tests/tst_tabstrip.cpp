#include <QtTest>
#include <QToolButton>
#include "ui/TabStrip.h"
#include "ui/Theme.h"

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
    void closeButtonBlendsWithTab_data()
    {
        QTest::addColumn<Qt::ColorScheme>("scheme");
        QTest::newRow("claro") << Qt::ColorScheme::Light;
        QTest::newRow("oscuro") << Qt::ColorScheme::Dark;
    }
    void closeButtonBlendsWithTab()
    {
        // La × no debe llevar un cuadro de otro color: alrededor del aspa se ve la pestaña.
        QFETCH(Qt::ColorScheme, scheme);
        const sora::Palette p = sora::nieblaPalette(scheme);
        QWidget host;
        host.setStyleSheet(sora::nieblaStyleSheet(p));
        TabStrip bar(&host);
        bar.addTab("A");
        bar.addTab("B");
        host.resize(1200, 40);
        bar.resize(1200, 40);
        host.show();
        const QImage img = bar.grab().toImage();
        for (int i : {0, 1}) {
            const QWidget* close = bar.tabButton(i, QTabBar::RightSide);
            const QPoint corner = close->geometry().topLeft() + QPoint(1, 1);
            const QColor tab = i == bar.currentIndex() ? p.surface : p.window;
            QCOMPARE(QColor(img.pixel(corner * img.devicePixelRatio())).name(), tab.name());
        }
    }
    void plusDoesNotCoverTabsOrArrows_data()
    {
        QTest::addColumn<bool>("styled");
        QTest::newRow("sin hoja de estilos") << false;
        QTest::newRow("con la hoja Niebla") << true;
    }
    void plusDoesNotCoverTabsOrArrows()
    {
        QFETCH(bool, styled);
        QWidget host;
        if (styled)
            host.setStyleSheet(sora::nieblaStyleSheet(sora::nieblaPalette(Qt::ColorScheme::Light)));
        TabStrip bar(&host);
        host.resize(1200, 40);
        bar.resize(1200, 40);
        host.show();
        for (int i = 0; i < 30; ++i)
            bar.addTab("x");
        bar.setCurrentIndex(29); // desplaza hasta el final, como al abrir una pestaña nueva
        const QRect plus = plusOf(bar)->geometry();
        for (int i = 0; i < bar.count(); ++i)
            QVERIFY2(!bar.tabRect(i).intersects(plus), qPrintable(QString("tapa la pestaña %1").arg(i)));
        for (auto* b : bar.findChildren<QToolButton*>(Qt::FindDirectChildrenOnly))
            if (b->isVisible() && b != plusOf(bar) && b->objectName() != "TabCloseButton")
                QVERIFY2(!b->geometry().intersects(plus), "tapa una flecha");
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
