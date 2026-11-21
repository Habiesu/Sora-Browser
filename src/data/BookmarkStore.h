#pragma once
#include <QList>
#include <QObject>
#include <QUrl>

namespace sora {

// Marcadores en SQLite, en el orden de la barra de favoritos.
class BookmarkStore : public QObject {
    Q_OBJECT
public:
    struct Bookmark {
        qint64 id = 0;
        QUrl url;
        QString title;
    };

    explicit BookmarkStore(const QString& databasePath, QObject* parent = nullptr);
    ~BookmarkStore() override;

    qint64 add(const QUrl& url, const QString& title);
    bool contains(const QUrl& url) const;
    QList<Bookmark> all() const;
    void remove(qint64 id);
    void removeUrl(const QUrl& url);
    void rename(qint64 id, const QString& title);
    void move(qint64 id, int position);

signals:
    void changed();

private:
    QString m_connection;
};

} // namespace sora
