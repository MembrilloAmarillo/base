#ifndef UI_STYLE_HPP
#define UI_STYLE_HPP

#include "ui_types.hpp"

namespace ui {

struct Style {
    float dpi_scale = 1.0f;

    float radius_small = 4.0f;
    float radius_medium = 8.0f;
    float radius_large = 16.0f;

    float panel_alpha = 0.88f;
    float border_width = 1.0f;
    float shadow_size = 4.0f;

    float item_spacing_x = 8.0f;
    float item_spacing_y = 8.0f;
    float padding_x = 12.0f;
    float padding_y = 10.0f;

    float body_font_size = 16.0f;
    float heading_font_size = 20.0f;

    Color background = Color::Hex(0x101312);
    Color panel = Color::RGBA(25, 27, 25, 220);
    Color panel_hover = Color::RGBA(35, 38, 35, 220);
    Color text = Color::Hex(0xf2f2f2);
    Color text_muted = Color::Hex(0xaaaaaa);
    Color accent = Color::Hex(0xff6a1a);
    Color warning = Color::Hex(0xffb020);
    Color success = Color::Hex(0x20c878);
    Color danger = Color::Hex(0xff3b30);
};

}  // namespace ui

#endif  // UI_STYLE_HPP
