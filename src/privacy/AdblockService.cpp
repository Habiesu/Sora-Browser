#include "privacy/AdblockService.h"

#include "privacy/AdblockEngine.h"
#include "privacy/RequestInterceptor.h"

namespace sora {

AdblockService::AdblockService(QObject* parent)
    : QObject(parent)
{
}

void AdblockService::setEngine(std::shared_ptr<const AdblockEngine> engine)
{
    m_engine.store(std::move(engine));
}

bool AdblockService::decide(const QUrl& url, const QUrl& firstParty,
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

} // namespace sora
