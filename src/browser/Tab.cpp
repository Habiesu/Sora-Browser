#include "browser/Tab.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QKeySequence>
#include <QPixmap>
#include <QShortcut>
#include <atomic>
#include <cmath>
#include <map>

#include "include/cef_client.h"
#include "include/cef_parser.h"

#include "browser/DownloadManager.h"
#include "browser/Engine.h"
#include "browser/Profile.h"
#include "privacy/AdblockService.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace sora {
namespace {

QString qstr(const CefString& s) { return QString::fromStdWString(s.ToWString()); }
QUrl qurl(const CefString& s) { return QUrl(qstr(s)); }

enum MenuCommand { OpenLinkInNewTab = MENU_ID_USER_FIRST, CopyLinkAddress };

// Tecla de Windows + modificadores de CEF → combinación de Qt (para los atajos de la ventana).
int keyCombination(const CefKeyEvent& e)
{
    int key = 0;
    const int vk = e.windows_key_code;
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
        key = vk; // Qt::Key_A.. y Qt::Key_0.. coinciden con los códigos de Windows
#ifdef Q_OS_WIN
    else if (vk >= VK_F1 && vk <= VK_F24)
        key = Qt::Key_F1 + (vk - VK_F1);
    else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
        key = Qt::Key_0 + (vk - VK_NUMPAD0);
    else
        switch (vk) {
        case VK_TAB: key = Qt::Key_Tab; break;
        case VK_ESCAPE: key = Qt::Key_Escape; break;
        case VK_PRIOR: key = Qt::Key_PageUp; break;
        case VK_NEXT: key = Qt::Key_PageDown; break;
        case VK_LEFT: key = Qt::Key_Left; break;
        case VK_RIGHT: key = Qt::Key_Right; break;
        case VK_DELETE: key = Qt::Key_Delete; break;
        case VK_OEM_PLUS: case VK_ADD: key = Qt::Key_Plus; break; // "+" en teclado español, "=" en inglés
        case VK_OEM_MINUS: case VK_SUBTRACT: key = Qt::Key_Minus; break;
        default: return 0;
        }
#endif
    if (e.modifiers & EVENTFLAG_CONTROL_DOWN) key |= Qt::ControlModifier;
    if (e.modifiers & EVENTFLAG_SHIFT_DOWN) key |= Qt::ShiftModifier;
    if (e.modifiers & EVENTFLAG_ALT_DOWN) key |= Qt::AltModifier;
    return key;
}

// Ventanas que abre CEF por su cuenta (emergentes con opener, como los inicios de sesión, y las
// herramientas para desarrolladores): solo se cuentan para apagar CEF a tiempo.
class PopupClient : public CefClient, public CefLifeSpanHandler {
public:
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    void OnAfterCreated(CefRefPtr<CefBrowser>) override { engine::browserCreated(); }
    void OnBeforeClose(CefRefPtr<CefBrowser>) override { engine::browserClosed(); }

private:
    IMPLEMENT_REFCOUNTING(PopupClient);
};

class ImageCallback : public CefDownloadImageCallback {
public:
    explicit ImageCallback(std::function<void(QByteArray)> done) : m_done(std::move(done)) {}
    void OnDownloadImageFinished(const CefString&, int, CefRefPtr<CefImage> image) override
    {
        int w = 0, h = 0;
        CefRefPtr<CefBinaryValue> png = image ? image->GetAsPNG(1.0f, true, w, h) : nullptr;
        QByteArray bytes;
        if (png) {
            bytes.resize(qsizetype(png->GetSize()));
            png->GetData(bytes.data(), png->GetSize(), 0);
        }
        m_done(bytes);
    }

private:
    std::function<void(QByteArray)> m_done;
    IMPLEMENT_REFCOUNTING(ImageCallback);
};

} // namespace

