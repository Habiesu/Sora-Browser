#pragma once
#include <QWidget>
#include "ui/Theme.h"

class QMenu;
class QToolButton;

namespace sora {

class MainMenu;
class Omnibox;

// Barra de herramientas Niebla: atrás, adelante, recargar/parar, dirección, escudo, estrella, menú.
class Toolbar : public QWidget {
    Q_OBJECT
public:
    explicit Toolbar(QWidget* parent = nullptr);

    Omnibox* omnibox() const { return m_omnibox; }
    MainMenu* mainMenu() const { return m_mainMenu; }
    // Menú del botón de extensiones (puzle); quien lo usa lo rellena en aboutToShow.
    QMenu* extensionsMenu() const { return m_extensionsMenu; }
    void setExtensionsVisible(bool visible);
    // Menú del escudo; quien lo usa lo rellena en aboutToShow.
    QMenu* shieldMenu() const { return m_shieldMenu; }
    void setShieldActive(bool active); // apagado en este sitio: escudo gris y sin número
    void setNavigation(bool canBack, bool canForward, bool loading);
    void setBlockedCount(int count);
    void setBookmarked(bool bookmarked);
    void applyPalette(const Palette& p);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void backRequested();
    void forwardRequested();
    void reloadRequested();
    void stopRequested();
    void starClicked();

private:
    QToolButton* m_back;
    QToolButton* m_forward;
    QToolButton* m_reload;
    Omnibox* m_omnibox;
    QToolButton* m_shield;
    QMenu* m_shieldMenu;
    bool m_shieldActive = true;
    QToolButton* m_star;
    QToolButton* m_extensions;
    QMenu* m_extensionsMenu;
    QToolButton* m_menu;
    MainMenu* m_mainMenu;
    Palette m_palette;
    bool m_loading = false;
    void refreshIcons();
};

} // namespace sora
