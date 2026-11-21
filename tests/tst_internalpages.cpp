#include <QtTest>
#include <QWebEnginePage>
#include <QWebEngineProfile>

#include "app/InternalPages.h"
#include "browser/Tab.h"
#include "app/InternalApi.h"
#include "data/BookmarkStore.h"
#include "data/HistoryStore.h"
#include <QTemporaryDir>
#include "ui/Omnibox.h"

class TestInternalPages : public QObject {
    Q_OBJECT
    QWebEngineProfile* m_profile = nullptr;
    QTemporaryDir m_storage;

    static QVariant runJs(QWebEnginePage& page, const QString& js)
    {
        QVariant result;
        bool done = false;
        page.runJavaScript(js, [&](const QVariant& v) { result = v; done = true; });
        QDeadlineTimer deadline(5000);
        while (!done && !deadline.hasExpired())
            QTest::qWait(20);
        return result;
    }

private slots:
    void initTestCase()
    {
        m_profile = new QWebEngineProfile(this);
        sora::installInternalPages(m_profile);
    }
    void newTabPageLoads()
    {
        QWebEnginePage page(m_profile);
        QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
        page.setUrl(QUrl("sora://newtab"));
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(loaded.first().at(0).toBool(), true);
        QCOMPARE(page.title(), QString::fromUtf8("Nueva pestaña"));
        QVERIFY(runJs(page, "document.querySelector('input[type=search]') !== null").toBool());
    }
    void unknownPageFails()
    {
        QWebEnginePage page(m_profile);
        QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
        page.setUrl(QUrl("sora://no-existe"));
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(loaded.first().at(0).toBool(), false);
    }
    void webPagesCannotOpenInternalPages()
    {
        // Una web normal no puede navegar a sora:// (ni incrustarla) para leer o tocar ajustes.
        sora::Page page(m_profile); // la página real de las pestañas de Sora
        QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
        page.setHtml("<p>web</p>", QUrl("https://ataque.test/"));
        QTRY_COMPARE(loaded.count(), 1);
        runJs(page, "location.href = 'sora://settings'");
        QTest::qWait(800);
        QCOMPARE(page.url().scheme(), QString("https"));
    }
    void historyPageListsAndDeletes()
    {
        sora::HistoryStore history(m_storage.filePath("p1.db"));
        sora::BookmarkStore bookmarks(m_storage.filePath("p1.db"));
        sora::InternalApi api(&history, &bookmarks);
        sora::setInternalApi(&api);
        history.addVisit(QUrl("https://lobos.test/"), "Manada de lobos");
        sora::Page page(m_profile);
        page.setUrl(QUrl("sora://history"));
        QTRY_VERIFY_WITH_TIMEOUT(runJs(page, "document.body.innerText").toString().contains("Manada de lobos"), 8000);
        runJs(page, "document.querySelector('[data-remove]').click()");
        QTRY_COMPARE(history.search({}).size(), 0);
    }
    void bookmarksPageListsAndRenames()
    {
        sora::HistoryStore history(m_storage.filePath("p2.db"));
        sora::BookmarkStore bookmarks(m_storage.filePath("p2.db"));
        sora::InternalApi api(&history, &bookmarks);
        sora::setInternalApi(&api);
        const qint64 id = bookmarks.add(QUrl("https://pino.test/"), "Pino");
        sora::Page page(m_profile);
        page.setUrl(QUrl("sora://bookmarks"));
        // El nombre va en un <input> editable: innerText no incluye su valor.
        QTRY_COMPARE_WITH_TIMEOUT(runJs(page, "(document.querySelector('input.name') || {}).value").toString(),
                                  QString("Pino"), 8000);
        runJs(page, QStringLiteral("sora.renameBookmark(%1, 'Pino verde')").arg(id));
        QTRY_COMPARE(bookmarks.all().first().title, QString("Pino verde"));
    }
    void webPagesDoNotGetTheBridge()
    {
        sora::HistoryStore history(m_storage.filePath("p3.db"));
        sora::BookmarkStore bookmarks(m_storage.filePath("p3.db"));
        sora::InternalApi api(&history, &bookmarks);
        sora::setInternalApi(&api);
        sora::Page page(m_profile);
        QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
        page.setUrl(QUrl("data:text/html,<p>web</p>"));
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(runJs(page, "typeof qt === 'undefined'").toBool(), true);
    }
    void newTabShowsEmptyAddress()
    {
        QCOMPARE(sora::displayUrl(QUrl("sora://newtab")), QString());
        QCOMPARE(sora::displayUrl(QUrl("sora://history")), QString("sora://history"));
    }
};

int main(int argc, char** argv)
{
    sora::registerSoraScheme(); // antes de crear la aplicación, como en main.cpp
    QApplication app(argc, argv);
    TestInternalPages t;
    return QTest::qExec(&t, argc, argv);
}
#include "tst_internalpages.moc"