// Recibe los avisos de CEF (en su hilo UI o IO) y los pasa a la pestaña por la cola de Qt.
// m_tab solo se toca en el hilo de Qt; lo demás (m_browserUi, scripts, ids) solo en el de CEF.
class Tab::Client : public CefClient,
                    public CefLifeSpanHandler,
                    public CefLoadHandler,
                    public CefDisplayHandler,
                    public CefRequestHandler,
                    public CefFindHandler,
                    public CefFocusHandler,
                    public CefKeyboardHandler,
                    public CefDownloadHandler,
                    public CefContextMenuHandler,
                    public CefDevToolsMessageObserver {
public:
    Client(Tab* tab, Profile* profile) : m_tab(tab), m_profile(profile) {}

    QPointer<Tab> m_tab;
    Profile* m_profile; // vive tanto como sus ventanas
    std::atomic<bool> closing{false}; // la pestaña ya no existe: cerrar el navegador en cuanto nazca
    std::atomic<bool> loading{false};
    std::string youtubeScript; // hilo de CEF

    // Corre f en el hilo de Qt si la pestaña sigue viva.
    void post(std::function<void(Tab*)> f)
    {
        CefRefPtr<Client> self(this);
        engine::onQt([self, f = std::move(f)] {
            if (Tab* t = self->m_tab)
                f(t);
        });
    }

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefFindHandler> GetFindHandler() override { return this; }
    CefRefPtr<CefFocusHandler> GetFocusHandler() override { return this; }
    CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }
    CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source,
                                  CefRefPtr<CefProcessMessage> message) override
    {
        return engine::router() && engine::router()->OnProcessMessageReceived(browser, frame, source, message);
    }

    // --- Vida del navegador ---
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override
    {
        engine::browserCreated();
        m_browserUi = browser;
        m_devtools = browser->GetHost()->AddDevToolsMessageObserver(this);
        applyYoutubeScript();
        if (closing) { // la pestaña se cerró antes de que el navegador existiera
            browser->GetHost()->CloseBrowser(true);
            return;
        }
        void* window = browser->GetHost()->GetWindowHandle();
        post([browser, window](Tab* t) {
            t->m_browser = browser;
            t->m_browserWindow = window;
            t->fitBrowser();
            if (t->m_zoom != 1.0)
                t->setZoomFactor(t->m_zoom);
            if (t->m_pendingUrl.isValid())
                t->setUrl(std::exchange(t->m_pendingUrl, QUrl()));
            if (t->hasFocus())
                browser->GetHost()->SetFocus(true);
        });
    }

    bool DoClose(CefRefPtr<CefBrowser> browser) override
    {
        // Cerrar una pestaña no cierra la ventana: solo se destruye la ventana del navegador (es de
        // este hilo). Si lo pidió la página (window.close), además se cierra la pestaña.
        if (!closing)
            post([](Tab* t) { emit t->windowCloseRequested(); });
#ifdef Q_OS_WIN
        DestroyWindow(browser->GetHost()->GetWindowHandle());
#endif
        return true;
    }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override
    {
        if (engine::router())
            engine::router()->OnBeforeClose(browser);
        m_devtools = nullptr;
        m_browserUi = nullptr;
        engine::browserClosed();
        post([](Tab* t) {
            t->m_browser = nullptr;
            t->m_browserWindow = nullptr;
        });
    }

    bool OnBeforePopup(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int, const CefString& targetUrl, const CefString&,
                       cef_window_open_disposition_t disposition, bool, const CefPopupFeatures&, CefWindowInfo&,
                       CefRefPtr<CefClient>& client, CefBrowserSettings&, CefRefPtr<CefDictionaryValue>&, bool*) override
    {
        // Ventana emergente de verdad (window.open con tamaño: inicios de sesión): la crea CEF y
        // conserva window.opener. Lo demás (enlaces a pestaña nueva) va a una pestaña de Sora.
        if (disposition == CEF_WOD_NEW_POPUP) {
            client = new PopupClient;
            return false;
        }
        const QUrl url = qurl(targetUrl);
        const bool background = disposition == CEF_WOD_NEW_BACKGROUND_TAB;
        post([url, background](Tab* t) {
            if (t->onCreateWindow)
                t->onCreateWindow(url, background);
        });
        return true;
    }

    // --- Carga ---
    void OnLoadingStateChange(CefRefPtr<CefBrowser>, bool isLoading, bool canGoBack, bool canGoForward) override
    {
        loading = isLoading;
        post([isLoading, canGoBack, canGoForward](Tab* t) {
            const bool was = t->m_loading;
            t->m_loading = isLoading;
            t->m_canGoBack = canGoBack;
            t->m_canGoForward = canGoForward;
            if (isLoading && !was) {
                t->m_blocked = 0;
                emit t->blockedCountChanged(0);
                emit t->loadStarted();
            } else if (!isLoading && was) {
                emit t->loadFinished(!t->m_failed);
            }
        });
    }

    void OnLoadStart(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, TransitionType) override
    {
        if (frame->IsMain())
            post([](Tab* t) { t->m_failed = false; });
    }

    void OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, ErrorCode code, const CefString&,
                     const CefString&) override
    {
        if (frame->IsMain() && code != ERR_ABORTED)
            post([](Tab* t) { t->m_failed = true; });
    }

    // --- Lo que se ve ---
    void OnAddressChange(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, const CefString& url) override
    {
        if (!frame->IsMain())
            return;
        const QUrl u = qurl(url);
        post([u](Tab* t) {
            if (t->m_url == u)
                return;
            t->m_url = u;
            emit t->urlChanged(u);
        });
    }

    void OnTitleChange(CefRefPtr<CefBrowser>, const CefString& title) override
    {
        const QString s = qstr(title);
        post([s](Tab* t) {
            t->m_title = s;
            emit t->titleChanged(s);
        });
    }

    void OnFaviconURLChange(CefRefPtr<CefBrowser> browser, const std::vector<CefString>& urls) override
    {
        if (urls.empty())
            return;
        CefRefPtr<Client> self(this);
        browser->GetHost()->DownloadImage(urls.front(), true, 32, false, new ImageCallback([self](QByteArray png) {
            self->post([png](Tab* t) {
                QPixmap pixmap;
                if (!png.isEmpty() && pixmap.loadFromData(png, "PNG")) {
                    t->m_icon = QIcon(pixmap);
                    emit t->iconChanged(t->m_icon);
                }
            });
        }));
    }

    void OnFullscreenModeChange(CefRefPtr<CefBrowser>, bool fullscreen) override
    {
        post([fullscreen](Tab* t) { emit t->fullScreenRequested(fullscreen); });
    }

    void OnStatusMessage(CefRefPtr<CefBrowser>, const CefString& value) override
    {
        const QUrl url = qurl(value);
        post([url](Tab* t) { emit t->linkHovered(url); });
    }

    // --- Navegación y puente sora:// ---
    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest>, bool,
                        bool) override
    {
        if (engine::router())
            engine::router()->OnBeforeBrowse(browser, frame);
        return false;
    }

    void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser, TerminationStatus, int, const CefString&) override
    {
        if (engine::router())
            engine::router()->OnRenderProcessTerminated(browser);
    }

    // --- Buscar, foco y teclado ---
    void OnFindResult(CefRefPtr<CefBrowser>, int, int count, const CefRect&, int active, bool) override
    {
        post([active, count](Tab* t) { emit t->findResult(active, count); });
    }

    void OnGotFocus(CefRefPtr<CefBrowser>) override
    {
        post([](Tab* t) {
            if (!t->hasFocus())
                t->setFocus(Qt::MouseFocusReason);
        });
    }

    bool OnPreKeyEvent(CefRefPtr<CefBrowser>, const CefKeyEvent& event, CefEventHandle, bool*) override
    {
        // Con la web enfocada Qt no ve las teclas: los atajos de la ventana se le pasan aquí.
        if (event.type != KEYEVENT_RAWKEYDOWN)
            return false;
        const int combo = keyCombination(event);
        if (!combo || !engine::isShortcut(combo))
            return false;
        if (combo == Qt::Key_Escape && !loading)
            return false; // Esc es de la página salvo para parar una carga
        post([combo](Tab* t) { t->triggerShortcut(combo); });
        return true;
    }

    // --- Descargas ---
    bool OnBeforeDownload(CefRefPtr<CefBrowser>, CefRefPtr<CefDownloadItem> item, const CefString& suggestedName,
                          CefRefPtr<CefBeforeDownloadCallback> callback) override
    {
        const uint32_t id = item->GetId();
        const QString name = qstr(suggestedName);
        const QUrl url = qurl(item->GetURL());
        Profile* profile = m_profile;
        engine::onQt([profile, id, name, url, callback] {
            DownloadManager* downloads = profile->downloads();
            const QString path = downloads ? downloads->begin(id, name, url, profile->isOffTheRecord()) : QString();
            if (!path.isEmpty())
                callback->Continue(path.toStdWString(), false);
        });
        return true;
    }

    void OnDownloadUpdated(CefRefPtr<CefBrowser>, CefRefPtr<CefDownloadItem> item,
                           CefRefPtr<CefDownloadItemCallback> callback) override
    {
        DownloadManager::Update u;
        u.id = item->GetId();
        u.received = item->GetReceivedBytes();
        u.total = item->GetTotalBytes();
        u.path = qstr(item->GetFullPath());
        u.state = item->IsComplete()      ? DownloadManager::Done
                  : item->IsCanceled()    ? DownloadManager::Cancelled
                  : item->IsInterrupted() ? DownloadManager::Failed
                                          : DownloadManager::Progress;
        u.callback = callback;
        Profile* profile = m_profile;
        engine::onQt([profile, u] {
            if (DownloadManager* downloads = profile->downloads())
                downloads->update(u);
        });
    }

    // --- Menú contextual (CEF lo trae en español con locale=es) ---
    void OnBeforeContextMenu(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefContextMenuParams> params,
                             CefRefPtr<CefMenuModel> model) override
    {
        if (params->GetLinkUrl().empty())
            return;
        model->InsertItemAt(0, OpenLinkInNewTab, "Abrir enlace en una pestaña nueva");
        model->InsertItemAt(1, CopyLinkAddress, "Copiar dirección del enlace");
        model->InsertSeparatorAt(2);
    }

    bool OnContextMenuCommand(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefContextMenuParams> params,
                              int command, EventFlags) override
    {
        const QUrl link = qurl(params->GetLinkUrl());
        if (command == OpenLinkInNewTab) {
            post([link](Tab* t) {
                if (t->onCreateWindow)
                    t->onCreateWindow(link, true);
            });
            return true;
        }
        if (command == CopyLinkAddress) {
            engine::onQt([link] { QApplication::clipboard()->setText(link.toString()); });
            return true;
        }
        return false;
    }

    // --- Protocolo de DevTools (script de YouTube, guardar página) ---
    void applyYoutubeScript() // hilo de CEF
    {
        if (!m_browserUi)
            return;
        CefRefPtr<CefBrowserHost> host = m_browserUi->GetHost();
        if (!m_youtubeId.empty()) {
            CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
            params->SetString("identifier", m_youtubeId);
            host->ExecuteDevToolsMethod(++m_nextMessage, "Page.removeScriptToEvaluateOnNewDocument", params);
            m_youtubeId.clear();
        }
        if (youtubeScript.empty())
            return;
        CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
        params->SetString("source", youtubeScript);
        m_youtubeMessage = ++m_nextMessage;
        host->ExecuteDevToolsMethod(m_youtubeMessage, "Page.addScriptToEvaluateOnNewDocument", params);
    }

    void savePage(const QString& path) // hilo de CEF
    {
        if (!m_browserUi)
            return;
        CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
        params->SetString("format", "mhtml");
        const int id = ++m_nextMessage;
        m_saves[id] = path;
        m_browserUi->GetHost()->ExecuteDevToolsMethod(id, "Page.captureSnapshot", params);
    }

    void OnDevToolsMethodResult(CefRefPtr<CefBrowser>, int id, bool success, const void* result, size_t size) override
    {
        CefRefPtr<CefValue> value = success ? CefParseJSON(result, size, JSON_PARSER_RFC) : nullptr;
        CefRefPtr<CefDictionaryValue> dict = value ? value->GetDictionary() : nullptr;
        if (id == m_youtubeMessage) {
            m_youtubeId = dict ? dict->GetString("identifier").ToString() : std::string();
            return;
        }
        const auto save = m_saves.find(id);
        if (save == m_saves.end())
            return;
        const QString path = save->second;
        m_saves.erase(save);
        const QByteArray data = dict ? QByteArray::fromStdString(dict->GetString("data").ToString()) : QByteArray();
        engine::onQt([path, data] {
            QFile file(path);
            if (!data.isEmpty() && file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                file.write(data);
        });
    }

    CefRefPtr<CefBrowser> m_browserUi;

private:
    CefRefPtr<CefRegistration> m_devtools;
    std::string m_youtubeId;
    int m_youtubeMessage = 0;
    int m_nextMessage = 1000;
    std::map<int, QString> m_saves;
    IMPLEMENT_REFCOUNTING(Client);
};

