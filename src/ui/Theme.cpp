#include "ui/Theme.h"

namespace sora {

Palette nieblaPalette(Qt::ColorScheme scheme)
{
    if (scheme == Qt::ColorScheme::Dark)
        return {QColor("#1b1f27"), QColor("#252a33"), QColor("#e6e9ef"), QColor("#a3abb9"), QColor("#6b7383"),
                QColor("#323946"), QColor("#1b1f27"), QColor("#2f3540"), QColor("#51cf66"), QColor("#c42b1c")};
    return {QColor("#e9edf3"), QColor("#f8fafc"), QColor("#1c2330"), QColor("#5a6476"), QColor("#9aa3b2"),
            QColor("#dfe4ec"), QColor("#eaeef4"), QColor("#dde3eb"), QColor("#2b8a3e"), QColor("#c42b1c")};
}

QString nieblaStyleSheet(const Palette& p)
{
    // Medidas del plan del Hito 2 (a 100 % de escala; Qt las escala con el DPI).
    QString css = QStringLiteral(R"(
#TitleBar { background: @window; }
QTabBar { background: transparent; }
QTabBar QToolButton { background: @window; border: none; }
#TabCloseButton { background: transparent; border: none; border-radius: 4px; }
#TabCloseButton:hover { background: @hover; }
#NewTabButton { background: transparent; border: none; border-radius: 6px; margin-top: 6px; }
#NewTabButton:hover { background: @hover; }
#WindowButton, #CloseButton { background: transparent; border: none; border-radius: 0; }
#WindowButton:hover, #WindowButton[hovered="true"] { background: @hover; }
#CloseButton:hover { background: @close; }
#Toolbar { background: @surface; }
#Toolbar QToolButton { background: transparent; border: none; border-radius: 6px; color: @text; }
#Toolbar QToolButton:hover { background: @hover; }
#Toolbar QToolButton::menu-indicator { image: none; }
#Omnibox {
    background: @field; color: @text; border: 1px solid @field; border-radius: 8px;
    padding: 0 8px; min-height: 30px; selection-background-color: @shield; selection-color: #ffffff;
}
#Omnibox:focus { background: @surface; border: 1px solid @faint; }
#ShieldButton { color: @shield; font-weight: 600; padding: 0 6px; }
#BookmarksBar { background: @surface; border-top: 1px solid @line; border-bottom: 1px solid @line; }
#BookmarksBar QLabel { color: @muted; }
#LinkPreview { background: @surface; color: @muted; border: 1px solid @line; border-radius: 4px; padding: 2px 6px; }
QMenu { background: @surface; color: @text; border: 1px solid @line; padding: 4px; }
QMenu::item { padding: 6px 24px 6px 12px; border-radius: 4px; }
QMenu::item:selected { background: @hover; }
QMenu::separator { height: 1px; background: @line; margin: 4px 8px; }
QToolTip { background: @surface; color: @text; border: 1px solid @line; }
)");
    const std::pair<const char*, QColor> vars[] = {
        {"@window", p.window}, {"@surface", p.surface}, {"@text", p.text}, {"@muted", p.mutedText},
        {"@faint", p.faintText}, {"@line", p.line}, {"@field", p.field}, {"@hover", p.hover},
        {"@shield", p.shield}, {"@close", p.closeHover}};
    for (const auto& [name, color] : vars)
        css.replace(QLatin1String(name), color.name());
    return css;
}

} // namespace sora
