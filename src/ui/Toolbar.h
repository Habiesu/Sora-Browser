#pragma once
#include <QWidget>
#include "ui/Theme.h"

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
    void setNavigation(bool canBack, bool canForward, bool loading);
    void setBlockedCount(int count);
    void applyPalette(const Palette& p);

signals:
    void backRequested();
    void forwardRequested();
    void reloadRequested();
    void stopRequested();

private:
    QToolButton* m_back;
    QToolButton* m_forward;
    QToolButton* m_reload;
    Omnibox* m_omnibox;
    QToolButton* m_shield;
    QToolButton* m_star;
    QToolButton* m_menu;
    MainMenu* m_mainMenu;
    Palette m_palette;
    bool m_loading = false;
    void refreshIcons();
};

} // namespace sora