Tab::Tab(Profile* profile, QWidget* parent)
    : QWidget(parent)
    , m_profile(profile)
    , m_client(new Client(this, profile))
{
    setAttribute(Qt::WA_NativeWindow); // CEF necesita un HWND padre
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    if (AdblockService* adblock = profile->adblock())
        connect(adblock, &AdblockService::youtubeScriptChanged, this, &Tab::updateYoutubeScript);
}

Tab::~Tab()
{
    m_client->m_tab = nullptr;
    m_client->closing = true;
#ifdef Q_OS_WIN
    // El HWND de esta pestaña se destruye ahora: el del navegador (de otro hilo) sale antes de él.
    if (HWND browserWindow = static_cast<HWND>(m_browserWindow)) {
        ShowWindow(browserWindow, SW_HIDE);
        SetParent(browserWindow, nullptr);
    }
#endif
    if (m_browser)
        m_browser->GetHost()->CloseBrowser(true);
}

void Tab::createBrowser(const QUrl& url)
{
    CefWindowInfo info;
    const qreal dpr = devicePixelRatioF();
    const CefRect rect(0, 0, qMax(1, qRound(width() * dpr)), qMax(1, qRound(height() * dpr)));
#ifdef Q_OS_WIN
    info.SetAsChild(reinterpret_cast<HWND>(winId()), rect);
#endif
    info.runtime_style = CEF_RUNTIME_STYLE_ALLOY; // la interfaz es la de Sora
    AdblockService* adblock = m_profile->adblock();
    m_client->youtubeScript = adblock ? adblock->youtubeScript().toStdString() : std::string();
    CefBrowserSettings settings;
    CefBrowserHost::CreateBrowser(info, m_client, url.toString().toStdWString(), settings, nullptr, m_profile->context());
}

