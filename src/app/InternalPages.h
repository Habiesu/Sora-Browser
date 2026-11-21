#pragma once
#include <QUrl>

class QWebEngineProfile;

namespace sora {

// Páginas internas de Sora en sora:// (nueva pestaña, historial, ajustes...), como chrome:// en
// Chrome. Son locales: una web normal no puede navegar a ellas ni incrustarlas.
void registerSoraScheme(); // antes de crear QApplication
void installInternalPages(QWebEngineProfile* profile);

inline const QUrl kNewTabUrl{QStringLiteral("sora://newtab")};
inline const QUrl kIncognitoNewTabUrl{QStringLiteral("sora://newtab/incognito")};

} // namespace sora
