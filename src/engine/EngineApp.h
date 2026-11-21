#pragma once
// Parte de CEF que comparten sora.exe y sora-helper.exe (sin Qt): el esquema sora:// tiene que
// registrarse igual en todos los procesos, y el renderer pone window.cefQuery para el puente.
#include <string>
#include <utility>
#include <vector>

#include "include/cef_app.h"
#include "include/wrapper/cef_message_router.h"

namespace sora {

// Nombres de window.cefQuery/cefQueryCancel: iguales en el navegador y en el renderer.
CefMessageRouterConfig messageRouterConfig();

class EngineApp : public CefApp, public CefBrowserProcessHandler, public CefRenderProcessHandler {
public:
    // Interruptores de Chromium del proceso del navegador (antes de CefInitialize).
    std::vector<std::pair<std::string, std::string>> switches;

    void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
    void OnBeforeCommandLineProcessing(const CefString& processType, CefRefPtr<CefCommandLine> commandLine) override;
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

    // Renderer
    void OnWebKitInitialized() override;
    void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;
    void OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source,
                                  CefRefPtr<CefProcessMessage> message) override;

private:
    CefRefPtr<CefMessageRouterRendererSide> m_router;
    IMPLEMENT_REFCOUNTING(EngineApp);
};

} // namespace sora
