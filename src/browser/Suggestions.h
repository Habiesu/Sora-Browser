#pragma once
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "data/BookmarkStore.h"
#include "data/HistoryStore.h"

namespace sora {

// Una fila de la lista que se abre bajo la barra de direcciones.
struct Suggestion {
    enum Kind { Search, Url, Tab, Bookmark, History };
    Kind kind;
    QString text;  // lo que se abre: búsqueda o dirección
    QString title; // título de la página (vacío en búsquedas)
    int tab = -1;  // Tab: índice de la pestaña abierta
};

struct OpenTab {
    int index;
    QString title;
    QUrl url;
};

// Sugerencias del buscador: la respuesta de duckduckgo.com/ac/?type=list es ["texto", ["s1", "s2"]].
QStringList parseSearchSuggestions(const QByteArray& json);

// Como Chrome: lo escrito, pestañas abiertas, marcadores, historial y búsquedas sugeridas, sin repetir.
QList<Suggestion> buildSuggestions(const QString& input, const QList<OpenTab>& tabs,
                                   const QList<BookmarkStore::Bookmark>& bookmarks,
                                   const QList<HistoryStore::Entry>& history, const QStringList& searches);

// Lo que se completa en línea al escribir el comienzo de un sitio conocido ("you" → "youtube.com").
// Devuelve el texto entero (con lo escrito tal cual) o vacío.
QString inlineCompletion(const QString& typed, const QList<BookmarkStore::Bookmark>& bookmarks,
                         const QList<HistoryStore::Entry>& history);

} // namespace sora
