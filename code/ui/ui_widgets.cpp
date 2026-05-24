#include "ui_widgets.hpp"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {

AsciiFont g_ascii_font{};
bool g_has_ascii_font = false;

Vec2 MeasureTextFallback(std::string_view text, float font_size) {
    if (!g_has_ascii_font) {
        return Vec2{static_cast<float>(text.size()) * (font_size * 0.58f), font_size};
    }

    const float base_size = g_ascii_font.nominal_size > 0.0f ? g_ascii_font.nominal_size : font_size;
    const float scale = base_size > 0.0f ? (font_size / base_size) : 1.0f;

    float max_line_width = 0.0f;
    float line_width = 0.0f;
    int line_count = 1;
    for (char c : text) {
        if (c == '\n') {
            max_line_width = std::max(max_line_width, line_width);
            line_width = 0.0f;
            line_count += 1;
            continue;
        }

        const unsigned char uc = static_cast<unsigned char>(c);
        if (g_ascii_font.glyphs[uc].valid) {
            line_width += g_ascii_font.glyphs[uc].advance * scale;
        } else {
            line_width += font_size * 0.58f;
        }
    }
    max_line_width = std::max(max_line_width, line_width);
    const float line_height = (g_ascii_font.line_height > 0.0f ? g_ascii_font.line_height : font_size) * scale;
    return Vec2{max_line_width, line_height * static_cast<float>(line_count)};
}

Vec2 MeasureText(Context& ctx, std::string_view text, float font_size) {
    if (ctx.measure_text_fn != nullptr) {
        return ctx.measure_text_fn(text, font_size, ctx.measure_text_user_data);
    }
    return MeasureTextFallback(text, font_size);
}

Rect ResolveWidgetRect(Context& ctx, std::string_view text, bool heading) {
    if (ctx.next_widget_rect.has_value()) {
        Rect r = *ctx.next_widget_rect;
        ctx.next_widget_rect.reset();
        return r;
    }

    const float font_size = heading ? ctx.style.heading_font_size : ctx.style.body_font_size;
    const float padding_x = ctx.style.padding_x;
    const float padding_y = ctx.style.padding_y;
    const Vec2 text_size = MeasureText(ctx, text, font_size);
    const float width = std::max(40.0f, text_size.x + padding_x * 2.0f);
    const float height = std::max(font_size, text_size.y) + padding_y * 2.0f;
    Rect r{ctx.cursor.x, ctx.cursor.y, width, height};
    ctx.cursor.y += height + ctx.style.item_spacing_y;
    return r;
}

void PushCmd(DrawList& dl, std::uint32_t index_count, TextureId texture, Rect clip_rect, Pipeline pipeline) {
    if (index_count == 0U) {
        return;
    }

    if (!dl.commands.empty()) {
        DrawCmd& last = dl.commands.back();
        if (last.texture == texture &&
            last.pipeline == pipeline &&
            last.clip_rect.x == clip_rect.x &&
            last.clip_rect.y == clip_rect.y &&
            last.clip_rect.w == clip_rect.w &&
            last.clip_rect.h == clip_rect.h &&
            (last.first_index + last.index_count == (static_cast<std::uint32_t>(dl.indices.size()) - index_count))) {
            last.index_count += index_count;
            return;
        }
    }

    DrawCmd cmd{};
    cmd.first_index = static_cast<std::uint32_t>(dl.indices.size()) - index_count;
    cmd.index_count = index_count;
    cmd.texture = texture;
    cmd.clip_rect = clip_rect;
    cmd.pipeline = pipeline;
    dl.commands.push_back(cmd);
}

void EmitQuad(DrawList& dl, Rect rect, Vec2 uv0, Vec2 uv1, std::uint32_t packed_color, TextureId texture, Rect clip, Pipeline pipeline) {
    if (pipeline == Pipeline::Triangles) {
        // Sentinel UV to indicate "solid color primitive" in the shared UI shader path.
        uv0 = Vec2{-1.0f, -1.0f};
        uv1 = Vec2{-1.0f, -1.0f};
    }

    const std::uint32_t base = static_cast<std::uint32_t>(dl.vertices.size());

    dl.vertices.push_back(Vertex{{rect.x, rect.y}, {uv0.x, uv0.y}, packed_color});
    dl.vertices.push_back(Vertex{{rect.x + rect.w, rect.y}, {uv1.x, uv0.y}, packed_color});
    dl.vertices.push_back(Vertex{{rect.x + rect.w, rect.y + rect.h}, {uv1.x, uv1.y}, packed_color});
    dl.vertices.push_back(Vertex{{rect.x, rect.y + rect.h}, {uv0.x, uv1.y}, packed_color});

    dl.indices.push_back(base + 0U);
    dl.indices.push_back(base + 1U);
    dl.indices.push_back(base + 2U);
    dl.indices.push_back(base + 0U);
    dl.indices.push_back(base + 2U);
    dl.indices.push_back(base + 3U);

    PushCmd(dl, 6U, texture, clip, pipeline);
}

