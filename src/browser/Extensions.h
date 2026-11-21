#pragma once
#include <QList>
#include <QString>
#include <QUrl>

namespace sora {

// Extensiones de Chrome del perfil normal. Cada una vive desempaquetada en su carpeta dentro de
// <datos>/extensions y CEF las carga al arrancar (--load-extension): instalar, activar o quitar se
// aplica al reiniciar Sora.
namespace Extensions {

struct Entry {
    QString id; // el que le da Chromium (por la ruta, o por "key" del manifiesto)
    QString path;
    QString name;
    QString description;
    bool enabled = true;
    bool pending = false; // cambiada en esta sesión: se aplica al reiniciar
    QUrl popup; // ventana del botón de la extensión, si tiene
};

QString folder();
QList<Entry> all();
QStringList enabledPaths();
// Copia una carpeta desempaquetada, o desempaqueta un .crx/.zip. Devuelve un error o vacío.
QString install(const QString& source);
void setEnabled(const QString& id, bool enabled);
void remove(const QString& id);

// Como Chromium: SHA-256 de la ruta (UTF-16, unidad en mayúscula) o de la clave pública; los 32
// primeros dígitos hexadecimales pasados a a-p.
QString idForPath(const QString& absolutePath);
QString idForKey(const QByteArray& publicKey);

} // namespace Extensions
} // namespace sora
