#include "data/Database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace sora {

QString openDatabase(const QString& path, const QString& owner, const QString& schema)
{
    const QString connection = owner + QLatin1Char('@') + path;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(path);
    if (!db.open())
        qWarning("No se pudo abrir %s: %s", qPrintable(path), qPrintable(db.lastError().text()));
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL")); // dos almacenes en el mismo archivo sin bloquearse
    for (const QString& statement : schema.split(QLatin1Char(';'), Qt::SkipEmptyParts))
        if (!statement.trimmed().isEmpty() && !q.exec(statement))
            qWarning("SQL: %s", qPrintable(q.lastError().text()));
    return connection;
}

void closeDatabase(const QString& connection)
{
    QSqlDatabase::database(connection, false).close();
    QSqlDatabase::removeDatabase(connection);
}

} // namespace sora
