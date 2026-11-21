#pragma once
#include <QString>

namespace sora {

// Abre (y crea si hace falta) la base de datos SQLite de Sora con un nombre de conexión propio.
// Devuelve ese nombre; cerrarla con closeDatabase.
QString openDatabase(const QString& path, const QString& owner, const QString& schema);
void closeDatabase(const QString& connection);

} // namespace sora
