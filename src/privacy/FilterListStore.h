#pragma once
#include <QList>
#include <QObject>
#include <QUrl>
#include <memory>

class QNetworkAccessManager;

namespace sora {

class AdblockEngine;

// Mantiene en disco las listas de filtros y el motor compilado (engine.bin), y entrega el
// motor listo sin bloquear nunca el hilo de la interfaz. engineReady puede emitirse más de una
// vez: primero con las listas en disco y otra vez cuando terminan de llegar las que faltaban.
//   <dir>/lists/<archivo>.txt   listas descargadas
//   <dir>/engine.bin            motor serializado
//   <dir>/engine.version        huella de la versión del formato y del contenido de las listas
class FilterListStore : public QObject {
    Q_OBJECT
public:
    FilterListStore(QString dir, QList<QUrl> lists, QNetworkAccessManager* nam, QObject* parent = nullptr);
    void start();

signals:
    void engineReady(std::shared_ptr<const sora::AdblockEngine> engine);
    void failed(QString reason);

private:
    static QString fileNameFor(const QUrl& url);
    void build();

    QString m_dir;
    QList<QUrl> m_lists;
    QNetworkAccessManager* m_nam;
    int m_pendingDownloads = 0;
    bool m_downloadedAny = false;
};

} // namespace sora

Q_DECLARE_METATYPE(std::shared_ptr<const sora::AdblockEngine>)
