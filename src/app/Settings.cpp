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
    if (key == QLatin1String("downloadPath"))
        return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return {};
}

const QStringList kKeys = {QStringLiteral("search"), QStringLiteral("startup"), QStringLiteral("homepage"),
                           QStringLiteral("theme"), QStringLiteral("adblock"), QStringLiteral("adblockAllowed"),
                           QStringLiteral("downloadPath")};
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
    if (key == QLatin1String("adblockAllowed")) // vacía o de un solo sitio el .ini no la guarda como lista
        return m_store.value(key).toStringList();
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
    } else if (key == QLatin1String("adblockAllowed")) {
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
