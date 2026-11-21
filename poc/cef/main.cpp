// Prueba de concepto: ventana Qt Widgets + una pestaña CEF.
// sora-cef [--bench] [--chrome-style] [url]
#include <QApplication>
#include <QCloseEvent>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QMetaObject>
#include <QStandardPaths>
#include <QDir>
#include <cstdio>
#include <windows.h>

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"

extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace {

QElapsedTimer g_sinceStart;
qint64 g_windowShownMs = 0;
bool g_bench = false;
void mark(const char* what)
{
    if (qEnvironmentVariableIsSet("SORA_TRACE"))
        std::fprintf(stderr, "%6lld ms  %s\n", (long long)g_sinceStart.elapsed(), what);
}

// Pasa una función al hilo de Qt (CEF llama desde su propio hilo UI).
template <typename F> void onQt(F f) { QMetaObject::invokeMethod(qApp, std::move(f), Qt::QueuedConnection); }

class Client : public CefClient, public CefLifeSpanHandler, public CefLoadHandler, public CefDisplayHandler {
public:
    explicit Client(QWidget* host) : m_host(host) {}
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override { m_browser = browser; }
    // Cerrar una pestaña no cierra la ventana: se destruye solo la ventana hija de CEF (es de su hilo).
    bool DoClose(CefRefPtr<CefBrowser> browser) override
    {
        DestroyWindow(browser->GetHost()->GetWindowHandle());
        return true;
    }
    void OnBeforeClose(CefRefPtr<CefBrowser>) override
    {
        mark("OnBeforeClose");
        m_browser = nullptr;
        onQt([] { QApplication::quit(); });
    }
    void OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title) override
    {
        const QString t = QString::fromStdWString(title.ToWString());
        QWidget* w = m_host->window();
        onQt([w, t] { w->setWindowTitle(t + QStringLiteral(" — Sora (CEF)")); });
    }
    void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int) override
    {
        if (!g_bench || !frame->IsMain() || m_reported)
            return;
        m_reported = true;
        mark("OnLoadEnd");
        std::printf("{\"window_shown_ms\":%lld,\"first_load_ms\":%lld}\n", (long long)g_windowShownMs,
                    (long long)g_sinceStart.elapsed());
        std::fflush(stdout);
        browser->GetHost()->CloseBrowser(true);
    }

    CefRefPtr<CefBrowser> browser() const { return m_browser; }

private:
    QWidget* m_host;
    CefRefPtr<CefBrowser> m_browser;
    bool m_reported = false;
    IMPLEMENT_REFCOUNTING(Client);
};

// Hueco nativo donde CEF pinta; mantiene al navegador del tamaño del widget.
class TabHost : public QWidget {
public:
    TabHost()
    {
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_NoSystemBackground);
    }
    CefRefPtr<Client> client;

protected:
    void resizeEvent(QResizeEvent*) override
    {
        if (HWND child = GetWindow(reinterpret_cast<HWND>(winId()), GW_CHILD)) {
            const qreal dpr = devicePixelRatioF();
            SetWindowPos(child, nullptr, 0, 0, int(width() * dpr), int(height() * dpr), SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
};

class Window : public QMainWindow {
public:
    TabHost* host = new TabHost;
    Window() { setCentralWidget(host); }

protected:
    void closeEvent(QCloseEvent* e) override
    {
        // CEF cierra el navegador y luego avisa (OnBeforeClose) para salir.
        if (auto b = host->client ? host->client->browser() : nullptr) {
            b->GetHost()->CloseBrowser(true);
            e->ignore();
            return;
        }
        QMainWindow::closeEvent(e);
    }
};

} // namespace

int main(int argc, char* argv[])
{
    g_sinceStart.start();
    CefMainArgs mainArgs(GetModuleHandle(nullptr));

    QApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);
    g_bench = args.removeAll(QStringLiteral("--bench")) > 0;
    const bool chromeStyle = args.removeAll(QStringLiteral("--chrome-style")) > 0;
    args.removeIf([](const QString& a) { return a.startsWith(QStringLiteral("--")); }); // interruptores de Chromium: CEF los lee solo
    const QString url = args.value(0, QStringLiteral("https://www.youtube.com/"));

    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/SoraCef");
    QDir().mkpath(cache);

    CefSettings settings;
    settings.no_sandbox = true; // shortcut: sin sandbox en la prueba; la tarea 2 decide si enlaza cef_sandbox.lib
    settings.multi_threaded_message_loop = true;
    CefString(&settings.browser_subprocess_path) = QDir::toNativeSeparators(exeDir + QStringLiteral("/sora-helper.exe")).toStdWString();
    CefString(&settings.root_cache_path) = QDir::toNativeSeparators(cache).toStdWString();
    CefString(&settings.cache_path) = QDir::toNativeSeparators(cache + QStringLiteral("/Default")).toStdWString();
    CefString(&settings.locale) = "es";
    mark("antes de CefInitialize");
    if (!CefInitialize(mainArgs, settings, nullptr, nullptr)) {
        std::fprintf(stderr, "CefInitialize falló (%d)\n", CefGetExitCode());
        return 1;
    }

    mark("CefInitialize");
    Window window;
    window.resize(1280, 800);
    window.show();
    QApplication::processEvents();
    g_windowShownMs = g_sinceStart.elapsed();

    window.host->client = new Client(window.host);
    CefWindowInfo info;
    const qreal dpr = window.host->devicePixelRatioF();
    info.SetAsChild(reinterpret_cast<HWND>(window.host->winId()),
                    CefRect(0, 0, int(window.host->width() * dpr), int(window.host->height() * dpr)));
    info.runtime_style = chromeStyle ? CEF_RUNTIME_STYLE_CHROME : CEF_RUNTIME_STYLE_ALLOY;
    CefBrowserSettings browserSettings;
    CefBrowserHost::CreateBrowser(info, window.host->client, url.toStdWString(), browserSettings, nullptr, nullptr);

    const int code = app.exec();
    mark("fin de exec");
    window.host->client = nullptr;
    CefShutdown();
    mark("CefShutdown");
    return code;
}