void Tab::setUrl(const QUrl& url)
{
    if (!url.isValid())
        return;
    if (m_browser) {
        m_browser->GetMainFrame()->LoadURL(url.toString().toStdWString());
        return;
    }
    if (!m_creating) {
        m_creating = true;
        m_url = url;
        createBrowser(url);
        emit urlChanged(url);
        return;
    }
    m_pendingUrl = url; // el navegador está naciendo: se carga en cuanto exista
}

void Tab::back() { if (m_browser) m_browser->GoBack(); }
void Tab::forward() { if (m_browser) m_browser->GoForward(); }
void Tab::reload() { if (m_browser) m_browser->Reload(); }
void Tab::stop() { if (m_browser) m_browser->StopLoad(); }

void Tab::setZoomFactor(double factor)
{
    m_zoom = factor;
    if (m_browser)
        m_browser->GetHost()->SetZoomLevel(std::log(factor) / std::log(1.2)); // Chromium: factor = 1,2^nivel
}

void Tab::findText(const QString& text, bool backward)
{
    if (!m_browser)
        return;
    if (text.isEmpty()) {
        m_findText.clear();
        m_browser->GetHost()->StopFinding(true);
        return;
    }
    const bool next = text == m_findText;
    m_findText = text;
    m_browser->GetHost()->Find(text.toStdWString(), !backward, false, next);
}

