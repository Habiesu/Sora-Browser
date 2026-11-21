#include "app/Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QUrlQuery>

namespace sora {

namespace {
struct Engine {
    const char* id;
    const char* name;
    const char* action;
};
constexpr Engine kEngines[] = {
    {"duckduckgo", "DuckDuckGo", "https://duckduckgo.com/"},
    {"brave", "Brave Search", "https://search.brave.com/search"},
    {"google", "Google", "https://www.google.com/search"},
    {"bing", "Bing", "https://www.bing.com/search"},
    {"startpage", "Startpage", "https://www.startpage.com/do/search"},
};

const Engine& engineFor(const QString& id)
{
    for (const Engine& e : kEngines)
        if (id == QLatin1String(e.id))
            return e;
    return kEngines[0];
}

QVariant defaultFor(const QString& key)
{
    if (key == QLatin1String("search"))
        return QStringLiteral("duckduckgo");
    if (key == QLatin1String("startup"))
        return QStringLiteral("newtab");
    if (key == QLatin1String("homepage"))
        return QString();
    if (key == QLatin1String("theme"))
        return QStringLiteral("system");
    if (key == QLatin1String("adblock"))
        return true;
    if (key == QLatin1String("adblockAllowed"))
        return QStringList();
    if (key == QLatin1String("autoplay"))
        return QStringLiteral("sites");
    if (key == QLatin1String("autoplaySites"))
        return Settings::defaultAutoplaySites();
    if (key == QLatin1String("downloadPath"))
        return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return {};
}

const QStringList kKeys = {QStringLiteral("search"), QStringLiteral("startup"), QStringLiteral("homepage"),
                           QStringLiteral("theme"), QStringLiteral("adblock"), QStringLiteral("adblockAllowed"),
                           QStringLiteral("downloadPath"), QStringLiteral("autoplay"), QStringLiteral("autoplaySites")};
} // namespace

Settings& Settings::instance()
{
    static Settings settings;
    return settings;
}

Settings::Settings()
    // Sora.ini; cada prueba (otro nombre de aplicación) usa su propio archivo y no toca los ajustes reales.
    : m_store(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Sora"), QCoreApplication::applicationName())
{
}

QVariant Settings::value(const QString& key) const
{
    if (key == QLatin1String("adblock")) // el .ini guarda texto: "true"/"false"
        return m_store.value(key, true).toBool();
    if (key == QLatin1String("adblockAllowed") || key == QLatin1String("autoplaySites")) // el .ini no guarda listas de 0 o 1 como lista
        return m_store.value(key, defaultFor(key)).toStringList();
    return m_store.value(key, defaultFor(key));
}

QVariantMap Settings::all() const
{
    QVariantMap out;
    for (const QString& key : kKeys)
        out.insert(key, value(key));
    return out;
}

bool Settings::setValue(const QString& key, const QVariant& value)
{
    bool ok = false;
    const QString text = value.toString();
    if (key == QLatin1String("search")) {
        ok = engineFor(text).id == text;
    } else if (key == QLatin1String("startup")) {
        ok = text == QLatin1String("newtab") || text == QLatin1String("homepage");
    } else if (key == QLatin1String("homepage")) {
        const QUrl url(text, QUrl::StrictMode);
        ok = text.isEmpty() || (url.isValid() && (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http")));
    } else if (key == QLatin1String("theme")) {
        ok = text == QLatin1String("system") || text == QLatin1String("light") || text == QLatin1String("dark");
    } else if (key == QLatin1String("adblock")) {
        ok = value.typeId() == QMetaType::Bool;
    } else if (key == QLatin1String("autoplay")) {
        ok = text == QLatin1String("sites") || text == QLatin1String("always") || text == QLatin1String("never");
    } else if (key == QLatin1String("adblockAllowed") || key == QLatin1String("autoplaySites")) {
        ok = value.canConvert<QStringList>();
        for (const QString& site : value.toStringList())
            ok = ok && !site.isEmpty() && QUrl(QStringLiteral("https://") + site).host() == site;
    } else if (key == QLatin1String("downloadPath")) {
        ok = !text.isEmpty() && QDir(text).exists();
    }
    if (!ok)
        return false;
    if (this->value(key) != value) {
        m_store.setValue(key, value);
        emit changed(key);
    }
    return true;
}

QStringList Settings::defaultAutoplaySites()
{
    // Sitios donde Chrome suele dejar arrancar el vídeo con sonido (los que más se usan para ver vídeo).
    return {QStringLiteral("youtube.com"), QStringLiteral("twitch.tv"), QStringLiteral("vimeo.com"),
            QStringLiteral("netflix.com"), QStringLiteral("crunchyroll.com"), QStringLiteral("primevideo.com"),
            QStringLiteral("disneyplus.com"), QStringLiteral("max.com"), QStringLiteral("kick.com"),
            QStringLiteral("dailymotion.com"), QStringLiteral("open.spotify.com"), QStringLiteral("soundcloud.com")};
}

bool Settings::autoplayAllowed(const QUrl& url) const
{
    const QString mode = value(QStringLiteral("autoplay")).toString();
    if (mode != QLatin1String("sites"))
        return mode == QLatin1String("always");
    const QStringList sites = value(QStringLiteral("autoplaySites")).toStringList();
    for (QString host = url.host().toLower(); !host.isEmpty();) { // m.twitch.tv → twitch.tv → tv
        if (sites.contains(host))
            return true;
        const qsizetype dot = host.indexOf(QLatin1Char('.'));
        host = dot < 0 ? QString() : host.mid(dot + 1);
    }
    return false;
}

QString Settings::searchName() const
{
    return QString::fromLatin1(engineFor(value(QStringLiteral("search")).toString()).name);
}

QString Settings::searchAction() const
{
    return QString::fromLatin1(engineFor(value(QStringLiteral("search")).toString()).action);
}

QUrl Settings::searchUrl(const QString& query) const
{
    QUrl url(searchAction());
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), query);
    url.setQuery(q);
    return url;
}

} // namespace sora
