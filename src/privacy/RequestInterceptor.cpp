#include "privacy/RequestInterceptor.h"

#include "privacy/AdblockService.h"

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

RequestInterceptor::RequestInterceptor(const AdblockService* service, QObject* parent)
    : QWebEngineUrlRequestInterceptor(parent)
    , m_service(service)
{
}

void RequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info)
{
    if (m_service->decide(info.requestUrl(), info.firstPartyUrl(), info.resourceType())) {
        info.block(true);
        emit blocked(info.requestUrl(), info.firstPartyUrl());
    }
}

} // namespace sora
