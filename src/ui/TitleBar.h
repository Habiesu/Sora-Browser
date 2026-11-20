#pragma once
#include <QWidget>
#include "ui/Theme.h"
#include "ui/WindowFrame.h"

class QToolButton;

namespace sora {

class TabStrip;

// Barra de título Niebla: pestañas a la izquierda y minimizar/maximizar/cerrar a la derecha.
class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget* parent = nullptr);

    TabStrip* tabs() const { return m_tabs; }
    QToolButton* minimizeButton() const { return m_min; }
    QToolButton* maximizeButton() const { return m_max; }
    QToolButton* closeButton() const { return m_close; }

    // Botones → su valor; pestaña, "+" o flechas → Client; hueco libre → Caption (arrastrar).
    WindowHit hitAt(QPoint local) const;
    void setMaximized(bool maximized);
    // En Windows el ratón sobre maximizar llega como evento no cliente (Snap Layouts).
    void setMaximizeHovered(bool hovered);
    void applyPalette(const Palette& p);

signals:
    void minimizeRequested();
    void maximizeToggleRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    TabStrip* m_tabs;
    QToolButton* m_min;
    QToolButton* m_max;
    QToolButton* m_close;
    Palette m_palette;
    bool m_maximized = false;
    void refreshIcons();
};

} // namespace sora
