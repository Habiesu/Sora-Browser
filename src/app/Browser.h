#pragma once
#include <QList>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <memory>

class QWebEngineProfile;

namespace sora {

class AdblockService;
class BookmarkStore;
class DownloadManager;
class InternalApi;
class HistoryStore;
class MainWindow;

// Dueño de las ventanas y de los perfiles (normal e incógnito), como el navegador en Chrome.
class Browser : public QObject {
    Q_OBJECT
public:
    Browser(QWebEngineProfile* profile, AdblockService* adblock, QObject* parent = nullptr);
    ~Browser() override;

    // Ventana nueva y visible con una pestaña vacía.
    MainWindow* newWindow(bool incognito = false);
    QList<MainWindow*> windows() const;
    // La última que estuvo activa (para lo que llega de otro Sora abierto después).
    MainWindow* activeWindow() const;

    // Lleva una pestaña a una ventana nueva en `globalPos`; nullptr si es la única de su ventana.
    MainWindow* detachTab(MainWindow* from, int index, QPoint globalPos);
    // Mueve una pestaña a otra ventana del mismo perfil; si era la última, su ventana se cierra.
    bool moveTab(MainWindow* from, int index, MainWindow* to, int toIndex);

    // Historial y marcadores (SQLite). Se abren después de mostrar la primera ventana.
    void openStores(const QString& databasePath);
    HistoryStore* history() const { return m_history.get(); }
    BookmarkStore* bookmarks() const { return m_bookmarks; }

private:
    QWebEngineProfile* m_profile;
    QWebEngineProfile* m_incognito = nullptr; // se crea la primera vez y vive mientras Sora
    AdblockService* m_adblock;
    QList<QPointer<MainWindow>> m_windows;
    QPointer<MainWindow> m_active;
    std::unique_ptr<HistoryStore> m_history;
    BookmarkStore* m_bookmarks = nullptr;
    DownloadManager* m_downloads;
    QPointer<InternalApi> m_api;

    MainWindow* createWindow(bool incognito, bool startWithTab);
    void applySettings();
    void onTabDetachRequested(MainWindow* from, int index, QPoint globalPos);
};

} // namespace sora
