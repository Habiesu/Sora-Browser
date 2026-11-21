#pragma once
#include <QTabBar>
#include "ui/Theme.h"

class QToolButton;

namespace sora {

// Pestañas Niebla en la barra de título: de 60 a 220 px, "+" justo detrás de la última,
// × dibujada con Icons y cierre con el botón central del ratón.
class TabStrip : public QTabBar {
    Q_OBJECT
public:
    explicit TabStrip(QWidget* parent = nullptr);
    void applyPalette(const Palette& p);
    // Icono de la web; sin favicon se pone uno genérico (sin icono, Qt centraría el título).
    void setPageIcon(int index, const QIcon& icon);

signals:
    void newTabRequested();
    // Pestaña arrastrada y soltada fuera de la tira (posición global del ratón).
    void tabDetachRequested(int index, QPoint globalPos);

protected:
    QSize tabSizeHint(int index) const override;
    QSize minimumTabSizeHint(int index) const override;
    void tabInserted(int index) override;
    void tabLayoutChange() override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_dragging = false;
    QToolButton* m_plus;
    QColor m_closeColor;
    QColor m_lineColor;
    void placePlus();
};

} // namespace sora
