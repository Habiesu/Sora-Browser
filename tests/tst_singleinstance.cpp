#include <QtTest>
#include <QUuid>
#include <future>
#include "app/SingleInstance.h"

using sora::SingleInstance;

static QString uniqueKey() { return "sora-test-" + QUuid::createUuid().toString(QUuid::Id128); }

class TestSingleInstance : public QObject {
    Q_OBJECT
private slots:
    void nothingRunningMeansStartNormally()
    {
        SingleInstance alone(uniqueKey());
        QVERIFY(!alone.sendToRunning("https://a.test"));
    }
    void secondInstanceForwardsItsUrl()
    {
        const QString key = uniqueKey();
        SingleInstance first(key);
        QVERIFY(first.listen());
        QSignalSpy received(&first, &SingleInstance::received);
        // El otro Sora es otro proceso: aquí, otro hilo que bloquea mientras este atiende.
        auto sent = std::async(std::launch::async, [key] { return SingleInstance(key).sendToRunning("https://a.test/ruta con espacios"); });
        QTRY_COMPARE(received.count(), 1);
        QVERIFY(sent.get());
        QCOMPARE(received.first().at(0).toString(), QString("https://a.test/ruta con espacios"));
    }
    void emptyMessageMeansJustOpen()
    {
        const QString key = uniqueKey();
        SingleInstance first(key);
        QVERIFY(first.listen());
        QSignalSpy received(&first, &SingleInstance::received);
        auto sent = std::async(std::launch::async, [key] { return SingleInstance(key).sendToRunning(QString()); });
        QTRY_COMPARE(received.count(), 1);
        QVERIFY(sent.get());
        QCOMPARE(received.first().at(0).toString(), QString());
    }
};

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "tst_singleinstance.moc"
