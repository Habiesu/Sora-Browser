#include "ui/BookmarksBar.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QToolButton>

#include "data/BookmarkStore.h"

namespace sora {
namespace {
// Botón de marcador que también avisa del clic central (abrir en pestaña nueva, como Chrome).
class BookmarkButton : public QToolButton {
public:
    using QToolButton::QToolButton;
    std::function<void()> onMiddleClick;

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::MiddleButton && onMiddleClick) {
            onMiddleClick();
            return;
        }
        QToolButton::mouseReleaseEvent(event);
    }
};
} // namespace

BookmarksBar::BookmarksBar(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QHBoxLayout(this))
    , m_empty(new QLabel(tr("Para acceder rápido a tus páginas, añádelas a marcadores con la estrella (Ctrl+D)"), this))
{
    setObjectName(QStringLiteral("BookmarksBar"));
    setAttribute(Qt::WA_StyledBackground);
    setFixedHeight(30);
    m_layout->setContentsMargins(10, 0, 10, 0);
    m_layout->setSpacing(2);
    m_layout->addWidget(m_empty);
    m_layout->addStretch();
}

void BookmarksBar::setStore(BookmarkStore* store)
{
    if (m_store)
        m_store->disconnect(this);
    m_store = store;
    if (m_store)
        connect(m_store, &BookmarkStore::changed, this, &BookmarksBar::rebuild);
    rebuild();
}

void BookmarksBar::rebuild()
{
    // Quita los botones anteriores (deja la etiqueta de vacío y el estirador del final).
    for (auto* b : findChildren<QToolButton*>(Qt::FindDirectChildrenOnly))
        delete b;
    const auto bookmarks = m_store ? m_store->all() : QList<BookmarkStore::Bookmark>{};
    m_empty->setVisible(bookmarks.isEmpty());
    // shortcut: los que no caben quedan fuera de la vista; con muchos marcadores, añadir el botón "»".
    int at = 0;
    for (const auto& bm : bookmarks) {
        auto* b = new BookmarkButton(this);
        b->setText(fontMetrics().elidedText(bm.title, Qt::ElideRight, 160));
        b->setToolTip(bm.title + QLatin1Char('\n') + bm.url.toDisplayString());
        b->setAutoRaise(true);
        b->setFocusPolicy(Qt::NoFocus);
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        const QUrl url = bm.url;
        const qint64 id = bm.id;
        connect(b, &QToolButton::clicked, this, [this, url] { emit openRequested(url, false); });
        b->onMiddleClick = [this, url] { emit openRequested(url, true); };
        connect(b, &QWidget::customContextMenuRequested, this, [this, b, url, id, title = bm.title] {
            QMenu menu(this);
            menu.addAction(tr("Abrir en una pestaña nueva"), this, [this, url] { emit openRequested(url, true); });
            menu.addSeparator();
            menu.addAction(tr("Editar nombre..."), this, [this, id, title] {
                bool ok = false;
                const QString renamed = QInputDialog::getText(this, tr("Editar marcador"), tr("Nombre:"),
                                                              QLineEdit::Normal, title, &ok);
                if (ok && m_store)
                    m_store->rename(id, renamed);
            });
            menu.addAction(tr("Eliminar"), this, [this, id] { if (m_store) m_store->remove(id); });
            menu.exec(b->mapToGlobal(QPoint(0, b->height())));
        });
        m_layout->insertWidget(at++, b);
    }
}

} // namespace sora
