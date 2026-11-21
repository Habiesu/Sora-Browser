#include "browser/DownloadManager.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>
#include <QWebEngineDownloadRequest>
#include <QWebEngineProfile>

#include "data/Database.h"

namespace sora {

DownloadManager::~DownloadManager()
{
    if (!m_connection.isEmpty())
        closeDatabase(m_connection);
}

void DownloadManager::openHistory(const QString& databasePath)
{
    m_connection = openDatabase(databasePath, QStringLiteral("downloads-%1").arg(quintptr(this), 0, 16),
                                QStringLiteral("CREATE TABLE IF NOT EXISTS downloads ("
                                               " id INTEGER PRIMARY KEY, name TEXT NOT NULL, url TEXT NOT NULL,"
                                               " path TEXT NOT NULL, total INTEGER NOT NULL, state TEXT NOT NULL,"
                                               " finished INTEGER NOT NULL)"));
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.exec(QStringLiteral("SELECT id, name, url, path, total, state FROM downloads ORDER BY id DESC LIMIT 200"));
    while (q.next())
        m_past.append(QVariantMap{{QStringLiteral("id"), -q.value(0).toInt()},
                                  {QStringLiteral("name"), q.value(1)},
                                  {QStringLiteral("url"), q.value(2)},
                                  {QStringLiteral("path"), q.value(3)},
                                  {QStringLiteral("received"), q.value(4)},
                                  {QStringLiteral("total"), q.value(4)},
                                  {QStringLiteral("state"), q.value(5)}});
    emit changed();
}

void DownloadManager::save(const QWebEngineDownloadRequest* d)
{
    using R = QWebEngineDownloadRequest;
    if (m_connection.isEmpty())
        return;
    const QString state = d->state() == R::DownloadCompleted   ? QStringLiteral("done")
                          : d->state() == R::DownloadCancelled ? QStringLiteral("cancelled")
                                                               : QStringLiteral("failed");
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("INSERT INTO downloads(name, url, path, total, state, finished) VALUES(?, ?, ?, ?, ?, ?)"));
    q.addBindValue(d->downloadFileName());
    q.addBindValue(d->url().toString());
    q.addBindValue(QDir(d->downloadDirectory()).filePath(d->downloadFileName()));
    q.addBindValue(d->receivedBytes());
    q.addBindValue(state);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.exec();
}

QString DownloadManager::pastPath(int id) const
{
    for (const QVariant& v : m_past)
        if (v.toMap().value(QStringLiteral("id")).toInt() == id)
            return v.toMap().value(QStringLiteral("path")).toString();
    return {};
}

void DownloadManager::attach(QWebEngineProfile* profile)
{
    const bool incognito = profile->isOffTheRecord();
    connect(profile, &QWebEngineProfile::downloadRequested, this, [this, incognito](QWebEngineDownloadRequest* request) {
        if (request->isSavePageDownload() && askSavePath) {
            const QString path = askSavePath(QDir(request->downloadDirectory()).filePath(request->downloadFileName()));
            if (path.isEmpty()) {
                request->cancel();
                return;
            }
            request->setDownloadDirectory(QFileInfo(path).absolutePath());
            request->setDownloadFileName(QFileInfo(path).fileName());
        }
        connect(request, &QWebEngineDownloadRequest::stateChanged, this, &DownloadManager::changed);
        connect(request, &QWebEngineDownloadRequest::isPausedChanged, this, &DownloadManager::changed);
        if (!incognito) // incógnito no deja rastro en disco
            connect(request, &QWebEngineDownloadRequest::isFinishedChanged, this, [this, request] {
                if (request->isFinished())
                    save(request);
            });
        request->accept();
        m_items.append(request);
        emit started(request->downloadFileName());
        emit changed();
    });
}

QWebEngineDownloadRequest* DownloadManager::item(int id) const
{
    return id >= 0 && id < m_items.size() ? m_items.at(id).data() : nullptr;
}

QVariantList DownloadManager::list() const
{
    using R = QWebEngineDownloadRequest;
    QVariantList out;
    for (int id = int(m_items.size()) - 1; id >= 0; --id) {
        const R* d = m_items.at(id);
        if (!d)
            continue; // borrada con clearFinished o con su perfil
        QString state;
        switch (d->state()) {
        case R::DownloadRequested:
        case R::DownloadInProgress: state = d->isPaused() ? QStringLiteral("paused") : QStringLiteral("progress"); break;
        case R::DownloadCompleted: state = QStringLiteral("done"); break;
        case R::DownloadCancelled: state = QStringLiteral("cancelled"); break;
        case R::DownloadInterrupted: state = QStringLiteral("failed"); break;
        }
        out.append(QVariantMap{{QStringLiteral("id"), id},
                               {QStringLiteral("name"), d->downloadFileName()},
                               {QStringLiteral("url"), d->url().toString()},
                               {QStringLiteral("path"), QDir(d->downloadDirectory()).filePath(d->downloadFileName())},
                               {QStringLiteral("received"), d->receivedBytes()},
                               {QStringLiteral("total"), d->totalBytes()},
                               {QStringLiteral("state"), state}});
    }
    for (QVariant v : m_past) {
        QVariantMap d = v.toMap();
        if (d.value(QStringLiteral("state")) == QLatin1String("done") && !QFile::exists(d.value(QStringLiteral("path")).toString()))
            d[QStringLiteral("state")] = QStringLiteral("removed");
        out.append(d);
    }
    return out;
}

void DownloadManager::cancel(int id)
{
    if (auto* d = item(id))
        d->cancel();
}

void DownloadManager::pause(int id)
{
    if (auto* d = item(id))
        d->pause();
}

void DownloadManager::resume(int id)
{
    if (auto* d = item(id))
        d->resume();
}

void DownloadManager::open(int id) const
{
    if (id < 0) {
        if (const QString path = pastPath(id); QFile::exists(path))
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        return;
    }
    if (auto* d = item(id); d && d->state() == QWebEngineDownloadRequest::DownloadCompleted)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(d->downloadDirectory()).filePath(d->downloadFileName())));
}

void DownloadManager::showInFolder(int id) const
{
    auto* d = item(id);
    const QString path = QDir::toNativeSeparators(
        id < 0 ? pastPath(id) : d ? QDir(d->downloadDirectory()).filePath(d->downloadFileName()) : QString());
    if (path.isEmpty())
        return;
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,") + path});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void DownloadManager::clearFinished()
{
    for (auto& d : m_items)
        if (d && d->isFinished())
            d = nullptr; // el id de las demás no cambia
    m_past.clear();
    if (!m_connection.isEmpty())
        QSqlQuery(QSqlDatabase::database(m_connection)).exec(QStringLiteral("DELETE FROM downloads"));
    emit changed();
}

} // namespace sora
