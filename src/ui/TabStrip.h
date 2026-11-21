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

    // Arrastres como en Chrome. La ventana nueva de una pestaña separada (o la de una sola pestaña)
    // sigue al ratón hasta soltar; una pestaña que llega desde otra ventana se sigue arrastrando aquí.
    void beginWindowDrag(QPoint globalPos);
    void endWindowDrag();
    void beginTabDrag(int index, QPoint globalPos);
    bool isDraggingWindow() const { return m_windowDrag; }
    bool isDraggingTab() const { return m_dragging && m_tabMoved; }
    QRect draggedTabRect() const; // dónde se ve la pestaña que se arrastra

signals:
    void newTabRequested();
    // Pestaña arrastrada y soltada fuera de la tira (posición global del ratón).
    void tabDetachRequested(int index, QPoint globalPos); // salió de la tira: separarla ya
    void windowDragMoved(QPoint globalPos); // la ventana va con el ratón (¿encima de otra tira?)

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
    bool event(QEvent* event) override;

private:
    bool m_dragging = false;
    bool m_tabMoved = false;
    bool m_windowDrag = false;
    int m_pressX = 0;        // dónde se pulsó la pestaña que se arrastra
    int m_pressTabLeft = 0;  // y dónde empezaba y acababa entonces
    int m_pressTabRight = 0;
    QPoint m_pressGlobal;
    QPoint m_lastPos;        // último punto (ya con el tope) dado a QTabBar
    QPoint m_grabOffset;     // del ratón a la esquina de la ventana que se arrastra
    void finishTabDrag();
    int maxTabRight() const;
    QToolButton* m_plus;
    QProxyStyle* m_style;
    QColor m_closeColor;
    QColor m_lineColor;
    void placePlus(int draggedRight = -1);
};

} // namespace sora
