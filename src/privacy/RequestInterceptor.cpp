#include "privacy/RequestInterceptor.h"

#include "privacy/AdblockEngine.h"

namespace sora {

const char* adblockRequestType(QWebEngineUrlRequestInfo::ResourceType type)
{
    using RT = QWebEngineUrlRequestInfo::ResourceType;
    switch (type) {
    case RT::ResourceTypeMainFrame:
    case RT::ResourceTypeNavigationPreloadMainFrame:
        return "document";
    case RT::ResourceTypeSubFrame:
    case RT::ResourceTypeNavigationPreloadSubFrame:
        return "subdocument";
    case RT::ResourceTypeStylesheet:
        return "stylesheet";
    case RT::ResourceTypeScript:
    case RT::ResourceTypeWorker:
    case RT::ResourceTypeSharedWorker:
    case RT::ResourceTypeServiceWorker:
        return "script";
    case RT::ResourceTypeImage:
    case RT::ResourceTypeFavicon:
        return "image";
    case RT::ResourceTypeFontResource:
        return "font";
    case RT::ResourceTypeObject:
    case RT::ResourceTypePluginResource:
        return "object";
    case RT::ResourceTypeMedia:
        return "media";
    case RT::ResourceTypeXhr:
    case RT::ResourceTypeJson:
        return "xmlhttprequest";
    case RT::ResourceTypePing:
    case RT::ResourceTypeCspReport:
        return "ping";
    case RT::ResourceTypeWebSocket:
        return "websocket";
    default:
        return "other";
    }
}

RequestInterceptor::RequestInterceptor(QObject* parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
}

void RequestInterceptor::setEngine(std::shared_ptr<const AdblockEngine> engine)
{
    m_engine.store(std::move(engine));
}

bool RequestInterceptor::decide(const QUrl& url, const QUrl& firstParty,
                                QWebEngineUrlRequestInfo::ResourceType type) const
{
    // Navegar a una web (aunque esté en una lista) siempre debe funcionar.
    if (type == QWebEngineUrlRequestInfo::ResourceTypeMainFrame
        || type == QWebEngineUrlRequestInfo::ResourceTypeNavigationPreloadMainFrame)
        return false;
    const auto engine = m_engine.load();
    if (!engine)
        return false; // el motor aún se está cargando
    return engine->shouldBlock(url, firstParty, adblockRequestType(type));
}

void RequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info)
{
    if (decide(info.requestUrl(), info.firstPartyUrl(), info.resourceType())) {
        info.block(true);
        emit blocked(info.requestUrl()); // las conexiones con la interfaz son en cola (otro hilo)
    }
}

} // namespace sora
