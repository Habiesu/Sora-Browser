#pragma once
#include <QMainWindow>
#include <QUrl>

class QLabel;
class QStackedWidget;
class QWebEngineProfile;

namespace sora {

class AdblockService;
class BookmarksBar;
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
    MainWindow(QWebEngineProfile* profile, const AdblockService* adblock, QWidget* parent = nullptr);
    ~MainWindow() override;

    TabManager* tabs() const { return m_tabs; }
    Tab* currentTab() const;
    void navigate(const QString& input);
    void newTab();
    void applyColorScheme(Qt::ColorScheme scheme);
    // Aviso breve abajo a la izquierda (p. ej. si fallan las listas de bloqueo).
    void showNotice(const QString& text);

protected:
    void changeEvent(QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    TitleBar* m_titleBar;
    Toolbar* m_toolbar;
    BookmarksBar* m_bookmarks;
    QStackedWidget* m_stack;
    TabManager* m_tabs;
    QLabel* m_linkPreview;
    bool m_syncing = false; // cambios hechos por código en la tira: no reenviarlos

    void watchTab(Tab* tab);
    void showCurrentTab();
    void refreshNavigation();
    void selectTab(int index);
    void toggleMaximized();
    void placeLinkPreview();
    void addShortcuts();
};

} // namespace sora
