#ifndef UI_CONTEXT_HPP
#define UI_CONTEXT_HPP

#include <array>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ui_draw.hpp"
#include "ui_id.hpp"
#include "ui_input.hpp"
#include "ui_style.hpp"

namespace ui {

using MeasureTextFn = Vec2 (*)(std::string_view text, float font_size, void* user_data);

struct Widget_State {
    float hover = 0.0f;
    float active = 0.0f;
    float open = 0.0f;
    float alert = 0.0f;
    float scroll_x = 0.0f;
    float scroll_y = 0.0f;
    std::uint64_t last_touched_frame = 0;
};

struct Context {
    Input input{};
    Style style{};

    std::array<Id, 32> id_stack{};
    std::uint32_t id_stack_count = 0;

    Id hot_id = 0;
    Id active_id = 0;
    Id focused_id = 0;
    Id last_item_id = 0;

    std::unordered_map<Id, Widget_State> state_cache{};

    DrawData draw_data{};
    std::uint64_t frame_index = 0;
    float dt = 0.0f;

    Vec2 cursor{0.0f, 0.0f};
    Rect current_panel{0.0f, 0.0f, 0.0f, 0.0f};
    std::optional<Rect> next_widget_rect{};
    Rect current_clip{0.0f, 0.0f, 0.0f, 0.0f};

    std::size_t max_states = 8192;

    MeasureTextFn measure_text_fn = nullptr;
    void* measure_text_user_data = nullptr;

    Id Current_Id_Seed() const {
        if (id_stack_count == 0) {
            return 0;
        }
        return id_stack[id_stack_count - 1];
    }

    Widget_State& Get_State(Id id) {
        Widget_State& state = state_cache[id];
        state.last_touched_frame = frame_index;
        return state;
    }

    DrawList& Main_Draw_List() {
        if (draw_data.lists.empty()) {
            draw_data.lists.push_back(DrawList{});
        }
        return draw_data.lists[0];
    }
};

void SetCurrentContext(Context* ctx);
Context* GetCurrentContext();

}  // namespace ui

#endif  // UI_CONTEXT_HPP
