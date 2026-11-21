#include "app/InternalApi.h"

#include <QUrl>

#include "data/BookmarkStore.h"
#include "data/HistoryStore.h"

namespace sora {

InternalApi::InternalApi(HistoryStore* history, BookmarkStore* bookmarks, QObject* parent)
    : QObject(parent)
    , m_history(history)
    , m_bookmarks(bookmarks)
{
    if (m_bookmarks)
        connect(m_bookmarks, &BookmarkStore::changed, this, &InternalApi::bookmarksChanged);
}

QVariantList InternalApi::history(const QString& query) const
{
    QVariantList out;
    if (!m_history)
        return out;
    for (const auto& e : m_history->search(query))
        out.append(QVariantMap{{QStringLiteral("url"), e.url.toString()},
                               {QStringLiteral("title"), e.title},
                               {QStringLiteral("time"), e.lastVisit.toMSecsSinceEpoch()},
                               {QStringLiteral("visits"), e.visits}});
    return out;
}

void InternalApi::removeHistory(const QString& url)
{
    if (m_history)
        m_history->remove(QUrl(url));
}

void InternalApi::clearHistory()
{
    if (m_history)
        m_history->clear();
}

QVariantList InternalApi::bookmarks() const
{
    QVariantList out;
    if (!m_bookmarks)
        return out;
    for (const auto& b : m_bookmarks->all())
        out.append(QVariantMap{{QStringLiteral("id"), b.id},
                               {QStringLiteral("url"), b.url.toString()},
                               {QStringLiteral("title"), b.title}});
    return out;
}

void InternalApi::removeBookmark(qint64 id)
{
    if (m_bookmarks)
        m_bookmarks->remove(id);
}

void InternalApi::renameBookmark(qint64 id, const QString& title)
{
    if (m_bookmarks && !title.trimmed().isEmpty())
        m_bookmarks->rename(id, title.trimmed());
}

void InternalApi::moveBookmark(qint64 id, int position)
{
    if (m_bookmarks)
        m_bookmarks->move(id, position);
}

} // namespace sora
