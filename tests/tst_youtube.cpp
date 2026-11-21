#include <QtTest>
#include <QWebEnginePage>
#include <QWebEngineProfile>

#include "privacy/AdblockEngine.h"
#include "privacy/AdblockService.h"

// El script de YouTube quita los anuncios de las respuestas del reproductor, solo en youtube.com.
class TestYouTube : public QObject {
    Q_OBJECT
    static QString titleAfter(QWebEngineProfile* profile, const QUrl& base)
    {
        QWebEnginePage page(profile);
        QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
        page.setHtml(QStringLiteral(
                         "<script>window.ytInitialPlayerResponse = {adPlacements: [1], videoDetails: {}};"
                         "const r = JSON.parse('{\"playerAds\":[1],\"streamingData\":{}}');"
                         "document.title = Object.keys(ytInitialPlayerResponse).concat(Object.keys(r)).join(',');"
                         "</script>"),
                     base);
        if (!loaded.wait(10000))
            return {};
        return page.title();
    }

private slots:
    void prunesAdsOnYouTubeOnly()
    {
        QWebEngineProfile profile; // sin nombre: no toca disco
        sora::AdblockService adblock;
        adblock.attach(&profile);
        QCOMPARE(titleAfter(&profile, QUrl("https://www.youtube.com/watch?v=lobo")), QString("videoDetails,streamingData"));
        QCOMPARE(titleAfter(&profile, QUrl("https://ejemplo.test/")),
                 QString("adPlacements,videoDetails,playerAds,streamingData"));

        adblock.setEnabled(false); // bloqueador apagado en Ajustes: YouTube tal cual
        QCOMPARE(titleAfter(&profile, QUrl("https://www.youtube.com/watch?v=lobo")),
                 QString("adPlacements,videoDetails,playerAds,streamingData"));
    }
    void sitesCanBeAllowed()
    {
        // Desactivar el escudo en un sitio (como en Brave): nada se bloquea en él, ni en sus subdominios.
        sora::AdblockService adblock;
        adblock.setEngine(std::make_shared<const sora::AdblockEngine>(sora::AdblockEngine::fromRules("||ads.test^\n")));
        const auto script = QWebEngineUrlRequestInfo::ResourceTypeScript;
        QVERIFY(adblock.decide(QUrl("https://ads.test/a.js"), QUrl("https://www.lobos.test/"), script));
        adblock.setAllowedSites({"lobos.test"});
        QVERIFY(!adblock.decide(QUrl("https://ads.test/a.js"), QUrl("https://www.lobos.test/"), script));
        QVERIFY(!adblock.decide(QUrl("https://ads.test/a.js"), QUrl("https://blog.lobos.test/"), script));
        QVERIFY(adblock.decide(QUrl("https://ads.test/a.js"), QUrl("https://otro.test/"), script));

        QWebEngineProfile profile;
        adblock.attach(&profile);
        adblock.setAllowedSites({"youtube.com"});
        QCOMPARE(titleAfter(&profile, QUrl("https://www.youtube.com/watch?v=lobo")),
                 QString("adPlacements,videoDetails,playerAds,streamingData"));
    }
    void siteKeyDropsWww()
    {
        QCOMPARE(sora::AdblockService::siteOf(QUrl("https://www.youtube.com/watch")), QString("youtube.com"));
        QCOMPARE(sora::AdblockService::siteOf(QUrl("https://m.lobos.test/")), QString("m.lobos.test"));
    }
};

QTEST_MAIN(TestYouTube)
#include "tst_youtube.moc"
