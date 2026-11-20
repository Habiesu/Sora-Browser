#pragma once
#include <QColor>
#include <QString>

namespace sora {

// Colores de la estética Niebla (boceto E). Planos, sin transparencias.
struct Palette {
    QColor window;     // barra de título, pestaña inactiva
    QColor surface;    // pestaña activa, barra de herramientas y de favoritos
    QColor text;
    QColor mutedText;  // pestaña inactiva, favoritos
    QColor faintText;  // × de pestaña, iconos desactivados
    QColor line;       // separadores
    QColor field;      // fondo de la dirección
    QColor hover;      // botones y pestañas al pasar el ratón
    QColor shield;     // escudo y candado
    QColor closeHover; // cerrar ventana al pasar el ratón (icono blanco)
};

// Dark → tema oscuro; Light y Unknown → claro.
Palette nieblaPalette(Qt::ColorScheme scheme);

// Hoja de estilos de toda la ventana (selecciona por objectName).
QString nieblaStyleSheet(const Palette& p);

} // namespace sora
