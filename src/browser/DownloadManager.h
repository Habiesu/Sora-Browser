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
// (como Chrome). shortcut: la lista vive en memoria y se pierde al cerrar Sora; guardarla en
// sora.db si el usuario echa de menos las descargas antiguas.
class DownloadManager : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    void attach(QWebEngineProfile* profile);
    // "Guardar página como": devuelve la ruta elegida o vacío para cancelar. Sin él, va a Descargas.
    std::function<QString(const QString& suggestedPath)> askSavePath;

    // [{id, name, url, path, received, total, state: "progress"|"paused"|"done"|"cancelled"|"failed"}],
    // la más reciente primero.
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

    QWebEngineDownloadRequest* item(int id) const;
};

} // namespace sora
