#include "ui/TitleBar.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStyle>
#include <QToolButton>
#include <QWindow>

#include "ui/Icons.h"
#include "ui/TabStrip.h"

namespace sora {
namespace {
QToolButton* windowButton(QWidget* parent, const char* name, const QString& tip)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(QLatin1String(name));
    b->setToolTip(tip);
    b->setFixedSize(46, 40);
    b->setIconSize(QSize(10, 10));
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
} // namespace

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent)
    , m_tabs(new TabStrip(this))
    , m_min(windowButton(this, "WindowButton", tr("Minimizar")))
    , m_max(windowButton(this, "WindowButton", tr("Maximizar")))
    , m_close(windowButton(this, "CloseButton", tr("Cerrar")))
    , m_palette(nieblaPalette(Qt::ColorScheme::Light))
{
    setObjectName(QStringLiteral("TitleBar"));
    setAttribute(Qt::WA_StyledBackground);
    setFixedHeight(40);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabs, 1, Qt::AlignBottom);
    layout->addSpacing(40); // hueco mínimo para arrastrar aunque haya muchas pestañas
    layout->addWidget(m_min);
    layout->addWidget(m_max);
    layout->addWidget(m_close);

    connect(m_min, &QToolButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_max, &QToolButton::clicked, this, &TitleBar::maximizeToggleRequested);
    connect(m_close, &QToolButton::clicked, this, &TitleBar::closeRequested);
    refreshIcons();
}

WindowHit TitleBar::hitAt(QPoint local) const
{
    QWidget* w = childAt(local);
    if (!w)
        return WindowHit::Caption;
    if (w == m_min) return WindowHit::Minimize;
    if (w == m_max) return WindowHit::Maximize;
    if (w == m_close) return WindowHit::Close;
    if (w == m_tabs)
        return m_tabs->tabAt(m_tabs->mapFrom(this, local)) >= 0 ? WindowHit::Client : WindowHit::Caption;
    return WindowHit::Client; // "+", × de pestaña, flechas de desplazamiento
}

void TitleBar::setMaximized(bool maximized)
{
    m_maximized = maximized;
    m_max->setToolTip(maximized ? tr("Restaurar") : tr("Maximizar"));
    refreshIcons();
}

void TitleBar::setMaximizeHovered(bool hovered)
{
    if (m_max->property("hovered").toBool() == hovered)
        return;
    m_max->setProperty("hovered", hovered);
    m_max->style()->unpolish(m_max);
    m_max->style()->polish(m_max);
}

void TitleBar::applyPalette(const Palette& p)
{
    m_palette = p;
    m_tabs->applyPalette(p);
    refreshIcons();
}

void TitleBar::refreshIcons()
{
    m_min->setIcon(icon(Icon::Minimize, m_palette.text));
    m_max->setIcon(icon(m_maximized ? Icon::Restore : Icon::Maximize, m_palette.text));
    // Al pasar el ratón el fondo de cerrar es rojo: el icono pasa a blanco (modo Active).
    QIcon close;
    for (int side : {10, 20}) {
        close.addPixmap(icon(Icon::Close, m_palette.text).pixmap(side, side), QIcon::Normal);
        close.addPixmap(icon(Icon::Close, Qt::white).pixmap(side, side), QIcon::Active);
    }
    m_close->setIcon(close);
}

// Fuera de Windows no hay WM_NCHITTEST: el arrastre y el doble clic se hacen aquí.
void TitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && hitAt(event->position().toPoint()) == WindowHit::Caption) {
        if (QWindow* handle = window()->windowHandle())
            handle->startSystemMove();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && hitAt(event->position().toPoint()) == WindowHit::Caption) {
        emit maximizeToggleRequested();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

} // namespace sora
