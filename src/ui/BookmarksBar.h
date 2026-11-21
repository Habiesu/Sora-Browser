#pragma once
#include <QPointer>
#include <QUrl>
#include <QWidget>

class QHBoxLayout;
class QLabel;

namespace sora {

class BookmarkStore;

// Barra de favoritos: un botón por marcador, en el orden del almacén. Clic abre aquí; clic central
// o "Abrir en una pestaña nueva" abre aparte. Se rehace cuando el almacén cambia.
class BookmarksBar : public QWidget {
    Q_OBJECT
public:
    explicit BookmarksBar(QWidget* parent = nullptr);
    void setStore(BookmarkStore* store);

signals:
    void openRequested(const QUrl& url, bool newTab);

private:
    QPointer<BookmarkStore> m_store;
    QHBoxLayout* m_layout;
    QLabel* m_empty;
    void rebuild();
};

} // namespace sora
