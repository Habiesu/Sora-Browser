#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QStringList>
#include <QUrl>
#include <atomic>
#include <memory>

namespace sora {

class AdblockEngine;


// Motor de bloqueo compartido por todas las pestañas. El motor se cambia de forma atómica, así que
// es seguro aunque se consulte desde otro hilo.
class AdblockService : public QObject {
    Q_OBJECT
public:
    explicit AdblockService(QObject* parent = nullptr);

    // Sustituye el motor de forma atómica; nullptr = no bloquear nada.
    void setEngine(std::shared_ptr<const AdblockEngine> engine);
    // Ajuste del usuario: apagado no bloquea nada ni toca YouTube.
    void setEnabled(bool enabled);
    void recordBlocked(const QUrl& firstParty); // lo llama el interceptor
    // Sitios donde el usuario apagó el escudo (como en Brave): ahí y en sus subdominios no se bloquea nada.
    void setAllowedSites(const QStringList& sites);
    // El sitio de una dirección tal como se guarda en esa lista: el host sin "www.".
    static QString siteOf(const QUrl& url);
    // ¿Bloquear esta petición? type es el de adblock-rust ("script", "image"...). Seguro entre hilos.
    bool decide(const QUrl& url, const QUrl& firstParty, const char* type) const;
    // Script que quita los anuncios de YouTube (resources/adblock/youtube.js), ya limitado a
    // youtube.com; vacío si el escudo está apagado ahí. Las pestañas lo ponen en cada página.
    QString youtubeScript() const { return m_youtubeScript; }

signals:
    // Bloqueos agrupados por sitio, como mucho cada 100 ms (no uno por petición).
    void blockedOnSite(const QString& host, int count);
    void youtubeScriptChanged();

private:
    std::atomic<std::shared_ptr<const AdblockEngine>> m_engine;
    std::atomic<bool> m_enabled{true};
    std::atomic<std::shared_ptr<const QSet<QString>>> m_allowed; // se lee desde el hilo de red
    bool isAllowed(const QUrl& firstParty) const;
    QHash<QString, int> m_pending; // bloqueos aún sin avisar, por sitio
    QTimer* m_flush;
    QString m_youtubeScript;
    void updateScript();
};

} // namespace sora
