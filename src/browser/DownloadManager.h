#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <functional>

class QWebEngineDownloadRequest;
class QWebEngineProfile;

namespace sora {

// Descargas de todos los perfiles: se aceptan sin preguntar en la carpeta de descargas del perfil
// (como Chrome). Las terminadas se guardan en sora.db (menos las de incógnito) y siguen en la lista
// al volver a abrir Sora.
class DownloadManager : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~DownloadManager() override;

    void attach(QWebEngineProfile* profile);
    // Carga las descargas de otras veces y guarda ahí las que terminen.
    void openHistory(const QString& databasePath);
    // "Guardar página como": devuelve la ruta elegida o vacío para cancelar. Sin él, va a Descargas.
    std::function<QString(const QString& suggestedPath)> askSavePath;

    // [{id, name, url, path, received, total, state: "progress"|"paused"|"done"|"cancelled"|"failed"|"removed"}],
    // la más reciente primero. Las de otras veces tienen id negativo; "removed" = el archivo ya no está.
    QVariantList list() const;
    void cancel(int id);
    void pause(int id);
    void resume(int id);
    void open(int id) const;
    void showInFolder(int id) const;
    void clearFinished();

signals:
    void started(const QString& fileName);
    void changed(); // alta, fin o cambio de estado (el avance de bytes no avisa: la página lo consulta)

private:
    QList<QPointer<QWebEngineDownloadRequest>> m_items; // el id es la posición
    QVariantList m_past; // de otras veces, como las da list()
    QString m_connection;

    void save(const QWebEngineDownloadRequest* request);
    QString pastPath(int id) const;

    QWebEngineDownloadRequest* item(int id) const;
};

} // namespace sora
