#include "ui/TabStrip.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QStyleOptionTab>
#include <QToolButton>

#include "ui/Icons.h"

namespace sora {
namespace {
constexpr int kMinTab = 60;
constexpr int kMaxTab = 220;
constexpr int kPlusSize = 32; // casi el alto de la pestaña visible (34), como en Chrome
constexpr int kPlusGap = 4;
constexpr int kDragGap = 48; // como en Chrome: siempre queda barra libre para arrastrar la ventana
constexpr int kPlusReserve = kPlusSize + 2 * kPlusGap + kDragGap;
constexpr int kTabTopMargin = 6; // hueco sobre la pestaña: se ve la barra de título
constexpr int kTabHeight = 40;
constexpr int kDetachDistance = 30; // px fuera de la tira para separar la pestaña

// Estilo propio de las pestañas: Sora las dibuja enteras (fondo, icono y título a la izquierda,
// como Chrome). La hoja de estilos de Qt centraba el título; así además la pestaña que se arrastra
// (Qt la dibuja aparte) se ve igual que las demás.
class TabStripStyle : public QProxyStyle {
public:
    Palette palette = nieblaPalette(Qt::ColorScheme::Light);

    // Con más pestañas de las que caben, las flechas se corren a la izquierda: hueco para el "+".
    // La × se centra en la parte visible de la pestaña (Qt la centra en todo el alto, con el hueco
    // de arriba); al hacerlo aquí vale también mientras se arrastra.
    QRect subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget) const override
    {
        QRect r = QProxyStyle::subElementRect(element, option, widget);
        if (element == SE_TabBarScrollLeftButton || element == SE_TabBarScrollRightButton)
            r.translate(-kPlusReserve, 0);
        if (element == SE_TabBarTabRightButton && r.isValid()) {
            const QRect visible = option->rect.adjusted(0, kTabTopMargin, 0, 0);
            r.moveTop(visible.center().y() - r.height() / 2 + 1);
        }
        return r;
    }

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget) const override
    {
        const auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option);
        if (!tab || (element != CE_TabBarTabShape && element != CE_TabBarTabLabel && element != CE_TabBarTab)) {
            QProxyStyle::drawControl(element, option, painter, widget);
            return;
        }
        if (element == CE_TabBarTab) {
            drawControl(CE_TabBarTabShape, option, painter, widget);
            drawControl(CE_TabBarTabLabel, option, painter, widget);
            return;
        }
        const bool selected = tab->state & State_Selected;
        const QRect visible = tab->rect.adjusted(1, kTabTopMargin, -1, 0);
        if (element == CE_TabBarTabShape) {
            const QColor fill = selected ? palette.surface
                              : (tab->state & State_MouseOver) ? palette.hover : palette.window;
            QPainterPath path;
            path.addRoundedRect(visible.adjusted(0, 0, 0, 8), 8, 8); // solo redondeada arriba
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setClipRect(visible);
            painter->fillPath(path, fill);
            painter->restore();
            return;
        }
        const QSize iconSize = tab->iconSize.isValid() ? tab->iconSize : QSize(16, 16);
        const QRect iconRect(QPoint(visible.left() + 10, visible.center().y() - iconSize.height() / 2 + 1), iconSize);
        if (!tab->icon.isNull())
            tab->icon.paint(painter, iconRect);
        QRect textRect(iconRect.right() + 9, visible.top(), 0, visible.height());
        textRect.setRight(visible.right() - (tab->rightButtonSize.isValid() ? tab->rightButtonSize.width() + 10 : 8));
        painter->save();
        painter->setPen(selected ? palette.text : palette.mutedText);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(tab->text, Qt::ElideRight, textRect.width()));
        painter->restore();
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
    m_style = new TabStripStyle;
    m_style->setParent(this);
    setStyle(m_style);

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
    static_cast<TabStripStyle*>(m_style)->palette = p;
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
    const int available = width() - kPlusReserve;
    const int each = count() ? available / count() : kMaxTab;
    Q_UNUSED(index);
    return {std::clamp(each, kMinTab, kMaxTab), kTabHeight};
}

QSize TabStrip::minimumTabSizeHint(int index) const
{
    Q_UNUSED(index);
    return {kMinTab, kTabHeight};
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
    placePlus();
}

void TabStrip::resizeEvent(QResizeEvent* event)
{
    QTabBar::resizeEvent(event);
    setIconSize(iconSize()); // fuerza a recolocar las pestañas con el ancho nuevo
    placePlus();
}

void TabStrip::mousePressEvent(QMouseEvent* event)
{
    const int index = tabAt(event->position().toPoint());
    m_dragging = event->button() == Qt::LeftButton && index >= 0;
    m_tabMoved = false;
    if (m_dragging) {
        m_pressX = event->position().toPoint().x();
        m_pressTabLeft = tabRect(index).left();
        m_pressTabRight = tabRect(index).right();
        m_pressGlobal = event->globalPosition().toPoint();
        m_lastPos = event->position().toPoint();
    }
    QTabBar::mousePressEvent(event);
}

int TabStrip::maxTabRight() const
{
    return width() - kPlusReserve - 1; // el "+" y el hueco para arrastrar la ventana siempre a la vista
}

