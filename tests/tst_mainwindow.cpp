#include <QtTest>
#include <QUrlQuery>
#include "app/MainWindow.h"

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
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
