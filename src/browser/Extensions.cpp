#include "browser/Extensions.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QtEndian>

namespace sora::Extensions {
namespace {

QSet<QString> s_pending; // ids cambiados en esta sesión

QString hashToId(const QByteArray& data)
{
    const QByteArray hex = QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex().left(32);
    QString id;
    for (char c : hex)
        id += QLatin1Char(char('a' + QByteArray(1, c).toInt(nullptr, 16)));
    return id;
}

QString stateFile() { return folder() + QStringLiteral("/estado.json"); }

QSet<QString> disabled()
{
    QFile f(stateFile());
    QSet<QString> out;
    if (f.open(QIODevice::ReadOnly))
        for (const QJsonValue& v : QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("disabled")).toArray())
            out.insert(v.toString());
    return out;
}

void saveDisabled(const QSet<QString>& ids)
{
    QJsonArray list;
    for (const QString& id : ids)
        list.append(id);
    QFile f(stateFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(QJsonObject{{QStringLiteral("disabled"), list}}).toJson());
}

// "__MSG_nombre__" → el texto de _locales (español si lo hay; si no, el idioma por defecto).
QString localized(const QString& text, const QString& dir, const QString& defaultLocale)
{
    if (!text.startsWith(QLatin1String("__MSG_")) || !text.endsWith(QLatin1String("__")))
        return text;
    const QString key = text.mid(6, text.size() - 8).toLower();
    for (const QString& locale : {QStringLiteral("es"), QStringLiteral("es_419"), defaultLocale}) {
        QFile f(QStringLiteral("%1/_locales/%2/messages.json").arg(dir, locale));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject messages = QJsonDocument::fromJson(f.readAll()).object();
        for (auto it = messages.begin(); it != messages.end(); ++it)
            if (it.key().toLower() == key)
                return it.value().toObject().value(QStringLiteral("message")).toString();
    }
    return text;
}

bool copyTree(const QString& from, const QString& to)
{
    QDirIterator it(from, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        const QString target = to + QLatin1Char('/') + QDir(from).relativeFilePath(file);
        QDir().mkpath(QFileInfo(target).absolutePath());
        if (!QFile::copy(file, target))
            return false;
    }
    return true;
}

} // namespace

QString folder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/extensions");
}

QString idForPath(const QString& absolutePath)
{
    QString path = QDir::toNativeSeparators(QDir::cleanPath(absolutePath));
    if (path.size() >= 2 && path.at(1) == QLatin1Char(':'))
        path[0] = path.at(0).toUpper();
    QByteArray utf16;
    for (QChar c : path) {
        const char16_t u = qToLittleEndian(c.unicode());
        utf16.append(reinterpret_cast<const char*>(&u), 2);
    }
    return hashToId(utf16);
}

QString idForKey(const QByteArray& publicKey) { return hashToId(publicKey); }

QList<Entry> all()
{
    QList<Entry> out;
    const QSet<QString> off = disabled();
    for (const QFileInfo& dir : QDir(folder()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QFile f(dir.absoluteFilePath() + QStringLiteral("/manifest.json"));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject m = QJsonDocument::fromJson(f.readAll()).object();
        const QString path = dir.absoluteFilePath();
        const QString locale = m.value(QStringLiteral("default_locale")).toString();
        Entry e;
        const QString key = m.value(QStringLiteral("key")).toString();
        e.id = key.isEmpty() ? idForPath(path) : idForKey(QByteArray::fromBase64(key.toLatin1()));
        e.path = path;
        e.name = localized(m.value(QStringLiteral("name")).toString(), path, locale);
        e.description = localized(m.value(QStringLiteral("description")).toString(), path, locale);
        e.enabled = !off.contains(e.id);
        e.pending = s_pending.contains(e.id);
        QJsonObject action = m.value(QStringLiteral("action")).toObject();
        if (action.isEmpty())
            action = m.value(QStringLiteral("browser_action")).toObject();
        const QString popup = action.value(QStringLiteral("default_popup")).toString();
        if (!popup.isEmpty())
            e.popup = QUrl(QStringLiteral("chrome-extension://%1/%2").arg(e.id, popup));
        out.append(e);
    }
    return out;
}

QStringList enabledPaths()
{
    QStringList out;
    for (const Entry& e : all())
        if (e.enabled)
            out.append(e.path);
    return out;
}

QString install(const QString& source)
{
    const QFileInfo info(source);
    QString name = info.completeBaseName().isEmpty() ? info.fileName() : info.completeBaseName();
    QString target = folder() + QLatin1Char('/') + name;
    for (int n = 2; QFileInfo::exists(target); ++n)
        target = folder() + QLatin1Char('/') + name + QLatin1Char('-') + QString::number(n);
    QDir().mkpath(target);
    if (info.isDir()) {
        if (!QFile::exists(source + QStringLiteral("/manifest.json")) || !copyTree(source, target)) {
            QDir(target).removeRecursively();
            return QStringLiteral("La carpeta no es una extensión (falta manifest.json).");
        }
    } else {
        QFile in(source);
        if (!in.open(QIODevice::ReadOnly))
            return QStringLiteral("No se puede leer el archivo.");
        QByteArray data = in.readAll();
        // .crx: "Cr24", versión y tamaño de la cabecera; detrás va el .zip.
        if (data.startsWith("Cr24") && data.size() > 16) {
            const quint32 version = qFromLittleEndian<quint32>(data.constData() + 4);
            const qsizetype skip = version == 2 ? 16 + qFromLittleEndian<quint32>(data.constData() + 8)
                                                     + qFromLittleEndian<quint32>(data.constData() + 12)
                                                : 12 + qFromLittleEndian<quint32>(data.constData() + 8);
            data = data.mid(skip);
        }
        const QString zip = target + QStringLiteral(".zip");
        QFile out(zip);
        if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size())
            return QStringLiteral("No se puede desempaquetar la extensión.");
        out.close();
        // tar de Windows 10+ (bsdtar) desempaqueta .zip.
        const int code = QProcess::execute(QStringLiteral("tar"), {QStringLiteral("-xf"), zip, QStringLiteral("-C"), target});
        QFile::remove(zip);
        if (code != 0 || !QFile::exists(target + QStringLiteral("/manifest.json"))) {
            QDir(target).removeRecursively();
            return QStringLiteral("El archivo no es una extensión válida.");
        }
    }
    s_pending.insert(idForPath(target));
    return {};
}

void setEnabled(const QString& id, bool enabled)
{
    QSet<QString> off = disabled();
    if (enabled)
        off.remove(id);
    else
        off.insert(id);
    saveDisabled(off);
    s_pending.insert(id);
}

void remove(const QString& id)
{
    for (const Entry& e : all())
        if (e.id == id)
            QDir(e.path).removeRecursively();
    QSet<QString> off = disabled();
    off.remove(id);
    saveDisabled(off);
}

} // namespace sora::Extensions
