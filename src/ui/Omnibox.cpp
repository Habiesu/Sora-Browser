#include "ui/Omnibox.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCompleter>
#include <QStandardItemModel>
#include <QKeyEvent>
#include <QTimer>

#include "ui/Icons.h"

namespace sora {

namespace {
constexpr int kTextRole = Qt::UserRole + 1;
constexpr int kTabRole = Qt::UserRole + 2;
} // namespace

QString displayUrl(const QUrl& url)
{
    // Como Chrome: la nueva pestaña no muestra dirección.
    if (url.isEmpty() || url == QUrl(QStringLiteral("about:blank")) || url.toString().startsWith(QLatin1String("sora://newtab")))
        return {};
    if (url.scheme() != QLatin1String("https"))
        return url.toDisplayString();
    QString s = url.toDisplayString(QUrl::RemoveScheme);
    // La raíz de un sitio se muestra sin "/" (StripTrailingSlash no lo quita en la raíz).
    if (url.path() == QLatin1String("/") && !url.hasQuery() && !url.hasFragment())
        s.chop(1);
    if (s.startsWith(QLatin1String("//")))
        s.remove(0, 2);
    if (s.startsWith(QLatin1String("www.")))
        s.remove(0, 4);
    return s;
}

Omnibox::Omnibox(QWidget* parent)
    : QLineEdit(parent)
    , m_security(new QAction(this))
    , m_palette(nieblaPalette(Qt::ColorScheme::Light))
{
    setObjectName(QStringLiteral("Omnibox"));
    setPlaceholderText(tr("Busca o escribe una dirección"));
    addAction(m_security, QLineEdit::LeadingPosition);
    refreshSecurityIcon();

    m_model = new QStandardItemModel(this);
    m_completer = new QCompleter(m_model, this);
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion); // la lista ya viene filtrada
    m_completer->setCompletionRole(kTextRole);
    m_completer->setMaxVisibleItems(9);
    setCompleter(m_completer);
    // Clic en una fila (Intro se atiende en keyPressEvent, que llega antes).
    connect(m_completer, qOverload<const QModelIndex&>(&QCompleter::activated), this, [this](const QModelIndex& index) {
        if (!m_keyHandled)
            activate(index.row());
    });
}

void Omnibox::setSuggestions(const QList<Suggestion>& items)
{
    m_model->clear();
    for (const Suggestion& s : items) {
        auto* item = new QStandardItem;
        QString label = s.title.isEmpty() ? s.text : s.title + QStringLiteral(" — ") + displayUrl(QUrl(s.text));
        Icon kind = Icon::Globe;
        switch (s.kind) {
        case Suggestion::Search:
            label = s.text + QStringLiteral(" — ") + tr("Buscar");
            kind = Icon::Search;
            break;
        case Suggestion::Url: label = s.text; break;
        case Suggestion::Tab: label += QStringLiteral(" — ") + tr("Cambiar a esta pestaña"); kind = Icon::Tab; break;
        case Suggestion::Bookmark: kind = Icon::Star; break;
        case Suggestion::History: kind = Icon::History; break;
        }
        item->setText(label);
        item->setIcon(icon(kind, m_palette.mutedText));
        item->setData(s.text, kTextRole);
        item->setData(s.tab, kTabRole);
        m_model->appendRow(item);
    }
    if (items.isEmpty() || !hasFocus())
        m_completer->popup()->hide();
    else
        m_completer->complete();
}

void Omnibox::setInlineCompletion(const QString& full)
{
    const QString typed = text();
    if (m_deleting || full.isEmpty() || cursorPosition() != typed.size() || !full.startsWith(typed, Qt::CaseInsensitive))
        return;
    setText(full);
    setSelection(int(typed.size()), int(full.size() - typed.size()));
}

void Omnibox::activate(int row)
{
    const QModelIndex index = m_model->index(row, 0);
    m_completer->popup()->hide();
    setModified(false);
    if (const int tab = index.data(kTabRole).toInt(); index.isValid() && tab >= 0)
        emit tabRequested(tab);
    else
        emit submitted(index.isValid() ? index.data(kTextRole).toString() : text());
}

void Omnibox::setUrl(const QUrl& url)
{
    m_url = url;
    refreshSecurityIcon();
    if (hasFocus() && isModified())
        return;
    setText(displayUrl(url));
    setModified(false);
    setCursorPosition(0);
}

void Omnibox::applyPalette(const Palette& p)
{
    m_palette = p;
    refreshSecurityIcon();
}

void Omnibox::refreshSecurityIcon()
{
    const bool secure = m_url.scheme() == QLatin1String("https");
    m_security->setIcon(secure ? icon(Icon::Lock, m_palette.shield) : icon(Icon::Globe, m_palette.faintText));
    m_security->setToolTip(secure ? tr("Conexión segura") : tr("La conexión no está cifrada"));
    m_security->setVisible(!displayUrl(m_url).isEmpty()); // nueva pestaña: sin icono, como Chrome
}

void Omnibox::keyPressEvent(QKeyEvent* event)
{
    m_deleting = event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete;
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        const QAbstractItemView* popup = m_completer->popup();
        const int row = popup->isVisible() && popup->currentIndex().isValid() ? popup->currentIndex().row() : -1;
        m_keyHandled = true; // QCompleter avisa después con activated: ya está hecho
        QTimer::singleShot(0, this, [this] { m_keyHandled = false; });
        activate(row);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        setText(displayUrl(m_url));
        setModified(false);
        clearFocus();
        return;
    }
    QLineEdit::keyPressEvent(event);
}

void Omnibox::focusInEvent(QFocusEvent* event)
{
    QLineEdit::focusInEvent(event);
    // Tras el clic, QLineEdit coloca el cursor; se selecciona todo justo después, como en Chrome.
    QTimer::singleShot(0, this, &QLineEdit::selectAll);
}

} // namespace sora
