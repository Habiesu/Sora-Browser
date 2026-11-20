#include "privacy/FilterListStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QtConcurrent>

#include "privacy/AdblockEngine.h"

namespace sora {

namespace {

// Cambiar este prefijo invalida todas las cachés (p. ej. al actualizar el crate adblock).
constexpr QByteArrayView kFormatTag = "sora-adblock-ffi-0.1/adblock-0.13";

struct BuildResult {
    std::shared_ptr<const AdblockEngine> engine;
    QString error;
};

bool writeAtomically(const QString& path, const QByteArray& data)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(data);
    return f.commit();
}

// Rechaza respuestas que no son listas (p. ej. la página de inicio de sesión de un Wi-Fi público).
bool looksLikeFilterList(QByteArray body)
{
    if (body.startsWith("\xEF\xBB\xBF"))
        body.remove(0, 3);
    body = body.trimmed();
    return body.startsWith("[Adblock") || body.startsWith('!');
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Se ejecuta fuera del hilo de interfaz.
BuildResult buildEngine(const QString& dir, const QStringList& listFiles)
{
    QByteArray rules;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(kFormatTag);
    for (const QString& file : listFiles) {
        const QByteArray content = readAll(dir + QStringLiteral("/lists/") + file);
        hash.addData(file.toUtf8());
        hash.addData(content);
        rules += content;
        rules += '\n';
    }
    const QByteArray fingerprint = hash.result().toHex();
    const QString binPath = dir + QStringLiteral("/engine.bin");
    const QString versionPath = dir + QStringLiteral("/engine.version");

    if (readAll(versionPath).trimmed() == fingerprint) {
        if (auto cached = AdblockEngine::deserialize(readAll(binPath)))
            return {std::make_shared<const AdblockEngine>(std::move(*cached)), {}};
    }

    auto engine = std::make_shared<const AdblockEngine>(AdblockEngine::fromRules(rules));
    // Si no se puede guardar la caché, el motor sigue siendo válido: se recompilará la próxima vez.
    if (writeAtomically(binPath, engine->serialize()))
        writeAtomically(versionPath, fingerprint);
    return {engine, {}};
}

} // namespace

FilterListStore::FilterListStore(QString dir, QList<QUrl> lists, QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent)
    , m_dir(std::move(dir))
    , m_lists(std::move(lists))
    , m_nam(nam)
{
    qRegisterMetaType<std::shared_ptr<const AdblockEngine>>();
}

QString FilterListStore::fileNameFor(const QUrl& url)
{
    return url.fileName().isEmpty() ? QStringLiteral("list.txt") : url.fileName();
}

void FilterListStore::start()
{
    QDir().mkpath(m_dir + QStringLiteral("/lists"));

    QList<QUrl> missing;
    for (const QUrl& url : m_lists) {
        if (!QFileInfo::exists(m_dir + QStringLiteral("/lists/") + fileNameFor(url)))
            missing << url;
    }
    const bool haveSomeLists = missing.size() < m_lists.size();

    // Con las listas que ya hay se prepara el motor enseguida; una descarga lenta no debe retrasarlo.
    if (haveSomeLists)
        build();
    if (missing.isEmpty())
        return;

    m_pendingDownloads = int(missing.size());
    m_downloadedAny = false;
    for (const QUrl& url : missing) {
        QNetworkRequest request(url);
        request.setTransferTimeout(30000);
        QNetworkReply* reply = m_nam->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, url, haveSomeLists] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            if (reply->error() == QNetworkReply::NoError && status == 200 && looksLikeFilterList(body)
                && writeAtomically(m_dir + QStringLiteral("/lists/") + fileNameFor(url), body)) {
                m_downloadedAny = true;
            }
            if (--m_pendingDownloads > 0)
                return;
            if (m_downloadedAny)
                build(); // nuevo motor con las listas recién llegadas
            else if (!haveSomeLists)
                emit failed(tr("No se pudieron descargar las listas de bloqueo. Se volverá a intentar en el próximo arranque."));
        });
    }
}

void FilterListStore::build()
{
    QStringList present;
    for (const QUrl& url : m_lists) {
        const QString name = fileNameFor(url);
        if (QFileInfo::exists(m_dir + QStringLiteral("/lists/") + name))
            present << name;
    }
    if (present.isEmpty()) {
        emit failed(tr("No se pudieron descargar las listas de bloqueo. Se volverá a intentar en el próximo arranque."));
        return;
    }

    QtConcurrent::run(buildEngine, m_dir, present).then(this, [this](const BuildResult& result) {
        if (result.engine)
            emit engineReady(result.engine);
        else
            emit failed(result.error);
    });
}

} // namespace sora
