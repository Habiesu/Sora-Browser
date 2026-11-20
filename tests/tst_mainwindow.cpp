#include <QtTest>
#include <QUrlQuery>
#include <QToolButton>
#include "app/MainWindow.h"
#include "ui/Omnibox.h"
#include "ui/Toolbar.h"

class TestMainWindow : public QObject {
    Q_OBJECT
private slots:
    void bareDomainBecomesHttps() { QCOMPARE(sora::urlFromInput("example.com"), QUrl("https://example.com")); }
    void explicitSchemeIsKept() { QCOMPARE(sora::urlFromInput("http://a.b/c"), QUrl("http://a.b/c")); }
    void textBecomesSearch() {
        const QUrl u = sora::urlFromInput("hola mundo");
        QCOMPARE(u.host(), QString("duckduckgo.com"));
        QCOMPARE(QUrlQuery(u).queryItemValue("q"), QString("hola mundo"));
    }
    void localhostUsesHttp() { QCOMPARE(sora::urlFromInput("localhost:8080"), QUrl("http://localhost:8080")); }
    void surroundingSpacesIgnored() { QCOMPARE(sora::urlFromInput("  example.com  "), QUrl("https://example.com")); }

    void displayUrlHidesHttpsAndWww()
    {
        QCOMPARE(sora::displayUrl(QUrl("https://www.example.com/a?b")), QString("example.com/a?b"));
        QCOMPARE(sora::displayUrl(QUrl("https://example.com/")), QString("example.com"));
        QCOMPARE(sora::displayUrl(QUrl("http://x.test/")), QString("http://x.test/"));
        QCOMPARE(sora::displayUrl(QUrl("about:blank")), QString());
    }
    void omniboxKeepsWhatUserTypes()
    {
        sora::Omnibox box;
        box.show();
        box.activateWindow();
        box.setFocus();
        QTRY_VERIFY(box.hasFocus());
        QTest::keyClicks(&box, "hola");
        box.setUrl(QUrl("https://x.test"));
        QCOMPARE(box.text(), QString("hola"));
        box.clearFocus();
        box.setUrl(QUrl("https://x.test"));
        QCOMPARE(box.text(), QString("x.test"));
    }
    void escapeRestoresUrl()
    {
        sora::Omnibox box;
        box.setUrl(QUrl("https://x.test"));
        box.show();
        box.activateWindow();
        box.setFocus();
        QTRY_VERIFY(box.hasFocus());
        box.selectAll();
        QTest::keyClicks(&box, "otra cosa");
        QTest::keyClick(&box, Qt::Key_Escape);
        QCOMPARE(box.text(), QString("x.test"));
    }
    void submitEmitsText()
    {
        sora::Omnibox box;
        QSignalSpy spy(&box, &sora::Omnibox::submitted);
        box.show();
        box.setFocus();
        QTest::keyClicks(&box, "hola");
        QTest::keyClick(&box, Qt::Key_Return);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QString("hola"));
    }
    void shieldShowsCount()
    {
        sora::Toolbar bar;
        bar.setBlockedCount(14);
        auto* shield = bar.findChild<QToolButton*>("ShieldButton");
        QVERIFY(shield);
        QVERIFY(shield->text().contains("14"));
        QVERIFY(shield->toolTip().contains("14"));
    }
    void reloadBecomesStop()
    {
        sora::Toolbar bar;
        bar.show();
        QSignalSpy stop(&bar, &sora::Toolbar::stopRequested);
        QSignalSpy reload(&bar, &sora::Toolbar::reloadRequested);
        auto* button = bar.findChild<QToolButton*>("ReloadButton");
        QVERIFY(button);
        bar.setNavigation(false, false, true);
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(stop.count(), 1);
        bar.setNavigation(false, false, false);
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(reload.count(), 1);
    }
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
