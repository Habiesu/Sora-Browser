#include "app/InternalPages.h"

#include <QBuffer>
#include <QFile>
#include <QMimeDatabase>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QPointer>
#include <QWebChannel>

namespace sora {
namespace {

// sora://<página>[/<variante>] → :/pages/<página>[-<variante>].html ; sora://res/<archivo> → :/pages/res/<archivo>
QString resourceFor(const QUrl& url)
{
    const QString host = url.host();
    QString path = url.path();
    if (path.contains(QLatin1String("..")))
        return {};
    if (host == QLatin1String("res") && path == QLatin1String("/qwebchannel.js"))
        return QStringLiteral(":/qtwebchannel/qwebchannel.js"); // viene con Qt WebChannel
    if (host == QLatin1String("res"))
        return QStringLiteral(":/pages/res") + path;
    if (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    const QString variant = path.isEmpty() ? QString() : QLatin1Char('-') + path.mid(1);
    return QStringLiteral(":/pages/") + host + variant + QStringLiteral(".html");
}

class Handler : public QWebEngineUrlSchemeHandler {
public:
    using QWebEngineUrlSchemeHandler::QWebEngineUrlSchemeHandler;

    void requestStarted(QWebEngineUrlRequestJob* job) override
    {
        const QString resource = resourceFor(job->requestUrl());
        QFile file(resource);
        if (resource.isEmpty() || !file.open(QIODevice::ReadOnly)) {
            job->fail(QWebEngineUrlRequestJob::UrlNotFound);
            return;
        }
        auto* buffer = new QBuffer(job);
        buffer->setData(file.readAll());
        const QByteArray mime = QMimeDatabase().mimeTypeForFile(resource, QMimeDatabase::MatchExtension).name().toUtf8();
        job->reply(mime == "text/html" ? QByteArrayLiteral("text/html;charset=utf-8") : mime, buffer);
    }
};

QPointer<QWebChannel> s_channel; // vive con el InternalApi actual

} // namespace

void setInternalApi(QObject* api)
{
    if (!s_channel)
        s_channel = new QWebChannel(api);
    s_channel->deregisterObject(s_channel->registeredObjects().value(QStringLiteral("sora")));
    s_channel->registerObject(QStringLiteral("sora"), api);
}

QWebChannel* internalChannel()
{
    return s_channel;
}

void registerSoraScheme()
{
    QWebEngineUrlScheme scheme(QByteArrayLiteral("sora"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    // LocalScheme: las webs normales no pueden enlazar, navegar ni incrustar páginas sora://.
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme | QWebEngineUrlScheme::LocalScheme);
    QWebEngineUrlScheme::registerScheme(scheme);
}

void installInternalPages(QWebEngineProfile* profile)
{
    profile->installUrlSchemeHandler(QByteArrayLiteral("sora"), new Handler(profile));
}

} // namespace sora
