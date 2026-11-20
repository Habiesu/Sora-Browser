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

signals:
    void newTabRequested();

protected:
    QSize tabSizeHint(int index) const override;
    QSize minimumTabSizeHint(int index) const override;
    void tabInserted(int index) override;
    void tabLayoutChange() override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QToolButton* m_plus;
    QColor m_closeColor;
    void placePlus();
};

} // namespace sora
