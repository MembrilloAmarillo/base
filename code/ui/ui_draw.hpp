#ifndef UI_DRAW_HPP
#define UI_DRAW_HPP

#include <cstdint>
#include <vector>

#include "ui_types.hpp"

namespace ui {

struct Vertex {
    Vec2 pos{};
    Vec2 uv{};
    std::uint32_t color = 0xffffffffU;
};

enum class Pipeline : std::uint8_t {
    Triangles = 0,
    Text = 1,
    Image = 2
};

struct DrawCmd {
    std::uint32_t first_index = 0;
    std::uint32_t index_count = 0;
    TextureId texture = 0;
    Rect clip_rect{};
    Pipeline pipeline = Pipeline::Triangles;
};

struct DrawList {
    std::vector<Vertex> vertices{};
    std::vector<std::uint32_t> indices{};
    std::vector<DrawCmd> commands{};
};

struct DrawData {
    std::vector<DrawList> lists{};
    Vec2 framebuffer_size{};
    float dpi_scale = 1.0f;
};

}  // namespace ui

#endif  // UI_DRAW_HPP
