#pragma once
#include <QObject>
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
    void setEnabled(bool enabled) { m_enabled = enabled; } // ajuste del usuario
    bool decide(const QUrl& url, const QUrl& firstParty, QWebEngineUrlRequestInfo::ResourceType type) const;
    // Pone el interceptor en el perfil: así se bloquea también lo que no sale de una página
    // (service workers). Cada bloqueo se avisa con blocked().
    void attach(QWebEngineProfile* profile);

signals:
    void blocked(QUrl url, QUrl firstParty);

private:
    std::atomic<std::shared_ptr<const AdblockEngine>> m_engine;
    std::atomic<bool> m_enabled{true};
};

} // namespace sora