void Tab::print()
{
    if (m_browser)
        m_browser->GetHost()->Print();
}

void Tab::showDevTools()
{
    if (!m_browser)
        return;
    CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
    const QString title = tr("Herramientas para desarrolladores — %1").arg(m_title);
    engine::onUi([host, title] {
        CefWindowInfo info;
#ifdef Q_OS_WIN
        info.SetAsPopup(nullptr, title.toStdWString());
#endif
        host->ShowDevTools(info, new PopupClient, CefBrowserSettings(), CefPoint());
    });
}

void Tab::exitFullScreen()
{
    if (!m_browser)
        return;
    CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
    engine::onUi([host] { host->ExitFullscreen(true); });
}

void Tab::savePage(const QString& path)
{
    CefRefPtr<Client> client = m_client;
    engine::onUi([client, path] { client->savePage(path); });
}

void Tab::executeJavaScript(const QString& code)
{
    if (m_browser)
        m_browser->GetMainFrame()->ExecuteJavaScript(code.toStdWString(), m_browser->GetMainFrame()->GetURL(), 0);
}

void Tab::countBlocked(int count)
{
    m_blocked += count;
    emit blockedCountChanged(m_blocked);
}

void Tab::updateYoutubeScript()
{
    AdblockService* adblock = m_profile->adblock();
    const std::string script = adblock ? adblock->youtubeScript().toStdString() : std::string();
    CefRefPtr<Client> client = m_client;
    engine::onUi([client, script] {
        client->youtubeScript = script;
        client->applyYoutubeScript();
    });
}

void Tab::triggerShortcut(int keyCombination)
{
    const QKeySequence keys(keyCombination);
    for (QShortcut* shortcut : window()->findChildren<QShortcut*>())
        if (shortcut->isEnabled() && shortcut->keys().contains(keys)) {
            emit shortcut->activated();
            return;
        }
}

void Tab::fitBrowser()
{
#ifdef Q_OS_WIN
    if (HWND browserWindow = static_cast<HWND>(m_browserWindow)) {
        const qreal dpr = devicePixelRatioF();
        SetWindowPos(browserWindow, nullptr, 0, 0, qRound(width() * dpr), qRound(height() * dpr),
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
    }
#endif
}

void Tab::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    fitBrowser();
}

void Tab::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    if (m_browser)
        m_browser->GetHost()->SetFocus(true); // el teclado va a la web, no al HWND de Qt
}

} // namespace sora
