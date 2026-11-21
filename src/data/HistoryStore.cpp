#include "data/HistoryStore.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

#include "data/Database.h"

namespace sora {

HistoryStore::HistoryStore(const QString& databasePath)
    : m_connection(openDatabase(databasePath, QStringLiteral("history"),
                                QStringLiteral("CREATE TABLE IF NOT EXISTS history ("
                                               " url TEXT PRIMARY KEY, title TEXT NOT NULL DEFAULT '',"
                                               " last_visit INTEGER NOT NULL, visits INTEGER NOT NULL DEFAULT 1);"
                                               "CREATE INDEX IF NOT EXISTS history_last ON history(last_visit)")))
{
}

HistoryStore::~HistoryStore()
{
    closeDatabase(m_connection);
}

void HistoryStore::addVisit(const QUrl& url, const QString& title)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("INSERT INTO history(url, title, last_visit, visits) VALUES(?, ?, ?, 1) "
                             "ON CONFLICT(url) DO UPDATE SET visits = visits + 1, last_visit = excluded.last_visit, "
                             "title = CASE WHEN excluded.title <> '' THEN excluded.title ELSE title END"));
    q.addBindValue(url.toString());
    q.addBindValue(title);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.exec();
}

QList<HistoryStore::Entry> HistoryStore::query(const QString& where, const QString& order, const QString& text,
                                               int limit) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("SELECT url, title, last_visit, visits FROM history %1 ORDER BY %2 LIMIT ?").arg(where, order));
    if (!where.isEmpty()) {
        const QString pattern = QLatin1Char('%') + text + QLatin1Char('%');
        q.addBindValue(pattern);
        q.addBindValue(pattern);
    }
    q.addBindValue(limit);
    q.exec();
    QList<Entry> out;
    while (q.next())
        out.append({QUrl(q.value(0).toString()), q.value(1).toString(),
                    QDateTime::fromMSecsSinceEpoch(q.value(2).toLongLong()), q.value(3).toInt()});
    return out;
}

QList<HistoryStore::Entry> HistoryStore::search(const QString& text, int limit) const
{
    const QString where = text.isEmpty() ? QString() : QStringLiteral("WHERE url LIKE ? OR title LIKE ?");
    return query(where, QStringLiteral("last_visit DESC, rowid DESC"), text, limit);
}

QList<HistoryStore::Entry> HistoryStore::topSites(int limit) const
{
    return query({}, QStringLiteral("visits DESC, last_visit DESC"), {}, limit);
}

void HistoryStore::remove(const QUrl& url)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("DELETE FROM history WHERE url = ?"));
    q.addBindValue(url.toString());
    q.exec();
}

void HistoryStore::clear()
{
    QSqlQuery(QSqlDatabase::database(m_connection)).exec(QStringLiteral("DELETE FROM history"));
}

} // namespace sora
