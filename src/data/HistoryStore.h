#pragma once
#include <QDateTime>
#include <QList>
#include <QString>
#include <QUrl>

namespace sora {

// Historial de navegación en SQLite (una fila por dirección, con veces visitada y última visita).
class HistoryStore {
public:
    struct Entry {
        QUrl url;
        QString title;
        QDateTime lastVisit;
        int visits = 0;
    };

    explicit HistoryStore(const QString& databasePath);
    ~HistoryStore();
    HistoryStore(const HistoryStore&) = delete;
    HistoryStore& operator=(const HistoryStore&) = delete;

    void addVisit(const QUrl& url, const QString& title);
    // Lo más reciente primero; texto vacío = todo. Busca en dirección y título sin distinguir mayúsculas.
    QList<Entry> search(const QString& text, int limit = 300) const;
    QList<Entry> topSites(int limit) const; // los más visitados
    void remove(const QUrl& url);
    void clear();

private:
    QString m_connection;
    QList<Entry> query(const QString& where, const QString& order, const QString& text, int limit) const;
};

} // namespace sora
