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
    void droppingOutsideDetaches()
    {
        TabStrip bar;
        bar.addTab("A"); bar.addTab("B");
        bar.resize(1200, 40);
        bar.show();
        QSignalSpy spy(&bar, &TabStrip::tabDetachRequested);
        const QPoint start = bar.tabRect(1).center();
        const QPoint outside = start + QPoint(0, 150);
        QTest::mousePress(&bar, Qt::LeftButton, {}, start);
        QTest::mouseMove(&bar, start + QPoint(0, 40));
        QTest::mouseMove(&bar, outside);
        QTest::mouseRelease(&bar, Qt::LeftButton, {}, outside);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 1);
        QCOMPARE(spy.first().at(1).toPoint(), bar.mapToGlobal(outside));
    }
    void clickingDoesNotDetach()
    {
        TabStrip bar;
        bar.addTab("A"); bar.addTab("B");
        bar.resize(1200, 40);
        bar.show();
        QSignalSpy spy(&bar, &TabStrip::tabDetachRequested);
        QTest::mouseClick(&bar, Qt::LeftButton, {}, bar.tabRect(1).center());
        QCOMPARE(spy.count(), 0);
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
        bar.applyPalette(p); // como hace la ventana (TitleBar::applyPalette)
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
    void tabWithoutIconStillHasOne()
    {
        // Sin favicon, Qt centra el título; como en Chrome, va a la izquierda tras un icono genérico.
        TabStrip bar;
        bar.addTab("Nueva pestaña");
        QVERIFY(!bar.tabIcon(0).isNull());
        bar.setPageIcon(0, QIcon()); // la web se queda sin favicon
        QVERIFY(!bar.tabIcon(0).isNull());
    }
    void shortTitleIsLeftAligned()
    {
        // Como Chrome: el título empieza justo tras el icono aunque sea corto (Qt lo centraba).
        const sora::Palette p = sora::nieblaPalette(Qt::ColorScheme::Dark);
        QWidget host;
        host.setStyleSheet(sora::nieblaStyleSheet(p));
        TabStrip bar(&host);
        bar.applyPalette(p);
        bar.addTab("WWWW");
        host.resize(1200, 40);
        bar.resize(1200, 40);
        host.show();
        const QImage img = bar.grab().toImage();
        const qreal dpr = img.devicePixelRatio();
        const QRect tab = bar.tabRect(0);
        const QColor bg = p.surface;
        int firstText = -1;
        for (int x = tab.left() + 36; x < tab.right() - 30 && firstText < 0; ++x)
            for (int y = tab.top() + 12; y < tab.bottom() - 6; ++y) {
                const QColor c(img.pixel(QPoint(x, y) * dpr));
                if (qAbs(c.lightness() - bg.lightness()) > 60) { firstText = x; break; }
            }
        QVERIFY2(firstText >= 0 && firstText < tab.left() + 60,
                 qPrintable(QString("el texto empieza en x=%1 (pestaña desde %2)").arg(firstText).arg(tab.left())));
    }
    void separatorsBetweenInactiveTabs()
    {
        const sora::Palette p = sora::nieblaPalette(Qt::ColorScheme::Dark);
        QWidget host;
        host.setStyleSheet(sora::nieblaStyleSheet(p));
        TabStrip bar(&host);
        bar.applyPalette(p);
        bar.addTab("A"); bar.addTab("B"); bar.addTab("C");
        bar.setCurrentIndex(2);
        host.resize(1200, 40);
        bar.resize(1200, 40);
        host.show();
        const QImage img = bar.grab().toImage();
        const qreal dpr = img.devicePixelRatio();
        const auto colorAt = [&](int x, int y) { return QColor(img.pixel(QPoint(x, y) * dpr)).name(); };
        const int y = bar.tabRect(0).center().y() + 3;
        // Entre A y B (ninguna activa): línea. Entre B y C (C activa): sin línea, como Chrome.
        QCOMPARE(colorAt(bar.tabRect(0).right(), y), p.line.name());
        QVERIFY(colorAt(bar.tabRect(1).right(), y) != p.line.name());
    }
    void closeButtonIsVerticallyCentered()
    {
        const sora::Palette p = sora::nieblaPalette(Qt::ColorScheme::Dark);
        QWidget host;
        host.setStyleSheet(sora::nieblaStyleSheet(p));
        TabStrip bar(&host);
        bar.addTab("A");
        host.resize(1200, 40);
        bar.resize(1200, 40);
        host.show();
        // La pestaña visible empieza 6 px más abajo (margin-top de la hoja de estilos).
        const QRect tab = bar.tabRect(0).adjusted(0, 6, 0, 0);
        const QWidget* close = bar.tabButton(0, QTabBar::RightSide);
        QVERIFY2(qAbs(close->geometry().center().y() - tab.center().y()) <= 1,
                 qPrintable(QString("× en y=%1, centro de la pestaña y=%2").arg(close->geometry().center().y()).arg(tab.center().y())));
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
    void plusIsCenteredAndNearlyTabHigh()
    {
        // Como en Chrome: un círculo centrado en la pestaña visible y casi de su alto.
        QWidget host;
        host.setStyleSheet(sora::nieblaStyleSheet(sora::nieblaPalette(Qt::ColorScheme::Dark)));
        TabStrip bar(&host);
        bar.addTab("A");
        host.resize(1200, 40);
        bar.resize(1200, 40);
        host.show();
        const QRect tab = bar.tabRect(0).adjusted(0, 6, 0, 0);
        const QRect plus = plusOf(bar)->geometry();
        QVERIFY2(qAbs(plus.center().y() - tab.center().y()) <= 1,
                 qPrintable(QString("+ en y=%1, pestaña en y=%2").arg(plus.center().y()).arg(tab.center().y())));
        QVERIFY(plus.height() >= tab.height() - 4);
        QCOMPARE(plus.width(), plus.height());
    }
    void plusFollowsDraggedTab()
    {
        TabStrip bar;
        bar.addTab("A"); bar.addTab("B");
        bar.resize(1200, 40);
        bar.show();
        const int restingX = plusOf(bar)->x();
        const QPoint start = bar.tabRect(1).center();
        QTest::mousePress(&bar, Qt::LeftButton, {}, start);
        QTest::mouseMove(&bar, start + QPoint(40, 0));
        QTest::mouseMove(&bar, start + QPoint(200, 0));
        QVERIFY2(plusOf(bar)->x() >= restingX + 150,
                 qPrintable(QString("+ en x=%1, antes en x=%2").arg(plusOf(bar)->x()).arg(restingX)));
        QTest::mouseRelease(&bar, Qt::LeftButton, {}, start + QPoint(200, 0));
        QTRY_COMPARE(plusOf(bar)->x(), restingX); // al soltar vuelve tras la última pestaña
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
