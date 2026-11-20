#pragma once
#include <QPoint>
#include <QSize>

namespace sora {

// Qué hay bajo un punto de la ventana Niebla (se traduce a HT* de Windows o a startSystemMove/Resize).
enum class WindowHit { Client, Caption, Minimize, Maximize, Close, Left, Right, Top, Bottom,
                       TopLeft, TopRight, BottomLeft, BottomRight };

// Sin maximizar, los `border` px de cada lado (y las esquinas, con el doble de alcance) ganan a
// todo; si no, manda lo que diga la barra de título (`titleBarHit`).
WindowHit windowHitTest(QPoint pos, QSize size, int border, bool maximized, WindowHit titleBarHit);

} // namespace sora
