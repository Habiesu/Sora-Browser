#pragma once
#include <QObject>
#include <QPointer>
#include <functional>
#include <vector>

#include "include/cef_request_context.h"

namespace sora {

class AdblockService;
class DownloadManager;

// Un perfil de navegación sobre un CefRequestContext. El normal guarda en disco (cookies, caché);
// incógnito vive en memoria y no deja nada al cerrarse. Las peticiones de todas sus pestañas (y de
// sus service workers) pasan por el bloqueador.
class Profile : public QObject {
    Q_OBJECT
public:
    explicit Profile(bool incognito, AdblockService* adblock, QObject* parent = nullptr);
    ~Profile() override;

    bool isOffTheRecord() const { return m_incognito; }
    // Se crea la primera vez (CEF tiene que estar arrancado).
    CefRefPtr<CefRequestContext> context();
    // Se llama al crear el contexto (p. ej. para registrar sora://).
    void onContextCreated(std::function<void(CefRefPtr<CefRequestContext>)> hook) { m_onContext.push_back(std::move(hook)); }
    AdblockService* adblock() const { return m_adblock; }
    DownloadManager* downloads() const { return m_downloads; }
    void setDownloads(DownloadManager* downloads) { m_downloads = downloads; }

    void clearCookies();
    // La caché de disco se borra al siguiente arranque (CEF no deja borrarla en marcha).
    void clearCache();

private:
    bool m_incognito;
    AdblockService* m_adblock; // los dos viven tanto como la aplicación
    DownloadManager* m_downloads = nullptr;
    CefRefPtr<CefRequestContext> m_context;
    std::vector<std::function<void(CefRefPtr<CefRequestContext>)>> m_onContext;
};

// Ruta del aviso "borrar la caché al arrancar" (lo mira Engine::start).
QString clearCacheMarker();

} // namespace sora
