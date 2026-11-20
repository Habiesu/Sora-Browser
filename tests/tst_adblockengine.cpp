#include <QtTest>
#include <QElapsedTimer>
#include <QFile>
#include <atomic>
#include <thread>
#include <vector>

#include "privacy/AdblockEngine.h"

using sora::AdblockEngine;

static QByteArray smallRules()
{
    QFile f(qEnvironmentVariable("SORA_TEST_DATA") + "/rules-small.txt");
    if (!f.open(QIODevice::ReadOnly))
        qFatal("no se encuentra rules-small.txt");
    return f.readAll();
}

class TestAdblockEngine : public QObject {
    Q_OBJECT
private slots:
    void blocksListedDomain()
    {
        const auto e = AdblockEngine::fromRules(smallRules());
        QVERIFY(e.shouldBlock(QUrl("https://ads.example/x.js"), QUrl("https://site.test"), "script"));
    }
    void exceptionIsNotBlocked()
    {
        const auto e = AdblockEngine::fromRules(smallRules());
        QVERIFY(!e.shouldBlock(QUrl("https://ads.example/ok/x.js"), QUrl("https://site.test"), "script"));
    }
    void thirdPartyRuleBlocksOnlyThirdParty()
    {
        const auto e = AdblockEngine::fromRules(smallRules());
        QVERIFY(e.shouldBlock(QUrl("https://tracker.test/p.gif"), QUrl("https://site.test"), "image"));
        QVERIFY(!e.shouldBlock(QUrl("https://tracker.test/p.gif"), QUrl("https://tracker.test"), "image"));
    }
    void serializeRoundtrip()
    {
        const auto e = AdblockEngine::fromRules(smallRules());
        const auto copy = AdblockEngine::deserialize(e.serialize());
        QVERIFY(copy.has_value());
        QVERIFY(copy->shouldBlock(QUrl("https://ads.example/x.js"), QUrl("https://site.test"), "script"));
    }
    void deserializeGarbageFails()
    {
        QVERIFY(!AdblockEngine::deserialize(QByteArray("basura")).has_value());
        QVERIFY(!AdblockEngine::deserialize(QByteArray()).has_value());
    }
    void concurrentChecks()
    {
        const auto e = AdblockEngine::fromRules(smallRules());
        std::atomic<int> wrong{0};
        std::vector<std::thread> threads;
        for (int t = 0; t < 8; ++t) {
            threads.emplace_back([&] {
                const QUrl blocked("https://ads.example/x.js"), allowed("https://ok.test/x.js"), src("https://site.test");
                for (int i = 0; i < 10000; ++i) {
                    if (!e.shouldBlock(blocked, src, "script") || e.shouldBlock(allowed, src, "script"))
                        ++wrong;
                }
            });
        }
        for (auto& th : threads)
            th.join();
        QCOMPARE(wrong.load(), 0);
    }
    void perCheckCostUnder50Microseconds()
    {
        QByteArray rules;
        for (int i = 0; i < 1000; ++i)
            rules += "||domain" + QByteArray::number(i) + ".example^\n";
        for (int i = 0; i < 1000; ++i)
            rules += "/adpath" + QByteArray::number(i) + "/*$script\n";
        const auto e = AdblockEngine::fromRules(rules);
        const QUrl src("https://news.site.test/article");
        QElapsedTimer timer;
        timer.start();
        const int n = 10000;
        int blocked = 0;
        for (int i = 0; i < n; ++i) {
            const QUrl url(QString("https://cdn%1.site.test/assets/adpath%2/file.js").arg(i % 50).arg(i % 1500));
            blocked += e.shouldBlock(url, src, "script");
        }
        const double usPerCheck = timer.nsecsElapsed() / 1000.0 / n;
        qInfo("coste medio por comprobación: %.2f µs (%d bloqueadas)", usPerCheck, blocked);
        QVERIFY(blocked > 0);
        QVERIFY2(usPerCheck < 50.0, qPrintable(QString::number(usPerCheck)));
    }
};

QTEST_GUILESS_MAIN(TestAdblockEngine)
#include "tst_adblockengine.moc"