QRect TabStrip::draggedTabRect() const
{
    const int width = currentIndex() >= 0 ? tabRect(currentIndex()).width() : 0;
    return {m_pressTabLeft + m_lastPos.x() - m_pressX, 0, width, height()};
}

void TabStrip::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    const QPoint global = event->globalPosition().toPoint();
    if (m_windowDrag) {
        window()->move(global - m_grabOffset);
        emit windowDragMoved(global);
        return;
    }
    if (!m_dragging || !(event->buttons() & Qt::LeftButton) || !isMovable()) {
        QTabBar::mouseMoveEvent(event);
        return;
    }
    // Como en Chrome: fuera de la tira (arriba, abajo o pasados los botones) la pestaña se separa ya.
    const bool outside = pos.y() < -kDetachDistance || pos.y() > height() + kDetachDistance
                         || pos.x() < -kDetachDistance || pos.x() > width() + kDetachDistance;
    if (outside) {
        const int index = currentIndex();
        finishTabDrag();
        if (count() == 1) { // la ventana entera va con el ratón, con la pestaña bajo él
            m_grabOffset = m_pressGlobal - window()->pos();
            m_windowDrag = true;
            window()->move(global - m_grabOffset);
            emit windowDragMoved(global);
        } else {
            emit tabDetachRequested(index, global);
        }
        return;
    }
    // Dentro, la pestaña se frena antes del "+" y del hueco libre, y no pasa del borde izquierdo.
    const int dx = std::clamp(pos.x() - m_pressX, -m_pressTabLeft, maxTabRight() - m_pressTabRight);
    m_lastPos = QPoint(m_pressX + dx, pos.y());
    m_tabMoved = m_tabMoved || qAbs(dx) >= QApplication::startDragDistance();
    QMouseEvent clamped(event->type(), m_lastPos, mapToGlobal(m_lastPos), event->button(), event->buttons(),
                        event->modifiers());
    QTabBar::mouseMoveEvent(&clamped);
    placePlus(m_pressTabRight + dx); // el "+" va pegado a la pestaña que se arrastra
}

void TabStrip::finishTabDrag()
{
    if (!m_dragging)
        return;
    m_dragging = false;
    m_tabMoved = false;
    // QTabBar acaba su arrastre como al soltar: la pestaña vuelve deslizándose a su sitio.
    QMouseEvent release(QEvent::MouseButtonRelease, m_lastPos, mapToGlobal(m_lastPos), Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QTabBar::mouseReleaseEvent(&release);
    QMetaObject::invokeMethod(this, [this] { placePlus(); }, Qt::QueuedConnection);
}

void TabStrip::beginWindowDrag(QPoint globalPos)
{
    m_grabOffset = globalPos - window()->pos();
    m_windowDrag = true;
    grabMouse(); // el ratón sigue pulsado: sus movimientos y el soltar llegan aquí
}

void TabStrip::endWindowDrag()
{
    if (!m_windowDrag)
        return;
    m_windowDrag = false;
    if (mouseGrabber() == this)
        releaseMouse();
}

void TabStrip::beginTabDrag(int index, QPoint globalPos)
{
    setCurrentIndex(index);
    const QRect r = tabRect(index);
    const QPoint local = mapFromGlobal(globalPos);
    const QPoint at(std::clamp(local.x(), r.left() + 4, r.right() - 4), r.center().y());
    QMouseEvent press(QEvent::MouseButtonPress, at, mapToGlobal(at), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    mousePressEvent(&press);
    grabMouse();
}

bool TabStrip::event(QEvent* event)
{
    // Alt+Tab (u otra ventana delante) en pleno arrastre: el soltar no llegará aquí, se termina ya.
    if (event->type() == QEvent::WindowDeactivate) {
        finishTabDrag();
        endWindowDrag();
    }
    return QTabBar::event(event);
}

void TabStrip::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_windowDrag) {
        endWindowDrag();
        return;
    }
    if (event->button() == Qt::LeftButton && m_dragging) {
        finishTabDrag();
        if (mouseGrabber() == this)
            releaseMouse(); // venía de beginTabDrag
        return;
    }
    if (event->button() == Qt::MiddleButton) {
        const int index = tabAt(event->position().toPoint());
        if (index >= 0) {
            emit tabCloseRequested(index);
            return;
        }
    }
    QTabBar::mouseReleaseEvent(event);
}

void TabStrip::placePlus(int draggedRight)
{
    // Justo tras la última pestaña (o tras la que se arrastra, si va más allá); si no caben todas,
    // en el hueco reservado tras las flechas.
    const int lastRight = count() ? std::max(tabRect(count() - 1).right(), draggedRight) : -1;
    int x = lastRight + 1 + kPlusGap;
    x = std::clamp(x, 0, std::max(0, width() - kPlusSize - kPlusGap - kDragGap));
    // Centrado en la parte visible de la pestaña (bajo el hueco de la barra de título).
    m_plus->move(x, kTabTopMargin + (height() - kTabTopMargin - kPlusSize) / 2);
    m_plus->raise();
}

} // namespace sora
