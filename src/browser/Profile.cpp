#include "browser/Profile.h"

#include <QFile>
#include <QStandardPaths>
#include <QUrl>

#include "include/cef_cookie.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_resource_request_handler.h"

#include "browser/Engine.h"
#include "privacy/AdblockService.h"

namespace sora {
namespace {

const char* adblockRequestType(cef_resource_type_t type)
{
    switch (type) {
    case RT_MAIN_FRAME: return "document";
    case RT_SUB_FRAME: return "subdocument";
    case RT_STYLESHEET: return "stylesheet";
    case RT_SCRIPT:
    case RT_WORKER:
    case RT_SHARED_WORKER:
    case RT_SERVICE_WORKER: return "script";
    case RT_IMAGE:
    case RT_FAVICON: return "image";
    case RT_FONT_RESOURCE: return "font";
    case RT_OBJECT:
    case RT_PLUGIN_RESOURCE: return "object";
    case RT_MEDIA: return "media";
    case RT_XHR: return "xmlhttprequest";
    case RT_PING:
    case RT_CSP_REPORT: return "ping";
    default: return "other";
    }
}

// Corre en el hilo IO de CEF. AdblockService::decide es seguro entre hilos; el aviso al escudo
// se pasa al hilo de Qt.
class RequestHandler : public CefRequestContextHandler, public CefResourceRequestHandler {
public:
    explicit RequestHandler(AdblockService* adblock) : m_adblock(adblock) {}

    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
                                                                   CefRefPtr<CefRequest>, bool, bool,
                                                                   const CefString&, bool&) override
    {
        return m_adblock ? this : nullptr;
    }

    ReturnValue OnBeforeResourceLoad(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest> request,
                                     CefRefPtr<CefCallback>) override
    {
        const cef_resource_type_t type = request->GetResourceType();
        if (type == RT_MAIN_FRAME) // navegar a una web (aunque esté en una lista) siempre debe funcionar
            return RV_CONTINUE;
        const QUrl url(QString::fromStdString(request->GetURL().ToString()));
        QUrl firstParty(QString::fromStdString(request->GetFirstPartyForCookies().ToString()));
        if (firstParty.isEmpty())
            firstParty = QUrl(QString::fromStdString(request->GetReferrerURL().ToString()));
        if (!m_adblock->decide(url, firstParty, adblockRequestType(type)))
            return RV_CONTINUE;
        AdblockService* adblock = m_adblock;
        engine::onQt([adblock, firstParty] { adblock->recordBlocked(firstParty); });
        return RV_CANCEL;
    }

private:
    AdblockService* m_adblock; // vive tanto como la aplicación
    IMPLEMENT_REFCOUNTING(RequestHandler);
};

} // namespace

QString clearCacheMarker()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/cef/borrar-cache");
}

Profile::Profile(bool incognito, AdblockService* adblock, QObject* parent)
    : QObject(parent)
    , m_incognito(incognito)
    , m_adblock(adblock)
{
}

CefRefPtr<CefRequestContext> Profile::context()
{
    // Se crea al usarlo por primera vez (la primera pestaña): así CEF puede arrancar después de
    // mostrar la ventana.
    if (m_context)
        return m_context;
    CefRefPtr<RequestHandler> handler = new RequestHandler(m_adblock);
    if (m_incognito) {
        CefRequestContextSettings settings; // sin cache_path: todo en memoria
        m_context = CefRequestContext::CreateContext(settings, handler);
    } else {
        // El perfil global (cache_path de CefSettings) con nuestro manejador: ahí cargan las extensiones.
        m_context = CefRequestContext::CreateContext(CefRequestContext::GetGlobalContext(), handler);
    }
    // Cookies de terceros bloqueadas, como en Brave.
    CefRefPtr<CefRequestContext> context = m_context;
    engine::onUi([context] {
        CefString error;
        CefRefPtr<CefValue> value = CefValue::Create();
        value->SetInt(1);
        context->SetPreference("profile.cookie_controls_mode", value, error);
    });
    for (const auto& hook : m_onContext)
        hook(m_context);
    return m_context;
}
Profile::~Profile() = default;

void Profile::clearCookies()
{
    if (CefRefPtr<CefCookieManager> cookies = context()->GetCookieManager(nullptr))
        cookies->DeleteCookies("", "", nullptr);
}

void Profile::clearCache()
{
    if (m_incognito)
        return;
    QFile marker(clearCacheMarker());
    if (marker.open(QIODevice::WriteOnly))
        marker.write("1");
}

} // namespace sora