void EmitTextQuads(Context& ctx, Rect rect, std::string_view text, Color color, bool heading) {
    DrawList& dl = ctx.Main_Draw_List();
    const float font_size = heading ? ctx.style.heading_font_size : ctx.style.body_font_size;
    const std::uint32_t packed = color.PackABGR();
    const float base_size = g_ascii_font.nominal_size > 0.0f ? g_ascii_font.nominal_size : font_size;
    const float scale = base_size > 0.0f ? (font_size / base_size) : 1.0f;

    float x = rect.x + ctx.style.padding_x;
    float y = rect.y + ctx.style.padding_y + font_size;
    for (char c : text) {
        if (c == '\n') {
            x = rect.x + ctx.style.padding_x;
            const float lh = g_has_ascii_font ? (g_ascii_font.line_height * scale) : (font_size + 2.0f);
            y += lh;
            continue;
        }

        const unsigned char uc = static_cast<unsigned char>(c);
        if (g_has_ascii_font && g_ascii_font.glyphs[uc].valid) {
            const AsciiFontGlyph& g = g_ascii_font.glyphs[uc];
            Rect glyph_rect{
                x + g.x_off * scale,
                y - g.y_off * scale,
                g.width * scale,
                g.height * scale
            };
            EmitQuad(dl, glyph_rect, Vec2{g.u0, g.v0}, Vec2{g.u1, g.v1}, packed, 1U, ctx.current_clip, Pipeline::Text);
            x += g.advance * scale;
        } else {
            const float glyph_w = font_size * 0.58f;
            const float glyph_h = font_size;
            const float cell_uv = 1.0f / 16.0f;
            const int tx = uc & 0x0f;
            const int ty = (uc >> 4) & 0x0f;
            const Vec2 uv0{tx * cell_uv, ty * cell_uv};
            const Vec2 uv1{uv0.x + cell_uv, uv0.y + cell_uv};
            Rect glyph_rect{x, y - glyph_h, glyph_w, glyph_h};
            EmitQuad(dl, glyph_rect, uv0, uv1, packed, 1U, ctx.current_clip, Pipeline::Text);
            x += glyph_w;
        }
    }
}

Color Mix(Color a, Color b, float t) {
    t = Clamp01(t);
    auto blend = [t](std::uint8_t c0, std::uint8_t c1) -> std::uint8_t {
        const float v = Lerp(static_cast<float>(c0), static_cast<float>(c1), t);
        return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 255.0f));
    };
    return Color{
        blend(a.r, b.r),
        blend(a.g, b.g),
        blend(a.b, b.b),
        blend(a.a, b.a)
    };
}

}  // namespace

void SetAsciiFont(const AsciiFont& font) {
    g_ascii_font = font;
    g_has_ascii_font = true;
}

void SetCursor(Vec2 pos) {
    Context* ctx = GetCurrentContext();
    if (ctx == nullptr) {
        return;
    }
    ctx->cursor = pos;
}

void SetNextWidgetRect(Rect rect) {
    Context* ctx = GetCurrentContext();
    if (ctx == nullptr) {
        return;
    }
    ctx->next_widget_rect = rect;
}

void PushID(std::string_view label) {
    Context* ctx = GetCurrentContext();
    if (ctx == nullptr || ctx->id_stack_count >= ctx->id_stack.size()) {
        return;
    }
    const Id parent = ctx->Current_Id_Seed();
    ctx->id_stack[ctx->id_stack_count++] = HashId(parent, label);
}

void PushID(const void* ptr) {
    Context* ctx = GetCurrentContext();
    if (ctx == nullptr || ctx->id_stack_count >= ctx->id_stack.size()) {
        return;
    }
    const Id parent = ctx->Current_Id_Seed();
    ctx->id_stack[ctx->id_stack_count++] = HashId(parent, ptr);
}

void PushID(std::uint64_t integer_id) {
    Context* ctx = GetCurrentContext();
    if (ctx == nullptr || ctx->id_stack_count >= ctx->id_stack.size()) {
        return;
    }
    const Id parent = ctx->Current_Id_Seed();
    ctx->id_stack[ctx->id_stack_count++] = HashId(parent, integer_id);
}

void PopID() {
    Context* ctx = GetCurrentContext();
    if (ctx == nullptr || ctx->id_stack_count <= 1U) {
        return;
    }
    ctx->id_stack_count -= 1U;
}

