#include "browser/Suggestions.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

namespace sora {

namespace {
constexpr int kMaxTabs = 3;
constexpr int kMaxPages = 4; // marcadores + historial
constexpr int kMaxSearches = 5;

bool matches(const QString& input, const QString& title, const QUrl& url)
{
    return title.contains(input, Qt::CaseInsensitive) || url.toString().contains(input, Qt::CaseInsensitive);
}

// El sitio sin "https://" ni "www.": lo que se compara al completar en línea.
QString bareHost(const QUrl& url)
{
    QString host = url.host();
    if (host.startsWith(QLatin1String("www.")))
        host.remove(0, 4);
    return host;
}
} // namespace

QStringList parseSearchSuggestions(const QByteArray& json)
{
    const QJsonArray root = QJsonDocument::fromJson(json).array();
    QStringList out;
    for (const auto& v : root.at(1).toArray())
        if (v.isString())
            out.append(v.toString());
    return out;
}

QList<Suggestion> buildSuggestions(const QString& rawInput, const QList<OpenTab>& tabs,
                                   const QList<BookmarkStore::Bookmark>& bookmarks,
                                   const QList<HistoryStore::Entry>& history, const QStringList& searches)
{
    const QString input = rawInput.trimmed();
    QList<Suggestion> out;
    if (input.isEmpty())
        return out;
    const bool looksLikeUrl = !input.contains(QLatin1Char(' '))
                              && (input.contains(QLatin1Char('.')) || input.contains(QLatin1Char(':')));
    out.append({looksLikeUrl ? Suggestion::Url : Suggestion::Search, input, {}});

    for (const OpenTab& t : tabs)
        if (out.size() <= kMaxTabs && matches(input, t.title, t.url))
            out.append({Suggestion::Tab, t.url.toString(), t.title, t.index});

    QSet<QString> seen;
    int pages = 0;
    for (const auto& b : bookmarks)
        if (pages < kMaxPages && matches(input, b.title, b.url) && !seen.contains(b.url.toString())) {
            seen.insert(b.url.toString());
            out.append({Suggestion::Bookmark, b.url.toString(), b.title});
            ++pages;
        }
    for (const auto& h : history)
        if (pages < kMaxPages && matches(input, h.title, h.url) && !seen.contains(h.url.toString())) {
            seen.insert(h.url.toString());
            out.append({Suggestion::History, h.url.toString(), h.title});
            ++pages;
        }

    int count = 0;
    for (const QString& s : searches)
        if (count < kMaxSearches && s.compare(input, Qt::CaseInsensitive) != 0) {
            out.append({Suggestion::Search, s, {}});
            ++count;
        }
    return out;
}

QString inlineCompletion(const QString& typed, const QList<BookmarkStore::Bookmark>& bookmarks,
                         const QList<HistoryStore::Entry>& history)
{
    if (typed.size() < 2 || typed.contains(QLatin1Char(' ')) || typed.contains(QLatin1Char('/')))
        return {};
    auto complete = [&typed](const QUrl& url) -> QString {
        const QString host = bareHost(url);
        if (host.size() > typed.size() && host.startsWith(typed, Qt::CaseInsensitive))
            return typed + host.mid(typed.size()); // lo escrito no cambia de mayúsculas
        return {};
    };
    for (const auto& b : bookmarks)
        if (QString c = complete(b.url); !c.isEmpty())
            return c;
    for (const auto& h : history)
        if (QString c = complete(h.url); !c.isEmpty())
            return c;
    return {};
}

} // namespace sora
