#ifndef UI_WIDGETS_HPP
#define UI_WIDGETS_HPP

#include <string_view>

#include "ui_context.hpp"

namespace ui {

struct AsciiFontGlyph {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float x_off = 0.0f;
    float y_off = 0.0f;
    float advance = 0.0f;
    bool valid = false;
};

struct AsciiFont {
    AsciiFontGlyph glyphs[256]{};
    float line_height = 18.0f;
    float nominal_size = 18.0f;
};

struct MetricCardDesc {
    std::string_view label{};
    std::string_view value{};
    Color accent = Color::Hex(0xff6a1a);
};

void SetAsciiFont(const AsciiFont& font);

void SetCursor(Vec2 pos);
void SetNextWidgetRect(Rect rect);

void PushID(std::string_view label);
void PushID(const void* ptr);
void PushID(std::uint64_t integer_id);
void PopID();

bool Button(std::string_view label);
void Text(std::string_view text);
void Image(TextureId tex, Rect rect);
void Panel(std::string_view title, Rect rect);
void MetricCard(const MetricCardDesc& desc);

}  // namespace ui

#endif  // UI_WIDGETS_HPP
