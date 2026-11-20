#pragma once
#include <QWebEnginePage>
#include <QWebEngineView>
#include <functional>

class QWebEngineProfile;

namespace sora {

// Una pestaña: la vista web con su propia página y el contador de bloqueos de la página actual
// (vuelve a 0 al empezar a cargar otra).
class Tab : public QWebEngineView {
    Q_OBJECT
public:
    explicit Tab(QWebEngineProfile* profile, QWidget* parent = nullptr);

    int blockedCount() const { return m_blocked; }
    bool isLoading() const { return m_loading; }
    void countBlocked();

    // Lo pone TabManager: dónde abrir lo que la web pide en otra ventana.
    std::function<Tab*(QWebEnginePage::WebWindowType)> onCreateWindow;
    QWebEngineView* createWindow(QWebEnginePage::WebWindowType type) override;

signals:
    void blockedCountChanged(int count);

private:
    int m_blocked = 0;
    bool m_loading = false;
};

} // namespace sora
