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
    , m_flush(new QTimer(this))
{
    // El interceptor corre en el hilo de la interfaz (el mismo de Chromium): por cada petición
    // bloqueada solo se suma; la interfaz se entera de golpe un poco después.
    m_flush->setSingleShot(true);
    m_flush->setInterval(100);
    connect(m_flush, &QTimer::timeout, this, [this] {
        const QHash<QString, int> pending = std::exchange(m_pending, {});
        for (auto it = pending.cbegin(); it != pending.cend(); ++it)
            emit blockedOnSite(it.key(), it.value());
    });
}

void AdblockService::recordBlocked(const QUrl& firstParty)
{
    ++m_pending[firstParty.host()];
    if (!m_flush->isActive())
        m_flush->start();
}

void AdblockService::setEngine(std::shared_ptr<const AdblockEngine> engine)
{
    m_engine.store(std::move(engine));
}

void AdblockService::attach(QWebEngineProfile* profile)
{
    auto* interceptor = new RequestInterceptor(this, this);
    connect(interceptor, &RequestInterceptor::blocked, this, [this](const QUrl&, const QUrl& firstParty) { recordBlocked(firstParty); });
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

void AdblockService::setAllowedSites(const QStringList& sites)
{
    m_allowed.store(std::make_shared<const QSet<QString>>(sites.cbegin(), sites.cend()));
    for (const auto& profile : std::as_const(m_profiles))
        if (profile)
            updateScripts(profile);
}

QString AdblockService::siteOf(const QUrl& url)
{
    QString host = url.host().toLower();
    if (host.startsWith(QLatin1String("www.")))
        host.remove(0, 4);
    return host;
}

bool AdblockService::isAllowed(const QUrl& firstParty) const
{
    const auto allowed = m_allowed.load();
    if (!allowed || allowed->isEmpty())
        return false;
    // blog.lobos.test → lobos.test → test
    QString host = siteOf(firstParty);
    while (!host.isEmpty()) {
        if (allowed->contains(host))
            return true;
        const qsizetype dot = host.indexOf(QLatin1Char('.'));
        host = dot < 0 ? QString() : host.mid(dot + 1);
    }
    return false;
}

void AdblockService::updateScripts(QWebEngineProfile* profile)
{
    // shortcut: reglas de YouTube escritas a mano; usar los scriptlets de uBlock Origin desde
    // adblock-rust (use_resources) cuando haya una fuente de recursos que se pueda descargar.
    static const QString kName = QStringLiteral("sora-youtube");
    QWebEngineScriptCollection* scripts = profile->scripts();
    for (const QWebEngineScript& s : scripts->find(kName))
        scripts->remove(s);
    if (!m_enabled || isAllowed(QUrl(QStringLiteral("https://youtube.com/"))))
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
    if (!m_enabled || isAllowed(firstParty))
        return false;
    const auto engine = m_engine.load();
    if (!engine)
        return false; // el motor aún se está cargando
    return engine->shouldBlock(url, firstParty, adblockRequestType(type));
}

} // namespace sora
