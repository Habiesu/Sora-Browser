#pragma once
#include <QUrl>

namespace sora {

inline const QUrl kNewTabUrl{QStringLiteral("sora://newtab")};
inline const QUrl kIncognitoNewTabUrl{QStringLiteral("sora://newtab/incognito")};

class InternalApi;
class Profile;

// Páginas sora:// (nueva pestaña, ajustes, historial...): HTML dentro del ejecutable.
void installInternalPages(Profile* profile);
// El objeto al que llaman esas páginas (window.sora). Solo lo ven las páginas sora://.
void setInternalApi(InternalApi* api);

} // namespace sora
