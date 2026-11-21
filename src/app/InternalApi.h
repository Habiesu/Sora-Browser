#pragma once
#include <QObject>
#include <QPointer>
#include <QVariantList>

namespace sora {

class BookmarkStore;
class HistoryStore;

// Lo que las páginas sora:// pueden pedirle a Sora (por QWebChannel, objeto "sora" en JavaScript).
// Solo se da a páginas sora://: ninguna web normal lo ve.
class InternalApi : public QObject {
    Q_OBJECT
public:
    InternalApi(HistoryStore* history, BookmarkStore* bookmarks, QObject* parent = nullptr);

    // Historial: [{url, title, time (ms), visits}], lo más reciente primero.
    Q_INVOKABLE QVariantList history(const QString& query) const;
    Q_INVOKABLE void removeHistory(const QString& url);
    Q_INVOKABLE void clearHistory();

    // Marcadores: [{id, url, title}] en el orden de la barra.
    Q_INVOKABLE QVariantList bookmarks() const;
    Q_INVOKABLE void removeBookmark(qint64 id);
    Q_INVOKABLE void renameBookmark(qint64 id, const QString& title);
    Q_INVOKABLE void moveBookmark(qint64 id, int position);

signals:
    void bookmarksChanged();

private:
    HistoryStore* m_history;
    QPointer<BookmarkStore> m_bookmarks;
};

} // namespace sora
