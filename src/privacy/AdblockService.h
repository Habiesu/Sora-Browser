#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QWebEngineUrlRequestInfo>
#include <atomic>
#include <memory>

class QWebEngineProfile;

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
    // Sitios donde el usuario apagó el escudo (como en Brave): ahí y en sus subdominios no se bloquea nada.
    void setAllowedSites(const QStringList& sites);
    // El sitio de una dirección tal como se guarda en esa lista: el host sin "www.".
    static QString siteOf(const QUrl& url);
    bool decide(const QUrl& url, const QUrl& firstParty, QWebEngineUrlRequestInfo::ResourceType type) const;
    // Pone el interceptor en el perfil: así se bloquea también lo que no sale de una página
    // (service workers). Cada bloqueo se avisa con blocked(). También pone el script que quita los
    // anuncios de YouTube (resources/adblock/youtube.js).
    void attach(QWebEngineProfile* profile);

signals:
    void blocked(QUrl url, QUrl firstParty);

private:
    std::atomic<std::shared_ptr<const AdblockEngine>> m_engine;
    std::atomic<bool> m_enabled{true};
    std::atomic<std::shared_ptr<const QSet<QString>>> m_allowed; // se lee desde el hilo de red
    bool isAllowed(const QUrl& firstParty) const;
    QList<QPointer<QWebEngineProfile>> m_profiles;
    void updateScripts(QWebEngineProfile* profile);
};

} // namespace sora