bool Button(std::string_view label) {
    Context* ctx = GetCurrentContext();
    if (ctx == nullptr) {
        return false;
    }

    Rect rect = ResolveWidgetRect(*ctx, label, false);
    Id id = HashId(ctx->Current_Id_Seed(), label);
    ctx->last_item_id = id;
    Widget_State& state = ctx->Get_State(id);

    const bool hovered = rect.Contains(ctx->input.mouse_pos);
    if (hovered) {
        ctx->hot_id = id;
    }
    if (hovered && ctx->input.mouse_pressed[0]) {
        ctx->active_id = id;
    }

    bool clicked = false;
    if (ctx->active_id == id && ctx->input.mouse_released[0]) {
        clicked = hovered;
        ctx->active_id = 0;
    }

    const float hover_t = state.hover;
    const float active_t = state.active;
    Color fill = ctx->style.panel;
    fill = Mix(fill, ctx->style.panel_hover, hover_t);
    fill = Mix(fill, ctx->style.accent, active_t * 0.25f);

    DrawList& dl = ctx->Main_Draw_List();
    EmitQuad(dl, rect, Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}, fill.PackABGR(), 0, ctx->current_clip, Pipeline::Triangles);
    EmitTextQuads(*ctx, rect, label, ctx->style.text, false);

    return clicked;
}

void Text(std::string_view text) {
    Context* ctx = GetCurrentContext();
    if (ctx == nullptr) {
        return;
    }

    Rect rect = ResolveWidgetRect(*ctx, text, false);
    ctx->last_item_id = HashId(ctx->Current_Id_Seed(), text);
    ctx->Get_State(ctx->last_item_id);
    EmitTextQuads(*ctx, rect, text, ctx->style.text, false);
}

void Image(TextureId tex, Rect rect) {
    Context* ctx = GetCurrentContext();
    if (ctx == nullptr) {
        return;
    }
    DrawList& dl = ctx->Main_Draw_List();
    EmitQuad(dl, rect, Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}, 0xffffffffU, tex, ctx->current_clip, Pipeline::Image);
}

void Panel(std::string_view title, Rect rect) {
    Context* ctx = GetCurrentContext();
    if (ctx == nullptr) {
        return;
    }

    Id id = HashId(ctx->Current_Id_Seed(), title);
    ctx->last_item_id = id;
    Widget_State& state = ctx->Get_State(id);
    const bool hovered = rect.Contains(ctx->input.mouse_pos);
    if (hovered) {
        ctx->hot_id = id;
    }
    state.open = 1.0f;

    DrawList& dl = ctx->Main_Draw_List();
    Color panel_color = Mix(ctx->style.panel, ctx->style.panel_hover, state.hover * 0.4f);
    EmitQuad(dl, rect, Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}, panel_color.PackABGR(), 0, ctx->current_clip, Pipeline::Triangles);

    Rect title_rect{
        rect.x + ctx->style.padding_x,
        rect.y + ctx->style.padding_y,
        rect.w - ctx->style.padding_x * 2.0f,
        ctx->style.heading_font_size + ctx->style.padding_y
    };
    EmitTextQuads(*ctx, title_rect, title, ctx->style.accent, true);

    ctx->current_panel = rect;
    ctx->current_clip = rect;
    ctx->cursor = Vec2{
        rect.x + ctx->style.padding_x,
        rect.y + ctx->style.padding_y * 2.0f + ctx->style.heading_font_size
    };
}

void MetricCard(const MetricCardDesc& desc) {
    Context* ctx = GetCurrentContext();
    if (ctx == nullptr) {
        return;
    }

    Rect rect = ResolveWidgetRect(*ctx, desc.value, true);
    rect.w = std::max(rect.w, 220.0f);
    rect.h = std::max(rect.h, 78.0f);
    ctx->cursor.y = rect.y + rect.h + ctx->style.item_spacing_y;

    Id id = HashId(ctx->Current_Id_Seed(), desc.label);
    Widget_State& state = ctx->Get_State(id);
    const bool hovered = rect.Contains(ctx->input.mouse_pos);
    if (hovered) {
        ctx->hot_id = id;
    }
    state.hover = Approach(state.hover, hovered ? 1.0f : 0.0f, ctx->dt, 12.0f);

    Color card_color = Mix(ctx->style.panel, ctx->style.panel_hover, state.hover * 0.5f);
    DrawList& dl = ctx->Main_Draw_List();
    EmitQuad(dl, rect, Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}, card_color.PackABGR(), 0, ctx->current_clip, Pipeline::Triangles);

    Rect accent_bar{rect.x, rect.y, 4.0f, rect.h};
    EmitQuad(dl, accent_bar, Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}, desc.accent.PackABGR(), 0, ctx->current_clip, Pipeline::Triangles);

    Rect label_rect{
        rect.x + 12.0f,
        rect.y + 10.0f,
        rect.w - 16.0f,
        24.0f
    };
    Rect value_rect{
        rect.x + 12.0f,
        rect.y + 34.0f,
        rect.w - 16.0f,
        rect.h - 34.0f
    };
    EmitTextQuads(*ctx, label_rect, desc.label, ctx->style.text_muted, false);
    EmitTextQuads(*ctx, value_rect, desc.value, ctx->style.text, true);
}

}  // namespace ui
