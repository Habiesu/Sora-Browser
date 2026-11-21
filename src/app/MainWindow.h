#pragma once
#include <QMainWindow>
#include <QUrl>
#include "ui/MainMenu.h"

class QLabel;
class QShortcut;
class QStackedWidget;
class QWebEngineProfile;

namespace sora {

class AdblockService;
class BookmarksBar;
class BookmarkStore;
class HistoryStore;
class FindBar;
class Tab;
class TabManager;
class TitleBar;
class Toolbar;

// Convierte lo que el usuario escribe en la barra de direcciones en una URL:
// con esquema → tal cual; dominio → https; localhost → http; otro texto → búsqueda en DuckDuckGo.
QUrl urlFromInput(const QString& input);

// Ventana Niebla: barra de título con pestañas, barra de herramientas, favoritos y la web.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWebEngineProfile* profile, const AdblockService* adblock, bool startWithTab = true,
               QWidget* parent = nullptr);
    ~MainWindow() override;

    TabManager* tabs() const { return m_tabs; }
    bool isIncognito() const { return m_incognito; }
    // Mover pestañas entre ventanas: la misma página, sin recargar.
    Tab* takeTab(int index);
    void adoptTab(Tab* tab, int index = -1);
    Tab* currentTab() const;
    void navigate(const QString& input);
    void newTab();
    // Historial (no se usa en incógnito) y marcadores; los pone Browser cuando están listos.
    void setStores(HistoryStore* history, BookmarkStore* bookmarks);
    void applyColorScheme(Qt::ColorScheme scheme);
    // Aviso breve abajo a la izquierda (p. ej. si fallan las listas de bloqueo).
    void showNotice(const QString& text);

signals:
    void newWindowRequested(bool incognito);
    // La pestaña se ha soltado fuera de la tira: ventana nueva o la ventana que haya debajo.
    void tabDetachRequested(int index, QPoint globalPos);

protected:
    void changeEvent(QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    TitleBar* m_titleBar;
    Toolbar* m_toolbar;
    BookmarksBar* m_bookmarks;
    QStackedWidget* m_stack;
    TabManager* m_tabs;
    QLabel* m_linkPreview;
    FindBar* m_findBar;
    HistoryStore* m_history = nullptr;
    BookmarkStore* m_bookmarkStore = nullptr;
    bool m_bookmarksWanted = true; // lo que el usuario eligió (Ctrl+Mayús+B), fuera de pantalla completa
    bool m_wasMaximized = false;
    bool m_pageFullScreen = false; // un vídeo pidió pantalla completa: F11 se la quita a él
    QShortcut* m_stop = nullptr; // Esc: solo mientras carga la pestaña visible y el foco está en la web
    bool m_incognito;
    bool m_syncing = false; // cambios hechos por código en la tira: no reenviarlos
    bool m_maximizePressed = false; // botón maximizar (no cliente) pulsado y aún no soltado

    void watchTab(Tab* tab);
    void showCurrentTab();
    void refreshNavigation();
    void selectTab(int index);
    void toggleMaximized();
    void leaveMaximizeButton();
    void placeLinkPreview();
    void addShortcuts();
    QString titleFor(const QString& pageTitle) const;
    void runCommand(MainMenu::Command command);
    void openInternal(const QUrl& url);
    void zoom(int step); // +1, -1 o 0 (100 %), con los niveles de Chrome
    void setBrowserFullScreen(bool on);
    void openDevTools();
    void printPage();
    void placeFindBar();
    void toggleBookmark();
    void refreshStar();
};

} // namespace sora
