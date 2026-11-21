#include "privacy/AdblockService.h"

#include "privacy/AdblockEngine.h"

#include <QFile>

namespace sora {

AdblockService::AdblockService(QObject* parent)
    : QObject(parent)
    , m_flush(new QTimer(this))
{
    // Cada bloqueo llega del hilo de red de CEF por la cola de Qt: solo se suma; la interfaz se
    // entera de golpe un poco después.
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

void AdblockService::setEnabled(bool enabled)
{
    m_enabled = enabled;
    updateScript();
}

void AdblockService::setAllowedSites(const QStringList& sites)
{
    m_allowed.store(std::make_shared<const QSet<QString>>(sites.cbegin(), sites.cend()));
    updateScript();
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

void AdblockService::updateScript()
{
    // shortcut: reglas de YouTube escritas a mano; usar los scriptlets de uBlock Origin desde
    // adblock-rust (use_resources) cuando haya una fuente de recursos que se pueda descargar.
    QString script;
    QFile file(QStringLiteral(":/adblock/youtube.js"));
    if (m_enabled && !isAllowed(QUrl(QStringLiteral("https://youtube.com/"))) && file.open(QIODevice::ReadOnly))
        script = QStringLiteral("if (/(^|\\.)youtube\\.com$/.test(location.hostname)) {\n%1\n}")
                     .arg(QString::fromUtf8(file.readAll()));
    if (script == m_youtubeScript)
        return;
    m_youtubeScript = script;
    emit youtubeScriptChanged();
}

bool AdblockService::decide(const QUrl& url, const QUrl& firstParty, const char* type) const
{
    if (!m_enabled || isAllowed(firstParty))
        return false;
    const auto engine = m_engine.load();
    if (!engine)
        return false; // el motor aún se está cargando
    return engine->shouldBlock(url, firstParty, type);
}

} // namespace sora
