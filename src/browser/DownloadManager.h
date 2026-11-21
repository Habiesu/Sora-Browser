#pragma once
#include <QList>
#include <QObject>
#include <QUrl>
#include <QVariantList>

#include "include/cef_download_handler.h"

namespace sora {

// Descargas de todos los perfiles: se aceptan sin preguntar en la carpeta de descargas (como Chrome).
// Las terminadas se guardan en sora.db (menos las de incógnito) y siguen en la lista al volver a
// abrir Sora. CEF avisa por las pestañas (begin/update), siempre en el hilo de Qt.
class DownloadManager : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~DownloadManager() override;

    enum State { Progress, Done, Cancelled, Failed };
    struct Update {
        uint32_t id = 0;
        qint64 received = 0;
        qint64 total = 0;
        QString path;
        State state = Progress;
        CefRefPtr<CefDownloadItemCallback> callback;
    };

    void setFolder(const QString& folder) { m_folder = folder; }
    // Una descarga nueva: devuelve la ruta donde guardarla (sin pisar archivos), o vacío para no bajarla.
    QString begin(uint32_t cefId, const QString& suggestedName, const QUrl& url, bool incognito);
    void update(const Update& update);
    // Carga las descargas de otras veces y guarda ahí las que terminen.
    void openHistory(const QString& databasePath);

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
    struct Item {
        uint32_t cefId = 0;
        QString name;
        QUrl url;
        QString path;
        qint64 received = 0;
        qint64 total = -1;
        State state = Progress;
        bool paused = false;
        bool incognito = false;
        bool removed = false; // quitada de la lista con clearFinished
        CefRefPtr<CefDownloadItemCallback> callback;
    };
    QList<Item> m_items; // el id es la posición
    QVariantList m_past; // de otras veces, como las da list()
    QString m_connection;
    QString m_folder;

    void save(const Item& item);
    QString pastPath(int id) const;
    Item* item(int id);
};

} // namespace sora
