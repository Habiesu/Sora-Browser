#include <QtTest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineExtensionManager>

#include "app/InternalPages.h"
#include "browser/Tab.h"
#include "app/InternalApi.h"
#include "app/MainWindow.h"
#include "app/Settings.h"
#include "browser/DownloadManager.h"
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
    void downloadsAreSavedAndListed()
    {
        QWebEngineProfile profile(QStringLiteral("descargas"));
        profile.setPersistentStoragePath(m_storage.filePath("perfil-descargas"));
        profile.setDownloadPath(m_storage.filePath("bajadas"));
        sora::installInternalPages(&profile);
        sora::DownloadManager downloads;
        downloads.attach(&profile);
        downloads.openHistory(m_storage.filePath("descargas.db"));
        sora::HistoryStore history(m_storage.filePath("p4.db"));
        sora::BookmarkStore bookmarks(m_storage.filePath("p4.db"));
        sora::InternalApi api(&history, &bookmarks, &downloads);
        sora::setInternalApi(&api);

        sora::Page web(&profile);
        web.download(QUrl("data:text/plain,hola%20lobo"), QStringLiteral("lobo.txt"));
        QTRY_COMPARE_WITH_TIMEOUT(api.downloads().value(0).toMap().value("state").toString(), QString("done"), 8000);
        QFile file(m_storage.filePath("bajadas/lobo.txt"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("hola lobo"));

        sora::Page page(&profile);
        page.setUrl(QUrl("sora://downloads"));
        QTRY_VERIFY_WITH_TIMEOUT(runJs(page, "document.body.innerText").toString().contains("lobo.txt"), 8000);

        // Al volver a abrir Sora la lista sigue ahí (como en Chrome).
        sora::DownloadManager later;
        later.openHistory(m_storage.filePath("descargas.db"));
        QCOMPARE(later.list().size(), 1);
        QCOMPARE(later.list().first().toMap().value("name").toString(), QString("lobo.txt"));
        QCOMPARE(later.list().first().toMap().value("state").toString(), QString("done"));
        file.close();
        QVERIFY(QFile::remove(m_storage.filePath("bajadas/lobo.txt")));
        QCOMPARE(later.list().first().toMap().value("state").toString(), QString("removed"));
        later.clearFinished();
        QCOMPARE(later.list().size(), 0);
        sora::DownloadManager empty;
        empty.openHistory(m_storage.filePath("descargas.db"));
        QCOMPARE(empty.list().size(), 0);
    }
    void settingsPageChangesSearchEngine()
    {
        sora::Settings& settings = sora::Settings::instance();
        settings.setValue("search", "duckduckgo");
        sora::HistoryStore history(m_storage.filePath("p5.db"));
        sora::BookmarkStore bookmarks(m_storage.filePath("p5.db"));
        sora::InternalApi api(&history, &bookmarks);
        sora::setInternalApi(&api);
        sora::Page page(m_profile);
        page.setUrl(QUrl("sora://settings"));
        QTRY_COMPARE_WITH_TIMEOUT(runJs(page, "document.getElementById('search').value").toString(),
                                  QString("duckduckgo"), 8000);
        runJs(page, "const s = document.getElementById('search'); s.value = 'brave';"
                    "s.dispatchEvent(new Event('change'))");
        QTRY_COMPARE(settings.value("search").toString(), QString("brave"));
        QCOMPARE(sora::urlFromInput("lobo gris").host(), QString("search.brave.com"));

        QWebEnginePage newTab(m_profile);
        newTab.setUrl(QUrl("sora://newtab"));
        QTRY_COMPARE_WITH_TIMEOUT(runJs(newTab, "document.querySelector('form') && document.querySelector('form').action").toString(),
                                  QString("https://search.brave.com/search"), 8000);
        settings.setValue("search", "duckduckgo");
    }
    void settingsRejectBadValues()
    {
        // Lo que llega de una página no puede meter valores raros ni cambiar la carpeta de descargas.
        sora::InternalApi api(nullptr, nullptr);
        QVERIFY(!api.setSetting("search", "malvado"));
        QVERIFY(!api.setSetting("homepage", "javascript:alert(1)"));
        QVERIFY(!api.setSetting("downloadPath", m_storage.path()));
        QVERIFY(!api.setSetting("otra", 1));
        QVERIFY(api.setSetting("homepage", "https://lobos.test/"));
        QCOMPARE(sora::Settings::instance().value("homepage").toString(), QString("https://lobos.test/"));
        QVERIFY(api.setSetting("homepage", ""));
    }
    void extensionsPageListsAndDisables()
    {
        QWebEngineProfile profile(QStringLiteral("extensiones"));
        profile.setPersistentStoragePath(m_storage.filePath("perfil-ext"));
        sora::installInternalPages(&profile);
        sora::InternalApi api(nullptr, nullptr);
        api.setProfiles({&profile});
        sora::setInternalApi(&api);

        QDir(m_storage.path()).mkpath("lobo-ext");
        QFile manifest(m_storage.filePath("lobo-ext/manifest.json"));
        QVERIFY(manifest.open(QIODevice::WriteOnly));
        manifest.write(R"({"manifest_version":3,"name":"Aullido","version":"1.0","description":"Prueba"})");
        manifest.close();
        profile.extensionManager()->loadExtension(m_storage.filePath("lobo-ext"));
        QTRY_COMPARE_WITH_TIMEOUT(api.extensions().size(), 1, 8000);

        sora::Page page(&profile);
        page.setUrl(QUrl("sora://extensions"));
        QTRY_VERIFY_WITH_TIMEOUT(runJs(page, "document.body.innerText").toString().contains("Aullido"), 8000);

        const QString id = api.extensions().first().toMap().value("id").toString();
        api.setExtensionEnabled(id, false);
        QTRY_COMPARE(api.extensions().first().toMap().value("enabled").toBool(), false);
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
        QCOMPARE(sora::displayUrl(QUrl("sora://history/")), QString("sora://history"));
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
