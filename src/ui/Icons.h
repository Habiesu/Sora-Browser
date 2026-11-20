#pragma once
#include <QColor>
#include <QIcon>

namespace sora {

enum class Icon { Back, Forward, Reload, Stop, Lock, Globe, Shield, Star, Menu, Plus, Close, Minimize, Maximize, Restore };

// Icono Niebla dibujado con QPainterPath (nítido a cualquier tamaño y escala, sin Qt Svg).
// En modo desactivado se pinta con el color atenuado al 40 %.
QIcon icon(Icon which, const QColor& color);

} // namespace sora
