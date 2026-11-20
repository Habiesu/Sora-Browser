#pragma once
#include <QWidget>

namespace sora {

// Barra de favoritos Niebla. Vacía hasta el Hito 3 (marcadores).
class BookmarksBar : public QWidget {
    Q_OBJECT
public:
    explicit BookmarksBar(QWidget* parent = nullptr);
};

} // namespace sora
