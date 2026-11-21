#pragma once
#include <QObject>
#include <QSettings>
#include <QUrl>
#include <QVariantMap>

namespace sora {

// Ajustes del usuario (Sora.ini en la carpeta de datos de la aplicación). Claves:
// search (duckduckgo|brave|google|bing|startpage), startup (newtab|homepage), homepage (URL http/https),
// theme (system|light|dark), adblock (bool), adblockAllowed (sitios sin escudo), downloadPath (carpeta),
// autoplay (sites|always|never: vídeo con sonido sin tocar la página), autoplaySites (los sitios de "sites").
class Settings : public QObject {
    Q_OBJECT
public:
    static Settings& instance();

    QVariant value(const QString& key) const;
    QVariantMap all() const;
    // Comprueba la clave y el valor (llegan de páginas sora://); devuelve false si no son válidos.
    bool setValue(const QString& key, const QVariant& value);

    QUrl searchUrl(const QString& query) const;
    QString searchName() const;
    QString searchAction() const; // la dirección del formulario de búsqueda (parámetro q)
    // ¿Puede esta página reproducir vídeo con sonido sin que el usuario la toque? (como Chrome)
    bool autoplayAllowed(const QUrl& url) const;
    static QStringList defaultAutoplaySites();

signals:
    void changed(const QString& key);

private:
    Settings();
    QSettings m_store;
};

} // namespace sora
