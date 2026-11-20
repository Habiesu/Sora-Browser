#include <QtTest>
#include <QFile>

#include "privacy/AdblockEngine.h"
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
        sora::RequestInterceptor interceptor;
        QVERIFY(!interceptor.decide(QUrl("https://ads.example/x.js"), QUrl("https://site.test"), RT::ResourceTypeScript));
    }
    void blocksWithEngine()
    {
        sora::RequestInterceptor interceptor;
        interceptor.setEngine(smallEngine());
        QVERIFY(interceptor.decide(QUrl("https://ads.example/x.js"), QUrl("https://site.test"), RT::ResourceTypeScript));
        QVERIFY(!interceptor.decide(QUrl("https://ok.test/x.js"), QUrl("https://site.test"), RT::ResourceTypeScript));
    }
    void neverBlocksMainFrameNavigation()
    {
        sora::RequestInterceptor interceptor;
        interceptor.setEngine(smallEngine());
        QVERIFY(!interceptor.decide(QUrl("https://ads.example/"), QUrl(), RT::ResourceTypeMainFrame));
    }
    void engineCanBeRemoved()
    {
        sora::RequestInterceptor interceptor;
        interceptor.setEngine(smallEngine());
        interceptor.setEngine(nullptr);
        QVERIFY(!interceptor.decide(QUrl("https://ads.example/x.js"), QUrl("https://site.test"), RT::ResourceTypeScript));
    }
};

QTEST_GUILESS_MAIN(TestRequestInterceptor)
#include "tst_requestinterceptor.moc"
