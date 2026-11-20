#pragma once
#include <QMainWindow>
#include <QUrl>

class QLineEdit;
class QWebEngineProfile;
class QWebEngineView;

namespace sora {

// Convierte lo que el usuario escribe en la barra de direcciones en una URL:
// con esquema → tal cual; dominio → https; localhost → http; otro texto → búsqueda en DuckDuckGo.
QUrl urlFromInput(const QString& input);

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWebEngineProfile* profile, QWidget* parent = nullptr);
    void navigate(const QString& input);
    QWebEngineView* view() const { return m_view; }
    // Suma un elemento bloqueado al contador de la página actual.
    void countBlocked();

private:
    QLineEdit* m_address = nullptr;
    QWebEngineView* m_view = nullptr;
    int m_blocked = 0;
    void showBlocked();
};

} // namespace sora
