#ifndef UI_INPUT_HPP
#define UI_INPUT_HPP

#include <array>
#include <vector>

#include "ui_types.hpp"

namespace ui {

constexpr int Mouse_Button_Count = 5;
constexpr int Key_Count = 256;

struct Input {
    Vec2 mouse_pos{};
    Vec2 mouse_delta{};
    float wheel_delta = 0.0f;

    std::array<bool, Mouse_Button_Count> mouse_down{};
    std::array<bool, Mouse_Button_Count> mouse_pressed{};
    std::array<bool, Mouse_Button_Count> mouse_released{};

    std::array<bool, Key_Count> key_down{};
    std::array<bool, Key_Count> key_pressed{};
    std::array<bool, Key_Count> key_released{};

    std::vector<std::uint32_t> text_input_utf32{};

    bool ctrl = false;
    bool shift = false;
    bool alt = false;

    Vec2 framebuffer_size{1280.0f, 720.0f};
    float dpi_scale = 1.0f;
};

}  // namespace ui

#endif  // UI_INPUT_HPP
