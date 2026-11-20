#pragma once
#include <QLineEdit>
#include <QUrl>
#include "ui/Theme.h"

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

signals:
    void submitted(const QString& text);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;

private:
    QUrl m_url;
    QAction* m_security;
    Palette m_palette;
    void refreshSecurityIcon();
};

} // namespace sora
