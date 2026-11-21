#include "ui/TabStrip.h"

#include <QMouseEvent>
#include <QPainter>
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
constexpr int kTabTopMargin = 6; // margin-top de QTabBar::tab en nieblaStyleSheet

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
    , m_lineColor(nieblaPalette(Qt::ColorScheme::Light).line)
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

void TabStrip::paintEvent(QPaintEvent* event)
{
    QTabBar::paintEvent(event);
    // Separadores como en Chrome: solo entre dos pestañas que no son la activa.
    QPainter painter(this);
    painter.setPen(m_lineColor);
    for (int i = 0; i + 1 < count(); ++i) {
        if (i == currentIndex() || i + 1 == currentIndex())
            continue;
        const QRect r = tabRect(i);
        painter.drawLine(r.right(), r.top() + kTabTopMargin + 8, r.right(), r.bottom() - 8);
    }
}

void TabStrip::applyPalette(const Palette& p)
{
    m_closeColor = p.faintText;
    m_lineColor = p.line;
    update();
    m_plus->setIcon(icon(Icon::Plus, p.mutedText));
    for (int i = 0; i < count(); ++i) {
        if (auto* b = qobject_cast<QToolButton*>(tabButton(i, RightSide)))
            b->setIcon(icon(Icon::Close, m_closeColor));
        if (tabData(i).toBool())
            setPageIcon(i, {});
    }
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

void TabStrip::setPageIcon(int index, const QIcon& pageIcon)
{
    setTabIcon(index, pageIcon.isNull() ? icon(Icon::Globe, m_closeColor) : pageIcon);
    setTabData(index, pageIcon.isNull()); // true = genérico, se repinta con el tema
}

void TabStrip::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    if (tabIcon(index).isNull())
        setPageIcon(index, {});
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
    // QTabBar centra la × en todo el alto, incluido el margen superior de la hoja de estilos.
    for (int i = 0; i < count(); ++i)
        if (QWidget* close = tabButton(i, RightSide)) {
            const QRect visible = tabRect(i).adjusted(0, kTabTopMargin, 0, 0);
            close->move(close->x(), visible.center().y() - close->height() / 2 + 1);
        }
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
