#include "app/InternalApi.h"

#include <QApplication>
#include <QFileDialog>
#include <QUrl>
#include <QWebEngineCookieStore>
#include <QWebEngineExtensionInfo>
#include <QWebEngineExtensionManager>
#include <QWebEngineProfile>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

#include "app/Settings.h"

#include "browser/DownloadManager.h"
#include "data/BookmarkStore.h"
#include "data/HistoryStore.h"

namespace sora {

bool isBuiltInExtension(const QString& id)
{
    return id == QLatin1String("mhjfbmdgcfjbbpaeojofohoefgiehjai") // visor de PDF
           || id == QLatin1String("nkeimhogjdpnpccoofpliimaahmaaome"); // Hangouts
}

InternalApi::InternalApi(HistoryStore* history, BookmarkStore* bookmarks, DownloadManager* downloads,
                         QObject* parent)
    : QObject(parent)
    , m_history(history)
    , m_bookmarks(bookmarks)
    , m_downloads(downloads)
{
    if (m_bookmarks)
        connect(m_bookmarks, &BookmarkStore::changed, this, &InternalApi::bookmarksChanged);
    if (m_downloads)
        connect(m_downloads, &DownloadManager::changed, this, &InternalApi::downloadsChanged);
}

QVariantList InternalApi::downloads() const
{
    return m_downloads ? m_downloads->list() : QVariantList();
}

void InternalApi::downloadAction(int id, const QString& action)
{
    if (!m_downloads)
        return;
    if (action == QLatin1String("cancel"))
        m_downloads->cancel(id);
    else if (action == QLatin1String("pause"))
        m_downloads->pause(id);
    else if (action == QLatin1String("resume"))
        m_downloads->resume(id);
    else if (action == QLatin1String("open"))
        m_downloads->open(id);
    else if (action == QLatin1String("show"))
        m_downloads->showInFolder(id);
}

QVariantMap InternalApi::settings() const
{
    return Settings::instance().all();
}

bool InternalApi::setSetting(const QString& key, const QVariant& value)
{
    return key != QLatin1String("downloadPath") && Settings::instance().setValue(key, value);
}

QString InternalApi::chooseDownloadFolder()
{
    Settings& s = Settings::instance();
    const QString dir = QFileDialog::getExistingDirectory(QApplication::activeWindow(), tr("Carpeta de descargas"),
                                                          s.value(QStringLiteral("downloadPath")).toString());
    if (!dir.isEmpty())
        s.setValue(QStringLiteral("downloadPath"), dir);
    return s.value(QStringLiteral("downloadPath")).toString();
}

void InternalApi::clearBrowsingData(bool history, bool cookies, bool cache)
{
    if (history && m_history)
        m_history->clear();
    for (QWebEngineProfile* p : std::as_const(m_profiles)) {
        if (cookies)
            p->cookieStore()->deleteAllCookies();
        if (cache)
            p->clearHttpCache();
    }
}

void InternalApi::setProfiles(QList<QWebEngineProfile*> profiles)
{
    const bool first = m_profiles.isEmpty();
    m_profiles = std::move(profiles);
    if (auto* manager = extensionManager(); manager && first) {
        using M = QWebEngineExtensionManager;
        for (auto signal : {&M::loadFinished, &M::installFinished, &M::unloadFinished, &M::uninstallFinished})
            connect(manager, signal, this, &InternalApi::extensionsChanged);
    }
}

QWebEngineExtensionManager* InternalApi::extensionManager() const
{
    return m_profiles.isEmpty() ? nullptr : m_profiles.first()->extensionManager();
}

QVariantList InternalApi::extensions() const
{
    QVariantList out;
    if (auto* manager = extensionManager())
        for (const QWebEngineExtensionInfo& e : manager->extensions()) {
            if (isBuiltInExtension(e.id()))
                continue;
            out.append(QVariantMap{{QStringLiteral("id"), e.id()},
                                   {QStringLiteral("name"), e.name()},
                                   {QStringLiteral("description"), e.description()},
                                   {QStringLiteral("enabled"), e.isEnabled()},
                                   {QStringLiteral("error"), e.error()},
                                   {QStringLiteral("popup"), e.actionPopupUrl().toString()}});
        }
    return out;
}

void InternalApi::installExtension()
{
    auto* manager = extensionManager();
    if (!manager)
        return;
    const QString file = QFileDialog::getOpenFileName(QApplication::activeWindow(), tr("Instalar extensión"), QString(),
                                                      tr("Extensión (*.zip *.crx)"));
    if (!file.isEmpty())
        manager->installExtension(file);
}

void InternalApi::loadUnpackedExtension()
{
    auto* manager = extensionManager();
    if (!manager)
        return;
    const QString dir = QFileDialog::getExistingDirectory(QApplication::activeWindow(), tr("Carpeta de la extensión"));
    if (!dir.isEmpty())
        manager->installExtension(dir); // se copia: sigue ahí aunque se borre la carpeta
}

void InternalApi::setExtensionEnabled(const QString& id, bool enabled)
{
    if (auto* manager = extensionManager())
        for (const QWebEngineExtensionInfo& e : manager->extensions())
            if (e.id() == id) {
                manager->setExtensionEnabled(e, enabled);
                emit extensionsChanged();
            }
}

void InternalApi::removeExtension(const QString& id)
{
    if (auto* manager = extensionManager())
        for (const QWebEngineExtensionInfo& e : manager->extensions())
            if (e.id() == id)
                e.isInstalled() ? manager->uninstallExtension(e) : manager->unloadExtension(e);
}

QVariantMap InternalApi::about() const
{
    return {{QStringLiteral("version"), QCoreApplication::applicationVersion()},
            {QStringLiteral("qt"), QString::fromLatin1(qVersion())},
            {QStringLiteral("chromium"), QString::fromLatin1(qWebEngineChromiumVersion())}};
}

void InternalApi::clearDownloads()
{
    if (m_downloads)
        m_downloads->clearFinished();
}

QVariantList InternalApi::history(const QString& query) const
{
    QVariantList out;
    if (!m_history)
        return out;
    for (const auto& e : m_history->search(query))
        out.append(QVariantMap{{QStringLiteral("url"), e.url.toString()},
                               {QStringLiteral("title"), e.title},
                               {QStringLiteral("time"), e.lastVisit.toMSecsSinceEpoch()},
                               {QStringLiteral("visits"), e.visits}});
    return out;
}

void InternalApi::removeHistory(const QString& url)
{
    if (m_history)
        m_history->remove(QUrl(url));
}

void InternalApi::clearHistory()
{
    if (m_history)
        m_history->clear();
}

QVariantList InternalApi::bookmarks() const
{
    QVariantList out;
    if (!m_bookmarks)
        return out;
    for (const auto& b : m_bookmarks->all())
        out.append(QVariantMap{{QStringLiteral("id"), b.id},
                               {QStringLiteral("url"), b.url.toString()},
                               {QStringLiteral("title"), b.title}});
    return out;
}

void InternalApi::removeBookmark(qint64 id)
{
    if (m_bookmarks)
        m_bookmarks->remove(id);
}

void InternalApi::renameBookmark(qint64 id, const QString& title)
{
    if (m_bookmarks && !title.trimmed().isEmpty())
        m_bookmarks->rename(id, title.trimmed());
}

void InternalApi::moveBookmark(qint64 id, int position)
{
    if (m_bookmarks)
        m_bookmarks->move(id, position);
}

} // namespace sora
