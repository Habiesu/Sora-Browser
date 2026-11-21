#pragma once
#include <QIcon>
#include <QPointer>
#include <QUrl>
#include <QWidget>
#include <functional>

#include "include/cef_base.h"
#include "include/cef_browser.h"

namespace sora {

class Profile;

// Una pestaña: un hueco nativo donde pinta un navegador CEF (ventana hija), con la misma interfaz
// que usaba la vista de Qt WebEngine. CEF avisa desde su hilo; todo llega aquí por la cola de Qt.
// El contador de bloqueos es de la página actual (vuelve a 0 al empezar a cargar otra).
class Tab : public QWidget {
    Q_OBJECT
public:
    explicit Tab(Profile* profile, QWidget* parent = nullptr);
    ~Tab() override;

    Profile* profile() const { return m_profile; }
    QUrl url() const { return m_url; }
    QString title() const { return m_title; }
    QIcon icon() const { return m_icon; }
    void setUrl(const QUrl& url);

    void back();
    void forward();
    void reload();
    void stop();
    bool canGoBack() const { return m_canGoBack; }
    bool canGoForward() const { return m_canGoForward; }
    bool isLoading() const { return m_loading; }

    double zoomFactor() const { return m_zoom; }
    void setZoomFactor(double factor);
    // Busca en la página; vacío quita el resaltado. El resultado llega con findResult().
    void findText(const QString& text, bool backward = false);
    void print();
    void showDevTools();
    void exitFullScreen();
    // "Guardar página como" en un solo archivo .mhtml.
    void savePage(const QString& path);
    void executeJavaScript(const QString& code);

    int blockedCount() const { return m_blocked; }
    void countBlocked(int count = 1);

    // Lo pone TabManager: dónde abrir lo que la web pide en otra pestaña.
    std::function<Tab*(const QUrl& url, bool background)> onCreateWindow;

    class Client;

signals:
    void urlChanged(const QUrl& url);
    void titleChanged(const QString& title);
    void iconChanged(const QIcon& icon);
    void loadStarted();
    void loadFinished(bool ok);
    void fullScreenRequested(bool on);
    void linkHovered(const QUrl& url); // vacío al salir del enlace
    void windowCloseRequested();
    void blockedCountChanged(int count);
    void findResult(int activeMatch, int matches);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    friend class Client;
    void createBrowser(const QUrl& url);
    void fitBrowser();
    void updateYoutubeScript();
    void triggerShortcut(int keyCombination);

    Profile* m_profile;
    CefRefPtr<Client> m_client;
    CefRefPtr<CefBrowser> m_browser; // solo en el hilo de Qt
    void* m_browserWindow = nullptr; // HWND del navegador (hijo del de esta pestaña)
    QUrl m_url;
    QUrl m_pendingUrl; // pedida antes de que el navegador exista
    QString m_title;
    QIcon m_icon;
    QString m_findText;
    double m_zoom = 1.0;
    int m_blocked = 0;
    bool m_loading = false;
    bool m_creating = false; // CreateBrowser pedido
    bool m_failed = false;
    bool m_canGoBack = false;
    bool m_canGoForward = false;
    QString m_youtubeScriptId;
};

} // namespace sora
