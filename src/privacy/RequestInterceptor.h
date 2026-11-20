#pragma once
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>
#include <atomic>
#include <memory>

namespace sora {

class AdblockEngine;

// Traduce el tipo de recurso de Qt WebEngine al nombre que entiende adblock-rust.
const char* adblockRequestType(QWebEngineUrlRequestInfo::ResourceType type);

// Consulta el bloqueador en cada petición. Se ejecuta en el hilo de E/S de Chromium.
class RequestInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
public:
    explicit RequestInterceptor(QObject* parent = nullptr);

    // Sustituye el motor de forma atómica; nullptr = no bloquear nada.
    void setEngine(std::shared_ptr<const AdblockEngine> engine);
    bool decide(const QUrl& url, const QUrl& firstParty, QWebEngineUrlRequestInfo::ResourceType type) const;
    void interceptRequest(QWebEngineUrlRequestInfo& info) override;

signals:
    void blocked(QUrl url);

private:
    std::atomic<std::shared_ptr<const AdblockEngine>> m_engine;
};

} // namespace sora
