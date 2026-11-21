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
#include <QStandardPaths>

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

void DownloadManager::save(const Item& d)
{
    if (m_connection.isEmpty() || d.incognito) // incógnito no deja rastro en disco
        return;
    const QString state = d.state == Done        ? QStringLiteral("done")
                          : d.state == Cancelled ? QStringLiteral("cancelled")
                                                 : QStringLiteral("failed");
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("INSERT INTO downloads(name, url, path, total, state, finished) VALUES(?, ?, ?, ?, ?, ?)"));
    q.addBindValue(d.name);
    q.addBindValue(d.url.toString());
    q.addBindValue(d.path);
    q.addBindValue(d.received);
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

QString DownloadManager::begin(uint32_t cefId, const QString& suggestedName, const QUrl& url, bool incognito)
{
    const QString folder = m_folder.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) : m_folder;
    QDir().mkpath(folder);
    // Como Chrome: "foto.jpg", "foto (1).jpg"... sin pisar nada.
    const QFileInfo name(suggestedName.isEmpty() ? QStringLiteral("descarga") : suggestedName);
    QString path = QDir(folder).filePath(name.fileName());
    for (int n = 1; QFileInfo::exists(path); ++n)
        path = QDir(folder).filePath(name.suffix().isEmpty()
                                         ? QStringLiteral("%1 (%2)").arg(name.completeBaseName()).arg(n)
                                         : QStringLiteral("%1 (%2).%3").arg(name.completeBaseName()).arg(n).arg(name.suffix()));
    Item item;
    item.cefId = cefId;
    item.name = QFileInfo(path).fileName();
    item.url = url;
    item.path = path;
    item.incognito = incognito;
    m_items.append(item);
    emit started(item.name);
    emit changed();
    return path;
}

void DownloadManager::update(const Update& u)
{
    for (Item& d : m_items) {
        if (d.cefId != u.id || d.removed)
            continue;
        const State before = d.state;
        d.received = u.received;
        d.total = u.total;
        if (!u.path.isEmpty())
            d.path = u.path;
        d.state = u.state;
        d.callback = u.state == Progress ? u.callback : nullptr;
        if (d.state != before) {
            if (d.state != Progress)
                save(d);
            emit changed();
        }
        return;
    }
}

DownloadManager::Item* DownloadManager::item(int id)
{
    return id >= 0 && id < m_items.size() && !m_items[id].removed ? &m_items[id] : nullptr;
}

QVariantList DownloadManager::list() const
{
    QVariantList out;
    for (int id = int(m_items.size()) - 1; id >= 0; --id) {
        const Item& d = m_items.at(id);
        if (d.removed)
            continue;
        QString state;
        switch (d.state) {
        case Progress: state = d.paused ? QStringLiteral("paused") : QStringLiteral("progress"); break;
        case Done: state = QStringLiteral("done"); break;
        case Cancelled: state = QStringLiteral("cancelled"); break;
        case Failed: state = QStringLiteral("failed"); break;
        }
        out.append(QVariantMap{{QStringLiteral("id"), id},
                               {QStringLiteral("name"), d.name},
                               {QStringLiteral("url"), d.url.toString()},
                               {QStringLiteral("path"), d.path},
                               {QStringLiteral("received"), d.received},
                               {QStringLiteral("total"), d.total},
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
    if (Item* d = item(id); d && d->callback)
        d->callback->Cancel();
}

void DownloadManager::pause(int id)
{
    if (Item* d = item(id); d && d->callback) {
        d->callback->Pause();
        d->paused = true;
        emit changed();
    }
}

void DownloadManager::resume(int id)
{
    if (Item* d = item(id); d && d->callback) {
        d->callback->Resume();
        d->paused = false;
        emit changed();
    }
}

void DownloadManager::open(int id) const
{
    const QString path = id < 0 ? pastPath(id)
                         : id < m_items.size() && m_items[id].state == Done ? m_items[id].path
                                                                            : QString();
    if (!path.isEmpty() && QFile::exists(path))
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
void DownloadManager::showInFolder(int id) const
{
    const QString path = QDir::toNativeSeparators(
        id < 0 ? pastPath(id) : id < m_items.size() ? m_items[id].path : QString());
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
    for (Item& d : m_items)
        if (d.state != Progress)
            d.removed = true; // el id de las demás no cambia
    m_past.clear();
    if (!m_connection.isEmpty())
        QSqlQuery(QSqlDatabase::database(m_connection)).exec(QStringLiteral("DELETE FROM downloads"));
    emit changed();
}

} // namespace sora
