#pragma once
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>

namespace sora {

class AdblockService;

// Traduce el tipo de recurso de Qt WebEngine al nombre que entiende adblock-rust.
const char* adblockRequestType(QWebEngineUrlRequestInfo::ResourceType type);

// Interceptor del perfil: pregunta al servicio y avisa de cada bloqueo con la web de primer nivel,
// para que se cuente en la pestaña de ese sitio.
class RequestInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
public:
    explicit RequestInterceptor(const AdblockService* service, QObject* parent = nullptr);

    const AdblockService* service() const { return m_service; }
    void interceptRequest(QWebEngineUrlRequestInfo& info) override;

signals:
    void blocked(QUrl url, QUrl firstParty);

private:
    const AdblockService* m_service;
};

} // namespace sora
