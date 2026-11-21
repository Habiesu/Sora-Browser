#include "ui/Omnibox.h"

#include <QAction>
#include <QKeyEvent>
#include <QTimer>

#include "ui/Icons.h"

namespace sora {

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
    connect(this, &QLineEdit::returnPressed, this, [this] {
        setModified(false);
        emit submitted(text());
    });
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
