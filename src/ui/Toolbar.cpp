#include "ui/Toolbar.h"

#include <QHBoxLayout>
#include <QEvent>
#include <QMenu>
#include <QProxyStyle>
#include <QScreen>
#include <QToolButton>

#include "ui/Icons.h"
#include "ui/MainMenu.h"
#include "ui/Omnibox.h"

namespace sora {

namespace {
class ScrollableMenuStyle : public QProxyStyle {
public:
    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget, QStyleHintReturn* ret) const override
    {
        return hint == SH_Menu_Scrollable ? 1 : QProxyStyle::styleHint(hint, option, widget, ret);
    }
};
} // namespace
namespace {
QToolButton* toolButton(QWidget* parent, const QString& tip)
{
    auto* b = new QToolButton(parent);
    b->setToolTip(tip);
    b->setFixedSize(32, 32);
    b->setIconSize(QSize(16, 16));
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
} // namespace

Toolbar::Toolbar(QWidget* parent)
    : QWidget(parent)
    , m_back(toolButton(this, tr("Atrás (Alt+Izquierda)")))
    , m_forward(toolButton(this, tr("Adelante (Alt+Derecha)")))
    , m_reload(toolButton(this, tr("Recargar (F5)")))
    , m_omnibox(new Omnibox(this))
    , m_shield(toolButton(this, {}))
    , m_shieldMenu(new QMenu(this))
    , m_star(toolButton(this, tr("Añadir esta página a marcadores (Ctrl+D)")))
    , m_extensions(toolButton(this, tr("Extensiones")))
    , m_extensionsMenu(new QMenu(this))
    , m_menu(toolButton(this, tr("Personaliza y controla Sora")))
    , m_palette(nieblaPalette(Qt::ColorScheme::Light))
{
    setObjectName(QStringLiteral("Toolbar"));
    setAttribute(Qt::WA_StyledBackground);
    setFixedHeight(40);
    m_reload->setObjectName(QStringLiteral("ReloadButton"));
    m_shield->setObjectName(QStringLiteral("ShieldButton"));
    m_shield->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_shield->setMinimumWidth(48); // el número crece con los bloqueos
    m_shield->setMaximumWidth(QWIDGETSIZE_MAX);
    m_shield->setMenu(m_shieldMenu);
    m_shield->setPopupMode(QToolButton::InstantPopup);
    m_star->setObjectName(QStringLiteral("StarButton"));
    m_star->setCheckable(true);
    connect(m_star, &QToolButton::clicked, this, &Toolbar::starClicked);

    m_extensions->setMenu(m_extensionsMenu);
    m_extensions->setPopupMode(QToolButton::InstantPopup);

    m_mainMenu = new MainMenu(this);
    m_menu->setMenu(m_mainMenu);
    // Como en Chrome: los menús de la derecha se abren hacia dentro de la ventana.
    // Una sola columna que se desplaza (flecha abajo) si no cabe, en vez de partirse en columnas.
    auto* scrollable = new ScrollableMenuStyle;
    scrollable->setParent(this);
    for (QMenu* menu : {static_cast<QMenu*>(m_mainMenu), m_extensionsMenu, m_shieldMenu}) {
        menu->installEventFilter(this);
        menu->setStyle(scrollable);
        // Como mucho hasta el borde inferior de la ventana.
        connect(menu, &QMenu::aboutToShow, this, [this, menu] {
            const int bottom = window()->mapToGlobal(QPoint(0, window()->height())).y();
            const int top = mapToGlobal(QPoint(0, height())).y();
            menu->setMaximumHeight(std::max(120, bottom - top - 8));
        });
    }
    m_menu->setPopupMode(QToolButton::InstantPopup);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(2);
    layout->addWidget(m_back);
    layout->addWidget(m_forward);
    layout->addWidget(m_reload);
    layout->addSpacing(4);
    layout->addWidget(m_omnibox, 1);
    layout->addSpacing(4);
    layout->addWidget(m_shield);
    layout->addWidget(m_star);
    layout->addWidget(m_extensions);
    layout->addWidget(m_menu);

    connect(m_back, &QToolButton::clicked, this, &Toolbar::backRequested);
    connect(m_forward, &QToolButton::clicked, this, &Toolbar::forwardRequested);
    connect(m_reload, &QToolButton::clicked, this, [this] { emit m_loading ? stopRequested() : reloadRequested(); });

    setNavigation(false, false, false);
    refreshIcons();
    setBlockedCount(0);
}

bool Toolbar::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Show) {
        auto* menu = static_cast<QMenu*>(watched);
        const QToolButton* button = menu == m_mainMenu ? m_menu : menu == m_extensionsMenu ? m_extensions : m_shield;
        // El borde derecho del menú, bajo el borde derecho de su botón (sin salirse de la pantalla).
        const QPoint corner = button->mapToGlobal(QPoint(button->width(), button->height()));
        const QRect screen = button->screen()->availableGeometry();
        menu->move(std::max(screen.left(), corner.x() - menu->width()), menu->y());
    }
    return QWidget::eventFilter(watched, event);
}

void Toolbar::setNavigation(bool canBack, bool canForward, bool loading)
{
    m_back->setEnabled(canBack);
    m_forward->setEnabled(canForward);
    if (m_loading == loading)
        return; // llega en cada cambio de dirección: sin cambios, no se rehacen los iconos
    m_loading = loading;
    m_reload->setToolTip(loading ? tr("Parar (Esc)") : tr("Recargar (F5)"));
    m_reload->setIcon(icon(m_loading ? Icon::Stop : Icon::Reload, m_palette.text));
}

void Toolbar::setBlockedCount(int count)
{
    m_shield->setText(m_shieldActive ? QString::number(count) : QString());
    m_shield->setToolTip(m_shieldActive ? tr("Bloqueados en esta página: %1").arg(count)
                                        : tr("El escudo está desactivado en este sitio"));
}

void Toolbar::setBookmarked(bool bookmarked)
{
    m_star->setChecked(bookmarked);
    m_star->setToolTip(bookmarked ? tr("Quitar de marcadores (Ctrl+D)") : tr("Añadir esta página a marcadores (Ctrl+D)"));
    refreshIcons();
}

void Toolbar::applyPalette(const Palette& p)
{
    m_palette = p;
    m_omnibox->applyPalette(p);
    refreshIcons();
}

void Toolbar::refreshIcons()
{
    m_back->setIcon(icon(Icon::Back, m_palette.text));
    m_forward->setIcon(icon(Icon::Forward, m_palette.text));
    m_reload->setIcon(icon(m_loading ? Icon::Stop : Icon::Reload, m_palette.text));
    m_shield->setIcon(icon(Icon::Shield, m_shieldActive ? m_palette.shield : m_palette.faintText));
    m_star->setIcon(m_star->isChecked() ? icon(Icon::StarFilled, m_palette.shield) : icon(Icon::Star, m_palette.text));
    m_extensions->setIcon(icon(Icon::Puzzle, m_palette.text));
    m_menu->setIcon(icon(Icon::Menu, m_palette.text));
}

} // namespace sora

namespace sora {
void Toolbar::setExtensionsVisible(bool visible)
{
    m_extensions->setVisible(visible);
}

void Toolbar::setShieldActive(bool active)
{
    if (m_shieldActive == active)
        return;
    m_shieldActive = active;
    m_shield->setIcon(icon(Icon::Shield, m_shieldActive ? m_palette.shield : m_palette.faintText));
}
} // namespace sora
