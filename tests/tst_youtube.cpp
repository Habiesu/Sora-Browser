#include <QtTest>
#include <QWebEnginePage>
#include <QWebEngineProfile>

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
};

QTEST_MAIN(TestYouTube)
#include "tst_youtube.moc"
