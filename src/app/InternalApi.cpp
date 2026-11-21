#include "app/InternalApi.h"

#include <QApplication>
#include <QFileDialog>
#include <QUrl>
#include <QMessageBox>

#include "app/Settings.h"

#include "browser/DownloadManager.h"
#include "browser/Engine.h"
#include "browser/Extensions.h"
#include "browser/Profile.h"
#include "data/BookmarkStore.h"
#include "data/HistoryStore.h"

namespace sora {

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
    for (Profile* p : std::as_const(m_profiles)) {
        if (cookies)
            p->clearCookies();
        if (cache)
            p->clearCache();
    }
}

void InternalApi::setProfiles(QList<Profile*> profiles)
{
    m_profiles = std::move(profiles);
}

QVariantList InternalApi::extensions() const
{
    QVariantList out;
    for (const Extensions::Entry& e : Extensions::all())
        out.append(QVariantMap{{QStringLiteral("id"), e.id},
                               {QStringLiteral("name"), e.name},
                               {QStringLiteral("description"), e.description},
                               {QStringLiteral("enabled"), e.enabled},
                               {QStringLiteral("error"), e.pending ? tr("Se aplicará al reiniciar Sora.") : QString()},
                               {QStringLiteral("popup"), e.popup.toString()}});
    return out;
}

void InternalApi::installExtension()
{
    const QString file = QFileDialog::getOpenFileName(QApplication::activeWindow(), tr("Instalar extensión"), QString(),
                                                      tr("Extensión (*.zip *.crx)"));
    if (file.isEmpty())
        return;
    if (const QString error = Extensions::install(file); !error.isEmpty())
        QMessageBox::warning(QApplication::activeWindow(), tr("Instalar extensión"), error);
    emit extensionsChanged();
}

void InternalApi::loadUnpackedExtension()
{
    const QString dir = QFileDialog::getExistingDirectory(QApplication::activeWindow(), tr("Carpeta de la extensión"));
    if (dir.isEmpty())
        return;
    // Se copia: sigue ahí aunque se borre la carpeta.
    if (const QString error = Extensions::install(dir); !error.isEmpty())
        QMessageBox::warning(QApplication::activeWindow(), tr("Cargar extensión"), error);
    emit extensionsChanged();
}

void InternalApi::setExtensionEnabled(const QString& id, bool enabled)
{
    Extensions::setEnabled(id, enabled);
    emit extensionsChanged();
}

void InternalApi::removeExtension(const QString& id)
{
    Extensions::remove(id);
    emit extensionsChanged();
}

QVariantMap InternalApi::about() const
{
    return {{QStringLiteral("version"), QCoreApplication::applicationVersion()},
            {QStringLiteral("qt"), QString::fromLatin1(qVersion())},
            {QStringLiteral("chromium"), engine::chromiumVersion()}};
}

QVariant InternalApi::call(const QString& method, const QJsonArray& args, bool* ok)
{
    // Las páginas sora:// son de Sora, pero lo que llega se valida igual (tipos y número de argumentos).
    *ok = true;
    const auto str = [&](int i) { return args.at(i).toString(); };
    const int n = int(args.size());
    if (method == QLatin1String("history") && n == 1) return history(str(0));
    if (method == QLatin1String("removeHistory") && n == 1) { removeHistory(str(0)); return {}; }
    if (method == QLatin1String("clearHistory") && n == 0) { clearHistory(); return {}; }
    if (method == QLatin1String("bookmarks") && n == 0) return bookmarks();
    if (method == QLatin1String("removeBookmark") && n == 1) { removeBookmark(qint64(args.at(0).toDouble())); return {}; }
    if (method == QLatin1String("renameBookmark") && n == 2) { renameBookmark(qint64(args.at(0).toDouble()), str(1)); return {}; }
    if (method == QLatin1String("moveBookmark") && n == 2) { moveBookmark(qint64(args.at(0).toDouble()), args.at(1).toInt()); return {}; }
    if (method == QLatin1String("downloads") && n == 0) return downloads();
    if (method == QLatin1String("downloadAction") && n == 2) { downloadAction(args.at(0).toInt(), str(1)); return {}; }
    if (method == QLatin1String("clearDownloads") && n == 0) { clearDownloads(); return {}; }
    if (method == QLatin1String("settings") && n == 0) return settings();
    if (method == QLatin1String("setSetting") && n == 2) return setSetting(str(0), args.at(1).toVariant());
    if (method == QLatin1String("chooseDownloadFolder") && n == 0) return chooseDownloadFolder();
    if (method == QLatin1String("clearBrowsingData") && n == 3) {
        clearBrowsingData(args.at(0).toBool(), args.at(1).toBool(), args.at(2).toBool());
        return {};
    }
    if (method == QLatin1String("about") && n == 0) return about();
    if (method == QLatin1String("extensions") && n == 0) return extensions();
    if (method == QLatin1String("installExtension") && n == 0) { installExtension(); return {}; }
    if (method == QLatin1String("loadUnpackedExtension") && n == 0) { loadUnpackedExtension(); return {}; }
    if (method == QLatin1String("setExtensionEnabled") && n == 2) { setExtensionEnabled(str(0), args.at(1).toBool()); return {}; }
    if (method == QLatin1String("removeExtension") && n == 1) { removeExtension(str(0)); return {}; }
    *ok = false;
    return {};
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
