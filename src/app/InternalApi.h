#pragma once
#include <QObject>
#include <QPointer>
#include <QVariantList>

class QWebEngineExtensionManager;
class QWebEngineProfile;

namespace sora {

class BookmarkStore;

// Las extensiones que trae Chromium (visor de PDF, Hangouts): no se muestran ni se tocan.
bool isBuiltInExtension(const QString& id);

class DownloadManager;
class HistoryStore;

// Lo que las páginas sora:// pueden pedirle a Sora (por QWebChannel, objeto "sora" en JavaScript).
// Solo se da a páginas sora://: ninguna web normal lo ve.
class InternalApi : public QObject {
    Q_OBJECT
public:
    InternalApi(HistoryStore* history, BookmarkStore* bookmarks, DownloadManager* downloads = nullptr,
                QObject* parent = nullptr);

    // Historial: [{url, title, time (ms), visits}], lo más reciente primero.
    Q_INVOKABLE QVariantList history(const QString& query) const;
    Q_INVOKABLE void removeHistory(const QString& url);
    Q_INVOKABLE void clearHistory();

    // Marcadores: [{id, url, title}] en el orden de la barra.
    Q_INVOKABLE QVariantList bookmarks() const;
    Q_INVOKABLE void removeBookmark(qint64 id);
    Q_INVOKABLE void renameBookmark(qint64 id, const QString& title);
    Q_INVOKABLE void moveBookmark(qint64 id, int position);

    // Descargas: ver DownloadManager::list.
    Q_INVOKABLE QVariantList downloads() const;
    Q_INVOKABLE void downloadAction(int id, const QString& action); // cancel|pause|resume|open|show
    Q_INVOKABLE void clearDownloads();

    // Ajustes (ver Settings). setSetting no deja cambiar la carpeta de descargas: solo chooseDownloadFolder.
    Q_INVOKABLE QVariantMap settings() const;
    Q_INVOKABLE bool setSetting(const QString& key, const QVariant& value);
    Q_INVOKABLE QString chooseDownloadFolder();
    Q_INVOKABLE void clearBrowsingData(bool history, bool cookies, bool cache);
    Q_INVOKABLE QVariantMap about() const;
    // El primero es el perfil normal: el único con extensiones.
    void setProfiles(QList<QWebEngineProfile*> profiles);

    // Extensiones: [{id, name, description, enabled, error, popup}].
    Q_INVOKABLE QVariantList extensions() const;
    Q_INVOKABLE void installExtension(); // pide un .zip o .crx
    Q_INVOKABLE void loadUnpackedExtension(); // pide una carpeta
    Q_INVOKABLE void setExtensionEnabled(const QString& id, bool enabled);
    Q_INVOKABLE void removeExtension(const QString& id);

signals:
    void bookmarksChanged();
    void downloadsChanged();
    void extensionsChanged();

private:
    HistoryStore* m_history;
    QPointer<BookmarkStore> m_bookmarks;
    QPointer<DownloadManager> m_downloads;
    QList<QWebEngineProfile*> m_profiles;

    QWebEngineExtensionManager* extensionManager() const;
};

} // namespace sora
