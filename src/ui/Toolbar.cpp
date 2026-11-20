#include "ui/Toolbar.h"

#include <QHBoxLayout>
#include <QMenu>
#include <QToolButton>

#include "ui/Icons.h"
#include "ui/Omnibox.h"

namespace sora {
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
    , m_star(toolButton(this, tr("Marcadores: llegan en el próximo hito")))
    , m_menu(toolButton(this, tr("Menú")))
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
    m_star->setEnabled(false);

    auto* menu = new QMenu(this);
    menu->addAction(tr("Nueva pestaña\tCtrl+T"), this, &Toolbar::newTabRequested);
    menu->addAction(tr("Cerrar pestaña\tCtrl+W"), this, &Toolbar::closeTabRequested);
    menu->addSeparator();
    menu->addAction(tr("Salir"), this, &Toolbar::quitRequested);
    m_menu->setMenu(menu);
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
    layout->addWidget(m_menu);

    connect(m_back, &QToolButton::clicked, this, &Toolbar::backRequested);
    connect(m_forward, &QToolButton::clicked, this, &Toolbar::forwardRequested);
    connect(m_reload, &QToolButton::clicked, this, [this] { emit m_loading ? stopRequested() : reloadRequested(); });

    setNavigation(false, false, false);
    setBlockedCount(0);
}

void Toolbar::setNavigation(bool canBack, bool canForward, bool loading)
{
    m_back->setEnabled(canBack);
    m_forward->setEnabled(canForward);
    m_loading = loading;
    m_reload->setToolTip(loading ? tr("Parar (Esc)") : tr("Recargar (F5)"));
    refreshIcons();
}

void Toolbar::setBlockedCount(int count)
{
    m_shield->setText(QString::number(count));
    m_shield->setToolTip(tr("Bloqueados en esta página: %1").arg(count));
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
    m_shield->setIcon(icon(Icon::Shield, m_palette.shield));
    m_star->setIcon(icon(Icon::Star, m_palette.text));
    m_menu->setIcon(icon(Icon::Menu, m_palette.text));
}

} // namespace sora
