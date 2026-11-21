#include "privacy/AdblockService.h"

#include "privacy/AdblockEngine.h"
#include "privacy/RequestInterceptor.h"

#include <QFile>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

namespace sora {

AdblockService::AdblockService(QObject* parent)
    : QObject(parent)
{
}

void AdblockService::setEngine(std::shared_ptr<const AdblockEngine> engine)
{
    m_engine.store(std::move(engine));
}

void AdblockService::attach(QWebEngineProfile* profile)
{
    auto* interceptor = new RequestInterceptor(this, this);
    connect(interceptor, &RequestInterceptor::blocked, this, &AdblockService::blocked);
    profile->setUrlRequestInterceptor(interceptor);
    m_profiles.append(profile);
    updateScripts(profile);
}

void AdblockService::setEnabled(bool enabled)
{
    m_enabled = enabled;
    for (const auto& profile : std::as_const(m_profiles))
        if (profile)
            updateScripts(profile);
}

void AdblockService::updateScripts(QWebEngineProfile* profile)
{
    // shortcut: reglas de YouTube escritas a mano; usar los scriptlets de uBlock Origin desde
    // adblock-rust (use_resources) cuando haya una fuente de recursos que se pueda descargar.
    static const QString kName = QStringLiteral("sora-youtube");
    QWebEngineScriptCollection* scripts = profile->scripts();
    for (const QWebEngineScript& s : scripts->find(kName))
        scripts->remove(s);
    if (!m_enabled)
        return;
    QFile file(QStringLiteral(":/adblock/youtube.js"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    QWebEngineScript script;
    script.setName(kName);
    script.setSourceCode(QString::fromUtf8(file.readAll())); // la cabecera @match limita a youtube.com
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld); // tiene que ver los objetos de la propia página
    script.setRunsOnSubFrames(false);
    scripts->insert(script);
}

bool AdblockService::decide(const QUrl& url, const QUrl& firstParty,
                            QWebEngineUrlRequestInfo::ResourceType type) const
{
    // Navegar a una web (aunque esté en una lista) siempre debe funcionar.
    if (type == QWebEngineUrlRequestInfo::ResourceTypeMainFrame
        || type == QWebEngineUrlRequestInfo::ResourceTypeNavigationPreloadMainFrame)
        return false;
    if (!m_enabled)
        return false;
    const auto engine = m_engine.load();
    if (!engine)
        return false; // el motor aún se está cargando
    return engine->shouldBlock(url, firstParty, adblockRequestType(type));
}

} // namespace sora
