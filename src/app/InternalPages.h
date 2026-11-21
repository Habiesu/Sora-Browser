#pragma once
#include <QUrl>

class QObject;
class QWebChannel;
class QWebEngineProfile;

namespace sora {

// Páginas internas de Sora en sora:// (nueva pestaña, historial, ajustes...), como chrome:// en
// Chrome. Son locales: una web normal no puede navegar a ellas ni incrustarlas.
void registerSoraScheme(); // antes de crear QApplication
void installInternalPages(QWebEngineProfile* profile);
// El objeto "sora" que ven las páginas internas (InternalApi). Solo se da a páginas sora://.
void setInternalApi(QObject* api);
QWebChannel* internalChannel(); // nullptr hasta setInternalApi

inline const QUrl kNewTabUrl{QStringLiteral("sora://newtab")};
inline const QUrl kIncognitoNewTabUrl{QStringLiteral("sora://newtab/incognito")};

} // namespace sora
