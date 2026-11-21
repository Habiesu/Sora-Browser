#pragma once
#include <QTabBar>
#include "ui/Theme.h"

class QProxyStyle;
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
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_dragging = false;
    int m_pressX = 0;       // dónde se pulsó la pestaña que se arrastra
    int m_pressTabRight = 0; // y dónde acababa entonces
    QToolButton* m_plus;
    QProxyStyle* m_style;
    QColor m_closeColor;
    QColor m_lineColor;
    void placePlus(int draggedRight = -1);
};

} // namespace sora
