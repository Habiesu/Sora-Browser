#include "ui/Icons.h"

#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>

namespace sora {
namespace {

struct Shape {
    QPainterPath path;
    qreal box = 24;    // lado de la caja de diseño
    qreal width = 2;   // grosor del trazo
    bool fill = false; // relleno en vez de trazo
};

QPainterPath polyline(std::initializer_list<QPointF> points)
{
    QPainterPath p;
    auto it = points.begin();
    p.moveTo(*it);
    for (++it; it != points.end(); ++it)
        p.lineTo(*it);
    return p;
}

Shape shapeFor(Icon which)
{
    Shape s;
    QPainterPath& p = s.path;
    switch (which) {
    case Icon::Back: p = polyline({{15, 18}, {9, 12}, {15, 6}}); break;
    case Icon::Forward: p = polyline({{9, 18}, {15, 12}, {9, 6}}); break;
    case Icon::Reload:
        // Círculo de radio 9 abierto arriba a la derecha, con la flecha en (21, 8).
        p.moveTo(21, 12);
        p.arcTo(QRectF(3, 3, 18, 18), 0, -312);
        p.lineTo(21, 8);
        p.addPath(polyline({{21, 3}, {21, 8}, {16, 8}}));
        break;
    case Icon::Stop:
        p = polyline({{6, 6}, {18, 18}});
        p.addPath(polyline({{18, 6}, {6, 18}}));
        break;
    case Icon::Lock:
        p.addRoundedRect(QRectF(5, 11, 14, 10), 2, 2);
        p.moveTo(8, 11);
        p.lineTo(8, 7);
        p.arcTo(QRectF(8, 3, 8, 8), 180, -180);
        p.lineTo(16, 11);
        s.width = 2.2;
        break;
    case Icon::Globe:
        p.addEllipse(QPointF(12, 12), 9, 9);
        p.addEllipse(QPointF(12, 12), 4, 9);
        p.addPath(polyline({{3, 12}, {21, 12}}));
        s.width = 1.6;
        break;
    case Icon::Shield:
        p.moveTo(12, 2);
        p.lineTo(20, 5);
        p.lineTo(20, 11);
        p.cubicTo(20, 16, 16.6, 20.4, 12, 22);
        p.cubicTo(7.4, 20.4, 4, 16, 4, 11);
        p.lineTo(4, 5);
        p.closeSubpath();
        s.fill = true;
        break;
    case Icon::StarFilled:
        s.fill = true;
        [[fallthrough]];
    case Icon::Star:
        p = polyline({{12, 3}, {14.7, 8.6}, {20.8, 9.5}, {16.4, 13.8}, {17.4, 19.9},
                      {12, 17}, {6.6, 19.9}, {7.6, 13.8}, {3.2, 9.5}, {9.3, 8.6}});
        p.closeSubpath();
        s.width = 1.8;
        break;
    case Icon::Menu:
        for (qreal x : {5.0, 12.0, 19.0})
            p.addEllipse(QPointF(x, 12), 1.8, 1.8);
        s.fill = true;
        break;
    case Icon::Plus:
        p = polyline({{12, 5}, {12, 19}});
        p.addPath(polyline({{5, 12}, {19, 12}}));
        break;
    // Botones de ventana: caja de 12 con trazo fino, como en Windows 11.
    case Icon::Close:
        p = polyline({{1.5, 1.5}, {10.5, 10.5}});
        p.addPath(polyline({{10.5, 1.5}, {1.5, 10.5}}));
        s.box = 12; s.width = 1;
        break;
    case Icon::Minimize:
        p = polyline({{1.5, 6}, {10.5, 6}});
        s.box = 12; s.width = 1;
        break;
    case Icon::Maximize:
        p.addRect(QRectF(1.5, 1.5, 9, 9));
        s.box = 12; s.width = 1;
        break;
    case Icon::Restore:
        p.addRect(QRectF(1.5, 3.5, 7, 7));
        p.addPath(polyline({{3.5, 3.5}, {3.5, 1.5}, {10.5, 1.5}, {10.5, 8.5}, {8.5, 8.5}}));
        s.box = 12; s.width = 1;
        break;
    }
    return s;
}

class PathIconEngine : public QIconEngine {
public:
    PathIconEngine(Icon which, QColor color) : m_which(which), m_color(color) {}

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State) override
    {
        const Shape s = shapeFor(m_which);
        QColor c = m_color;
        if (mode == QIcon::Disabled)
            c.setAlphaF(c.alphaF() * 0.4);
        const qreal side = std::min(rect.width(), rect.height());
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(rect.x() + (rect.width() - side) / 2.0, rect.y() + (rect.height() - side) / 2.0);
        painter->scale(side / s.box, side / s.box);
        if (s.fill) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(c);
        } else {
            painter->setPen(QPen(c, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(Qt::NoBrush);
        }
        painter->drawPath(s.path);
        painter->restore();
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        QImage img(size * scale, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        {
            QPainter painter(&img);
            paint(&painter, QRect(QPoint(), img.size()), mode, state);
        }
        QPixmap pm = QPixmap::fromImage(img);
        pm.setDevicePixelRatio(scale);
        return pm;
    }

    QIconEngine* clone() const override { return new PathIconEngine(m_which, m_color); }

private:
    Icon m_which;
    QColor m_color;
};

} // namespace

QIcon icon(Icon which, const QColor& color)
{
    return QIcon(new PathIconEngine(which, color));
}

} // namespace sora
