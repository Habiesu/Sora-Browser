#include "ui/TabStrip.h"

#include <QMouseEvent>
#include <QProxyStyle>
#include <QToolButton>

#include "ui/Icons.h"

namespace sora {
namespace {
constexpr int kMinTab = 60;
constexpr int kMaxTab = 220;
constexpr int kPlusSize = 28;
constexpr int kPlusGap = 6;
constexpr int kPlusReserve = kPlusSize + 2 * kPlusGap;

// Con más pestañas de las que caben, QTabBar pone las flechas al final y las pestañas hasta ellas.
// Se corren a la izquierda para dejarle al "+" su hueco sin tapar ninguna pestaña.
class TabStripStyle : public QProxyStyle {
public:
    QRect subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget) const override
    {
        QRect r = QProxyStyle::subElementRect(element, option, widget);
        if (element == SE_TabBarScrollLeftButton || element == SE_TabBarScrollRightButton)
            r.translate(-kPlusReserve, 0);
        return r;
    }
};
} // namespace

TabStrip::TabStrip(QWidget* parent)
    : QTabBar(parent)
    , m_plus(new QToolButton(this))
    , m_closeColor(nieblaPalette(Qt::ColorScheme::Light).faintText)
{
    setExpanding(false);
    setDrawBase(false);
    setDocumentMode(true);
    setMovable(true);
    setElideMode(Qt::ElideRight);
    setUsesScrollButtons(true);
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* tabStyle = new TabStripStyle;
    tabStyle->setParent(this);
    setStyle(tabStyle);

    m_plus->setObjectName(QStringLiteral("NewTabButton"));
    m_plus->setToolTip(tr("Nueva pestaña (Ctrl+T)"));
    m_plus->setFixedSize(kPlusSize, kPlusSize);
    m_plus->setIconSize(QSize(16, 16));
    m_plus->setIcon(icon(Icon::Plus, nieblaPalette(Qt::ColorScheme::Light).mutedText));
    connect(m_plus, &QToolButton::clicked, this, &TabStrip::newTabRequested);
}

void TabStrip::applyPalette(const Palette& p)
{
    m_closeColor = p.faintText;
    m_plus->setIcon(icon(Icon::Plus, p.mutedText));
    for (int i = 0; i < count(); ++i)
        if (auto* b = qobject_cast<QToolButton*>(tabButton(i, RightSide)))
            b->setIcon(icon(Icon::Close, m_closeColor));
}

QSize TabStrip::tabSizeHint(int index) const
{
    const int available = width() - kPlusSize - 2 * kPlusGap;
    const int each = count() ? available / count() : kMaxTab;
    return {std::clamp(each, kMinTab, kMaxTab), QTabBar::tabSizeHint(index).height()};
}

QSize TabStrip::minimumTabSizeHint(int index) const
{
    return {kMinTab, QTabBar::tabSizeHint(index).height()};
}

void TabStrip::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    auto* close = new QToolButton(this);
    close->setObjectName(QStringLiteral("TabCloseButton"));
    close->setToolTip(tr("Cerrar pestaña (Ctrl+W)"));
    close->setAutoRaise(true);
    close->setFixedSize(18, 18);
    close->setIconSize(QSize(8, 8));
    close->setIcon(icon(Icon::Close, m_closeColor));
    close->setFocusPolicy(Qt::NoFocus);
    // El índice se busca al pulsar: las pestañas se mueven y se cierran.
    connect(close, &QToolButton::clicked, this, [this, close] {
        for (int i = 0; i < count(); ++i)
            if (tabButton(i, RightSide) == close) {
                emit tabCloseRequested(i);
                return;
            }
    });
    setTabButton(index, RightSide, close);
}

void TabStrip::tabLayoutChange()
{
    QTabBar::tabLayoutChange();
    placePlus();
}

void TabStrip::resizeEvent(QResizeEvent* event)
{
    QTabBar::resizeEvent(event);
    setIconSize(iconSize()); // fuerza a recolocar las pestañas con el ancho nuevo
    placePlus();
}

void TabStrip::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        const int index = tabAt(event->position().toPoint());
        if (index >= 0) {
            emit tabCloseRequested(index);
            return;
        }
    }
    QTabBar::mouseReleaseEvent(event);
}

void TabStrip::placePlus()
{
    // Justo tras la última pestaña; si no caben todas, en el hueco reservado tras las flechas.
    int x = count() ? tabRect(count() - 1).right() + 1 + kPlusGap : kPlusGap;
    x = std::clamp(x, 0, std::max(0, width() - kPlusSize - kPlusGap));
    m_plus->move(x, (height() - kPlusSize) / 2 + 3);
    m_plus->raise();
}

} // namespace sora
