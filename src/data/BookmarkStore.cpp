#include "data/BookmarkStore.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

#include "data/Database.h"

namespace sora {

BookmarkStore::BookmarkStore(const QString& databasePath, QObject* parent)
    : QObject(parent)
    , m_connection(openDatabase(databasePath, QStringLiteral("bookmarks"),
                                QStringLiteral("CREATE TABLE IF NOT EXISTS bookmarks ("
                                               " id INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT NOT NULL,"
                                               " title TEXT NOT NULL DEFAULT '', position INTEGER NOT NULL)")))
{
}

BookmarkStore::~BookmarkStore()
{
    closeDatabase(m_connection);
}

qint64 BookmarkStore::add(const QUrl& url, const QString& title)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("INSERT INTO bookmarks(url, title, position) "
                             "VALUES(?, ?, (SELECT IFNULL(MAX(position), -1) + 1 FROM bookmarks))"));
    q.addBindValue(url.toString());
    q.addBindValue(title.isEmpty() ? url.host() : title);
    q.exec();
    emit changed();
    return q.lastInsertId().toLongLong();
}

bool BookmarkStore::contains(const QUrl& url) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("SELECT 1 FROM bookmarks WHERE url = ? LIMIT 1"));
    q.addBindValue(url.toString());
    q.exec();
    return q.next();
}

QList<BookmarkStore::Bookmark> BookmarkStore::all() const
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.exec(QStringLiteral("SELECT id, url, title FROM bookmarks ORDER BY position, id"));
    QList<Bookmark> out;
    while (q.next())
        out.append({q.value(0).toLongLong(), QUrl(q.value(1).toString()), q.value(2).toString()});
    return out;
}

void BookmarkStore::remove(qint64 id)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("DELETE FROM bookmarks WHERE id = ?"));
    q.addBindValue(id);
    q.exec();
    emit changed();
}

void BookmarkStore::removeUrl(const QUrl& url)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("DELETE FROM bookmarks WHERE url = ?"));
    q.addBindValue(url.toString());
    q.exec();
    emit changed();
}

void BookmarkStore::rename(qint64 id, const QString& title)
{
    QSqlQuery q(QSqlDatabase::database(m_connection));
    q.prepare(QStringLiteral("UPDATE bookmarks SET title = ? WHERE id = ?"));
    q.addBindValue(title);
    q.addBindValue(id);
    q.exec();
    emit changed();
}

void BookmarkStore::move(qint64 id, int position)
{
    QList<qint64> order;
    for (const auto& b : all())
        if (b.id != id)
            order.append(b.id);
    order.insert(std::clamp(position, 0, int(order.size())), id);
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    db.transaction();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE bookmarks SET position = ? WHERE id = ?"));
    for (int i = 0; i < order.size(); ++i) {
        q.addBindValue(i);
        q.addBindValue(order[i]);
        q.exec();
    }
    db.commit();
    emit changed();
}

} // namespace sora
