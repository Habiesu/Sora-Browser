#include <QtTest>
#include <QFile>

#include "privacy/AdblockEngine.h"
#include "privacy/AdblockService.h"
#include "privacy/RequestInterceptor.h"

using RT = QWebEngineUrlRequestInfo::ResourceType;

static std::shared_ptr<const sora::AdblockEngine> smallEngine()
{
    QFile f(qEnvironmentVariable("SORA_TEST_DATA") + "/rules-small.txt");
    if (!f.open(QIODevice::ReadOnly))
        qFatal("no se encuentra rules-small.txt");
    return std::make_shared<const sora::AdblockEngine>(sora::AdblockEngine::fromRules(f.readAll()));
}

class TestRequestInterceptor : public QObject {
    Q_OBJECT
private slots:
    void mapsResourceTypes()
    {
        QCOMPARE(QByteArray(sora::adblockRequestType(RT::ResourceTypeScript)), QByteArray("script"));
        QCOMPARE(QByteArray(sora::adblockRequestType(RT::ResourceTypeSubFrame)), QByteArray("subdocument"));
        QCOMPARE(QByteArray(sora::adblockRequestType(RT::ResourceTypeMainFrame)), QByteArray("document"));
        QCOMPARE(QByteArray(sora::adblockRequestType(RT::ResourceTypeXhr)), QByteArray("xmlhttprequest"));
        QCOMPARE(QByteArray(sora::adblockRequestType(RT::ResourceTypeImage)), QByteArray("image"));
        QCOMPARE(QByteArray(sora::adblockRequestType(RT::ResourceTypeWebSocket)), QByteArray("websocket"));
        QCOMPARE(QByteArray(sora::adblockRequestType(RT::ResourceTypeUnknown)), QByteArray("other"));
    }
    void noEngineMeansNoBlocking()
    {
        sora::AdblockService service;
        QVERIFY(!service.decide(QUrl("https://ads.example/x.js"), QUrl("https://site.test"), RT::ResourceTypeScript));
    }
    void blocksWithEngine()
    {
        sora::AdblockService service;
        service.setEngine(smallEngine());
        QVERIFY(service.decide(QUrl("https://ads.example/x.js"), QUrl("https://site.test"), RT::ResourceTypeScript));
        QVERIFY(!service.decide(QUrl("https://ok.test/x.js"), QUrl("https://site.test"), RT::ResourceTypeScript));
    }
    void neverBlocksMainFrameNavigation()
    {
        sora::AdblockService service;
        service.setEngine(smallEngine());
        QVERIFY(!service.decide(QUrl("https://ads.example/"), QUrl(), RT::ResourceTypeMainFrame));
        QVERIFY(!service.decide(QUrl("https://ads.example/"), QUrl(), RT::ResourceTypeNavigationPreloadMainFrame));
    }
    void engineCanBeRemoved()
    {
        sora::AdblockService service;
        service.setEngine(smallEngine());
        service.setEngine(nullptr);
        QVERIFY(!service.decide(QUrl("https://ads.example/x.js"), QUrl("https://site.test"), RT::ResourceTypeScript));
    }
    void interceptorsShareTheService()
    {
        // Un interceptor por pestaña; el motor llega una sola vez al servicio y lo ven todos.
        sora::AdblockService service;
        sora::RequestInterceptor first(&service);
        sora::RequestInterceptor second(&service);
        QCOMPARE(first.service(), &service);
        QCOMPARE(second.service(), &service);
        service.setEngine(smallEngine());
        QVERIFY(first.service()->decide(QUrl("https://ads.example/x.js"), QUrl("https://site.test"), RT::ResourceTypeScript));
    }
};

QTEST_GUILESS_MAIN(TestRequestInterceptor)
#include "tst_requestinterceptor.moc"
