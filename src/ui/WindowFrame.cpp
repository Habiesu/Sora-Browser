#include "ui/WindowFrame.h"

namespace sora {

WindowHit windowHitTest(QPoint pos, QSize size, int border, bool maximized, WindowHit titleBarHit)
{
    if (maximized)
        return titleBarHit;
    const int x = pos.x(), y = pos.y(), w = size.width(), h = size.height();
    const bool left = x < border, right = x >= w - border, top = y < border, bottom = y >= h - border;
    const int corner = 2 * border;
    const bool nearLeft = x < corner, nearRight = x >= w - corner;
    const bool nearTop = y < corner, nearBottom = y >= h - corner;
    if ((top && nearLeft) || (left && nearTop)) return WindowHit::TopLeft;
    if ((top && nearRight) || (right && nearTop)) return WindowHit::TopRight;
    if ((bottom && nearLeft) || (left && nearBottom)) return WindowHit::BottomLeft;
    if ((bottom && nearRight) || (right && nearBottom)) return WindowHit::BottomRight;
    if (left) return WindowHit::Left;
    if (right) return WindowHit::Right;
    if (top) return WindowHit::Top;
    if (bottom) return WindowHit::Bottom;
    return titleBarHit;
}

} // namespace sora
