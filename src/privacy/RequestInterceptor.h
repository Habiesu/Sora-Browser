#pragma once
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>

namespace sora {

class AdblockService;

// Traduce el tipo de recurso de Qt WebEngine al nombre que entiende adblock-rust.
const char* adblockRequestType(QWebEngineUrlRequestInfo::ResourceType type);

// Interceptor de una página: pregunta al servicio compartido y avisa de cada bloqueo para que la
// pestaña lleve su propio contador.
class RequestInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
public:
    explicit RequestInterceptor(const AdblockService* service, QObject* parent = nullptr);

    const AdblockService* service() const { return m_service; }
    void interceptRequest(QWebEngineUrlRequestInfo& info) override;

signals:
    void blocked(QUrl url);

private:
    const AdblockService* m_service;
};

} // namespace sora
