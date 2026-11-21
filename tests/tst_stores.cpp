#include <QtTest>
#include <QTemporaryDir>

#include "data/BookmarkStore.h"
#include "data/HistoryStore.h"

using sora::BookmarkStore;
using sora::HistoryStore;

class TestStores : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;
    int m_n = 0;
    QString freshDb() { return m_dir.filePath(QStringLiteral("t%1.db").arg(++m_n)); }

private slots:
    void historyCountsVisits()
    {
        HistoryStore h(freshDb());
        h.addVisit(QUrl("https://a.test/"), "A");
        h.addVisit(QUrl("https://a.test/"), "A nuevo");
        h.addVisit(QUrl("https://b.test/"), "B");
        const auto all = h.search({});
        QCOMPARE(all.size(), 2);
        QCOMPARE(all.first().url, QUrl("https://b.test/")); // lo más reciente primero
        const auto a = h.search("a.test").first();
        QCOMPARE(a.visits, 2);
        QCOMPARE(a.title, QString("A nuevo"));
    }
    void historySearchesTitleAndUrlIgnoringCase()
    {
        HistoryStore h(freshDb());
        h.addVisit(QUrl("https://lobos.test/"), "Manada de LOBOS");
        h.addVisit(QUrl("https://pinos.test/bosque"), "Pinares");
        QCOMPARE(h.search("lobos").size(), 1);
        QCOMPARE(h.search("BOSQUE").size(), 1);
        QCOMPARE(h.search("zorro").size(), 0);
    }
    void historyRemoveAndClear()
    {
        HistoryStore h(freshDb());
        h.addVisit(QUrl("https://a.test/"), "A");
        h.addVisit(QUrl("https://b.test/"), "B");
        h.remove(QUrl("https://a.test/"));
        QCOMPARE(h.search({}).size(), 1);
        h.clear();
        QCOMPARE(h.search({}).size(), 0);
    }
    void historyTopSitesByVisits()
    {
        HistoryStore h(freshDb());
        h.addVisit(QUrl("https://poco.test/"), "Poco");
        for (int i = 0; i < 3; ++i)
            h.addVisit(QUrl("https://mucho.test/"), "Mucho");
        QCOMPARE(h.topSites(1).first().url, QUrl("https://mucho.test/"));
    }
    void historyKeepsDataBetweenSessions()
    {
        const QString db = freshDb();
        { HistoryStore h(db); h.addVisit(QUrl("https://a.test/"), "A"); }
        HistoryStore again(db);
        QCOMPARE(again.search({}).size(), 1);
    }
    void bookmarksAddContainRemove()
    {
        BookmarkStore b(freshDb());
        QSignalSpy changed(&b, &BookmarkStore::changed);
        const qint64 id = b.add(QUrl("https://a.test/"), "A");
        QVERIFY(b.contains(QUrl("https://a.test/")));
        QCOMPARE(b.all().size(), 1);
        QCOMPARE(b.all().first().id, id);
        b.removeUrl(QUrl("https://a.test/"));
        QVERIFY(!b.contains(QUrl("https://a.test/")));
        QCOMPARE(changed.count(), 2);
    }
    void bookmarksRenameAndMove()
    {
        BookmarkStore b(freshDb());
        const qint64 a = b.add(QUrl("https://a.test/"), "A");
        const qint64 c = b.add(QUrl("https://c.test/"), "C");
        b.add(QUrl("https://b.test/"), "B");
        b.rename(a, "Alfa");
        b.move(c, 2); // al final
        QStringList titles;
        for (const auto& bm : b.all())
            titles << bm.title;
        QCOMPARE(titles, QStringList({"Alfa", "B", "C"}));
    }
    void bothStoresShareOneFile()
    {
        const QString db = freshDb();
        HistoryStore h(db);
        BookmarkStore b(db);
        h.addVisit(QUrl("https://a.test/"), "A");
        b.add(QUrl("https://a.test/"), "A");
        QCOMPARE(h.search({}).size(), 1);
        QCOMPARE(b.all().size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestStores)
#include "tst_stores.moc"
