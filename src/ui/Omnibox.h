#pragma once
#include <QLineEdit>
#include <QUrl>
#include "browser/Suggestions.h"
#include "ui/Theme.h"

class QCompleter;
class QStandardItemModel;

namespace sora {

// Dirección como la muestra Chrome: sin "https://" ni "www."; http se ve entero; about:blank vacío.
QString displayUrl(const QUrl& url);

// Barra de direcciones redondeada con candado (https) o globo (lo demás).
class Omnibox : public QLineEdit {
    Q_OBJECT
public:
    explicit Omnibox(QWidget* parent = nullptr);

    // Si el usuario está escribiendo, guarda la URL pero no le pisa el texto.
    void setUrl(const QUrl& url);
    void applyPalette(const Palette& p);
    // Lista bajo la barra (vacía = se cierra).
    void setSuggestions(const QList<Suggestion>& items);
    // Completa en línea con `full` dejando seleccionado lo añadido, si el usuario no está borrando.
    void setInlineCompletion(const QString& full);

signals:
    void submitted(const QString& text);
    void tabRequested(int index); // "Cambiar a esta pestaña"

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;

private:
    QUrl m_url;
    QAction* m_security;
    Palette m_palette;
    QCompleter* m_completer;
    QStandardItemModel* m_model;
    bool m_deleting = false; // la última tecla borró: no se completa en línea
    bool m_keyHandled = false;
    void activate(int row);
    void refreshSecurityIcon();
};

} // namespace sora
