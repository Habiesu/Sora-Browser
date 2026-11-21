#include "engine/EngineApp.h"

namespace sora {

CefMessageRouterConfig messageRouterConfig()
{
    return CefMessageRouterConfig(); // window.cefQuery / window.cefQueryCancel
}

void EngineApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar)
{
    // Estándar (con host: sora://settings) y seguro. LOCAL: las webs normales no pueden enlazar,
    // navegar ni incrustar páginas sora:// (como con Qt WebEngine).
    registrar->AddCustomScheme("sora", CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE | CEF_SCHEME_OPTION_LOCAL);
}

void EngineApp::OnBeforeCommandLineProcessing(const CefString& processType, CefRefPtr<CefCommandLine> commandLine)
{
    if (!processType.empty())
        return;
    for (const auto& [name, value] : switches)
        value.empty() ? commandLine->AppendSwitch(name) : commandLine->AppendSwitchWithValue(name, value);
}

void EngineApp::OnWebKitInitialized()
{
    m_router = CefMessageRouterRendererSide::Create(messageRouterConfig());
}

void EngineApp::OnBrowserCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDictionaryValue> extraInfo)
{
    if (extraInfo && extraInfo->HasKey("youtube"))
        m_youtube[browser->GetIdentifier()] = extraInfo->GetString("youtube").ToString();
}

void EngineApp::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser)
{
    m_youtube.erase(browser->GetIdentifier());
}

void EngineApp::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context)
{
    // Antes que cualquier script de la página, en su mismo mundo (como DocumentCreation de Qt).
    if (frame->IsMain()) {
        const auto it = m_youtube.find(browser->GetIdentifier());
        const std::string url = frame->GetURL().ToString();
        if (it != m_youtube.end() && !it->second.empty() && url.find("youtube.com") != std::string::npos) {
            CefRefPtr<CefV8Value> result;
            CefRefPtr<CefV8Exception> exception;
            context->Eval(it->second, frame->GetURL(), 0, result, exception);
        }
    }
    // El puente solo existe en páginas sora:// (el navegador además lo comprueba en cada consulta).
    if (m_router && frame->GetURL().ToString().rfind("sora://", 0) == 0)
        m_router->OnContextCreated(browser, frame, context);
}

void EngineApp::OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context)
{
    if (m_router)
        m_router->OnContextReleased(browser, frame, context);
}

bool EngineApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source,
                                         CefRefPtr<CefProcessMessage> message)
{
    if (message->GetName() == kYoutubeScriptMessage) {
        m_youtube[browser->GetIdentifier()] = message->GetArgumentList()->GetString(0).ToString();
        return true;
    }
    return m_router && m_router->OnProcessMessageReceived(browser, frame, source, message);
}

} // namespace sora
