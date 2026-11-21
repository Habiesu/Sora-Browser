#include <QtTest>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "privacy/AdblockEngine.h"
#include "privacy/FilterListStore.h"

using sora::FilterListStore;

static QByteArray smallRules()
{
    QFile f(qEnvironmentVariable("SORA_TEST_DATA") + "/rules-small.txt");
    if (!f.open(QIODevice::ReadOnly))
        qFatal("no se encuentra rules-small.txt");
    return f.readAll();
}

// Servidor HTTP mínimo que responde a cualquier GET con `body` y cuenta las peticiones.
class TinyHttpServer : public QTcpServer {
public:
    explicit TinyHttpServer(QByteArray body) : m_body(std::move(body))
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* s = nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, s, [this, s] {
                    s->readAll();
                    ++requests;
                    s->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(m_body.size()) +
                             "\r\nConnection: close\r\n\r\n" + m_body);
                    s->disconnectFromHost();
                });
            }
        });
        listen(QHostAddress::LocalHost);
    }
    QUrl url(const QString& path) const { return QUrl(QString("http://127.0.0.1:%1/%2").arg(serverPort()).arg(path)); }
    int requests = 0;

private:
    QByteArray m_body;
};

static bool blocksAd(const QList<QVariant>& args)
{
    const auto engine = args.at(0).value<std::shared_ptr<const sora::AdblockEngine>>();
    return engine && engine->shouldBlock(QUrl("https://ads.example/x.js"), QUrl("https://site.test"), "script");
}

// Acepta conexiones y nunca responde: simula un servidor que se cuelga.
class HangingServer : public QTcpServer {
public:
    HangingServer() { listen(QHostAddress::LocalHost); }
    QUrl url(const QString& path) const { return QUrl(QString("http://127.0.0.1:%1/%2").arg(serverPort()).arg(path)); }
};

class TestFilterListStore : public QObject {
    Q_OBJECT
private slots:
    void downloadsMissingLists()
    {
        QTemporaryDir dir;
        TinyHttpServer server(smallRules());
        QNetworkAccessManager nam;
        FilterListStore store(dir.path(), {server.url("easylist.txt")}, &nam);
        QSignalSpy ready(&store, &FilterListStore::engineReady);
        store.start();
        QVERIFY(ready.wait(10000));
        QVERIFY(blocksAd(ready.first()));
        QVERIFY(QFile::exists(dir.path() + "/lists/easylist.txt"));
        QVERIFY(QFile::exists(dir.path() + "/engine.bin"));
    }

    void usesCachedEngineWithoutNetwork()
    {
        QTemporaryDir dir;
        TinyHttpServer server(smallRules());
        QNetworkAccessManager nam;
        {
            FilterListStore first(dir.path(), {server.url("easylist.txt")}, &nam);
            QSignalSpy ready(&first, &FilterListStore::engineReady);
            first.start();
            QVERIFY(ready.wait(10000));
        }
        const int before = server.requests;
        FilterListStore second(dir.path(), {server.url("easylist.txt")}, &nam);
        QSignalSpy ready(&second, &FilterListStore::engineReady);
        second.start();
        QVERIFY(ready.wait(10000));
        QVERIFY(blocksAd(ready.first()));
        QCOMPARE(server.requests, before);
    }

    void changedListRebuildsEngine()
    {
        // La caché se reconoce por tamaño y fecha de las listas: una lista nueva no reutiliza el motor viejo.
        QTemporaryDir dir;
        TinyHttpServer server(smallRules());
        QNetworkAccessManager nam;
        {
            FilterListStore first(dir.path(), {server.url("easylist.txt")}, &nam);
            QSignalSpy ready(&first, &FilterListStore::engineReady);
            first.start();
            QVERIFY(ready.wait(10000));
        }
        QFile list(dir.path() + "/lists/easylist.txt");
        QVERIFY(list.open(QIODevice::WriteOnly));
        list.write("! lista nueva\n||otro.example^\n");
        list.close();
        FilterListStore second(dir.path(), {server.url("easylist.txt")}, &nam);
        QSignalSpy ready(&second, &FilterListStore::engineReady);
        second.start();
        QVERIFY(ready.wait(10000));
        const auto engine = ready.first().at(0).value<std::shared_ptr<const sora::AdblockEngine>>();
        QVERIFY(engine->shouldBlock(QUrl("https://otro.example/x.js"), QUrl("https://site.test"), "script"));
        QVERIFY(!blocksAd(ready.first()));
    }
    void rebuildsOnCorruptCache()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/lists");
        QFile list(dir.path() + "/lists/easylist.txt");
        QVERIFY(list.open(QIODevice::WriteOnly));
        list.write(smallRules());
        list.close();
        QFile bin(dir.path() + "/engine.bin");
        QVERIFY(bin.open(QIODevice::WriteOnly));
        bin.write("basura");
        bin.close();

        QNetworkAccessManager nam;
        FilterListStore store(dir.path(), {QUrl("http://127.0.0.1:1/easylist.txt")}, &nam);
        QSignalSpy ready(&store, &FilterListStore::engineReady);
        store.start();
        QVERIFY(ready.wait(10000));
        QVERIFY(blocksAd(ready.first()));
        QVERIFY(bin.open(QIODevice::ReadOnly));
        QVERIFY(sora::AdblockEngine::deserialize(bin.readAll()).has_value());
    }

    void hangingListDoesNotDelayCachedLists()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.path() + "/lists");
        QFile list(dir.path() + "/lists/easylist.txt");
        QVERIFY(list.open(QIODevice::WriteOnly));
        list.write(smallRules());
        list.close();

        HangingServer hanging;
        QNetworkAccessManager nam;
        FilterListStore store(dir.path(), {QUrl("http://127.0.0.1:1/easylist.txt"), hanging.url("other.txt")}, &nam);
        QSignalSpy ready(&store, &FilterListStore::engineReady);
        store.start();
        QVERIFY(ready.wait(3000));
        QVERIFY(blocksAd(ready.first()));
    }

    void htmlPageIsNotSavedAsList()
    {
        QTemporaryDir dir;
        TinyHttpServer portal("<html><body>Inicia sesión en el Wi-Fi del hotel</body></html>");
        QNetworkAccessManager nam;
        FilterListStore store(dir.path(), {portal.url("easylist.txt")}, &nam);
        QSignalSpy failed(&store, &FilterListStore::failed);
        store.start();
        QVERIFY(failed.wait(10000));
        QVERIFY(!QFile::exists(dir.path() + "/lists/easylist.txt"));
    }

    void noNetworkFirstRunFails()
    {
        QTemporaryDir dir;
        QNetworkAccessManager nam;
        FilterListStore store(dir.path(), {QUrl("http://127.0.0.1:1/easylist.txt")}, &nam);
        QSignalSpy ready(&store, &FilterListStore::engineReady);
        QSignalSpy failed(&store, &FilterListStore::failed);
        store.start();
        QVERIFY(failed.wait(10000));
        QCOMPARE(ready.count(), 0);
        QVERIFY(!failed.first().at(0).toString().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestFilterListStore)
#include "tst_filterliststore.moc"
