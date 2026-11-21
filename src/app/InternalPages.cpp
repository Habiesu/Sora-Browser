#include "app/InternalPages.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaMethod>
#include <QPointer>
#include <algorithm>
#include <QUrl>
#include <atomic>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>

#include "include/cef_scheme.h"
#include "include/wrapper/cef_stream_resource_handler.h"

#include "app/InternalApi.h"
#include "app/Settings.h"
#include "browser/Engine.h"
#include "browser/Profile.h"

namespace sora {
namespace {

// sora://<página>[/<variante>] → :/pages/<página>[-<variante>].html ; sora://res/<archivo> → :/pages/res/<archivo>
QString resourceFor(const QUrl& url)
{
    const QString host = url.host();
    QString path = url.path();
    if (path.contains(QLatin1String("..")))
        return {};
    if (host == QLatin1String("res"))
        return QStringLiteral(":/pages/res") + path;
    if (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    const QString variant = path.isEmpty() ? QString() : QLatin1Char('-') + path.mid(1);
    return QStringLiteral(":/pages/") + host + variant + QStringLiteral(".html");
}

const char* mimeFor(const QString& resource)
{
    if (resource.endsWith(QLatin1String(".html"))) return "text/html";
    if (resource.endsWith(QLatin1String(".js"))) return "text/javascript";
    if (resource.endsWith(QLatin1String(".css"))) return "text/css";
    if (resource.endsWith(QLatin1String(".svg"))) return "image/svg+xml";
    return "application/octet-stream";
}

// El buscador elegido en Ajustes para la nueva pestaña. Se lee desde el hilo IO de CEF.
struct Search {
    QByteArray action, name;
};
std::atomic<std::shared_ptr<const Search>> s_search;

void refreshSearch()
{
    const Settings& s = Settings::instance();
    s_search.store(std::make_shared<const Search>(Search{s.searchAction().toUtf8(), s.searchName().toUtf8()}));
}

class BytesReader : public CefReadHandler {
public:
    explicit BytesReader(QByteArray data) : m_data(std::move(data)) {}
    size_t Read(void* ptr, size_t size, size_t n) override
    {
        const size_t bytes = std::min<size_t>(size * n, size_t(m_data.size() - m_pos));
        std::memcpy(ptr, m_data.constData() + m_pos, bytes);
        m_pos += qsizetype(bytes);
        return size ? bytes / size : 0;
    }
    int Seek(int64_t offset, int whence) override
    {
        const qint64 base = whence == SEEK_CUR ? m_pos : whence == SEEK_END ? m_data.size() : 0;
        if (base + offset < 0 || base + offset > m_data.size())
            return -1;
        m_pos = qsizetype(base + offset);
        return 0;
    }
    int64_t Tell() override { return m_pos; }
    int Eof() override { return m_pos >= m_data.size(); }
    bool MayBlock() override { return false; }

private:
    QByteArray m_data;
    qsizetype m_pos = 0;
    IMPLEMENT_REFCOUNTING(BytesReader);
};

// Corre en el hilo IO de CEF: solo lee recursos de Qt (memoria estática) y el buscador atómico.
class SchemeFactory : public CefSchemeHandlerFactory {
public:
    CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString&,
                                         CefRefPtr<CefRequest> request) override
    {
        const QString resource = resourceFor(QUrl(QString::fromStdString(request->GetURL().ToString())));
        QFile file(resource);
        if (resource.isEmpty() || !file.open(QIODevice::ReadOnly))
            return new CefStreamResourceHandler(404, "Not Found", "text/plain", {},
                                                CefStreamReader::CreateForHandler(new BytesReader({})));
        QByteArray data = file.readAll();
        if (resource.startsWith(QLatin1String(":/pages/newtab")))
            if (const auto search = s_search.load()) {
                data.replace("https://duckduckgo.com/", search->action);
                data.replace("DuckDuckGo", search->name);
            }
        CefResponse::HeaderMap headers;
        headers.insert(std::make_pair(CefString("Cache-Control"), CefString("no-store")));
        return new CefStreamResourceHandler(200, "OK", mimeFor(resource), headers,
                                            CefStreamReader::CreateForHandler(new BytesReader(std::move(data))));
    }

private:
    IMPLEMENT_REFCOUNTING(SchemeFactory);
};

QPointer<InternalApi> s_api;

// window.cefQuery de las páginas sora://: {method, args} → [resultado]; {subscribe: señal} → aviso
// (persistente) cada vez que la señal se emite.
class QueryHandler : public CefMessageRouterBrowserSide::Handler {
public:
    bool OnQuery(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int64_t queryId, const CefString& request,
                 bool persistent, CefRefPtr<Callback> callback) override
    {
        if (frame->GetURL().ToString().rfind("sora://", 0) != 0)
            return false; // solo páginas de Sora
        const QJsonObject q = QJsonDocument::fromJson(QByteArray::fromStdString(request.ToString())).object();
        if (const QString signal = q.value(QStringLiteral("subscribe")).toString(); !signal.isEmpty()) {
            if (!persistent)
                return false;
            std::lock_guard lock(m_mutex);
            m_subscribers[queryId] = {signal, callback};
            return true;
        }
        const QString method = q.value(QStringLiteral("method")).toString();
        const QJsonArray args = q.value(QStringLiteral("args")).toArray();
        engine::onQt([method, args, callback] {
            if (!s_api) {
                callback->Failure(1, "Sora aún no está listo");
                return;
            }
            bool ok = false;
            const QVariant result = s_api->call(method, args, &ok);
            if (!ok) {
                callback->Failure(2, "Método desconocido");
                return;
            }
            const QByteArray json = QJsonDocument(QJsonArray{QJsonValue::fromVariant(result)}).toJson(QJsonDocument::Compact);
            callback->Success(json.toStdString());
        });
        return true;
    }

    void OnQueryCanceled(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int64_t queryId) override
    {
        std::lock_guard lock(m_mutex);
        m_subscribers.erase(queryId);
    }

    void notify(const QString& signal)
    {
        std::lock_guard lock(m_mutex);
        for (const auto& [id, s] : m_subscribers)
            if (s.signal == signal)
                s.callback->Success("");
    }

private:
    struct Subscriber {
        QString signal;
        CefRefPtr<Callback> callback;
    };
    std::mutex m_mutex;
    std::map<int64_t, Subscriber> m_subscribers;
};

QueryHandler* s_handler = nullptr; // vive hasta el final (el router guarda un puntero)

} // namespace

void installInternalPages(Profile* profile)
{
    if (!s_search.load()) {
        refreshSearch();
        QObject::connect(&Settings::instance(), &Settings::changed, [] { refreshSearch(); });
    }
    profile->onContextCreated([](CefRefPtr<CefRequestContext> context) {
        context->RegisterSchemeHandlerFactory("sora", "", new SchemeFactory);
    });
}

void setInternalApi(InternalApi* api)
{
    s_api = api;
    if (!s_handler) {
        s_handler = new QueryHandler;
        engine::onUi([] {
            if (engine::router())
                engine::router()->AddHandler(s_handler, false);
        });
    }
    for (auto signal : {&InternalApi::bookmarksChanged, &InternalApi::downloadsChanged, &InternalApi::extensionsChanged}) {
        const QMetaMethod m = QMetaMethod::fromSignal(signal);
        const QString name = QString::fromLatin1(m.name());
        QObject::connect(api, signal, api, [name] { s_handler->notify(name); });
    }
}

} // namespace sora
