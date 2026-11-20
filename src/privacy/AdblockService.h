#pragma once
#include <QObject>
#include <QUrl>
#include <QWebEngineUrlRequestInfo>
#include <atomic>
#include <memory>

namespace sora {

class AdblockEngine;

// Motor de bloqueo compartido por todas las pestañas. Seguro entre hilos: los interceptores lo
// consultan desde el hilo de Chromium mientras la interfaz cambia el motor.
class AdblockService : public QObject {
    Q_OBJECT
public:
    explicit AdblockService(QObject* parent = nullptr);

    // Sustituye el motor de forma atómica; nullptr = no bloquear nada.
    void setEngine(std::shared_ptr<const AdblockEngine> engine);
    bool decide(const QUrl& url, const QUrl& firstParty, QWebEngineUrlRequestInfo::ResourceType type) const;

private:
    std::atomic<std::shared_ptr<const AdblockEngine>> m_engine;
};

} // namespace sora
