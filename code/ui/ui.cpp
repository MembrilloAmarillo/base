#include "ui.h"

#include <algorithm>

namespace ui {

namespace {

thread_local Context* g_current_context = nullptr;

void TrimStateCache(Context& ctx) {
    if (ctx.state_cache.size() <= ctx.max_states) {
        return;
    }

    // Fast conservative pruning: drop entries untouched for a while.
    constexpr std::uint64_t stale_frames = 1200;
    for (auto it = ctx.state_cache.begin(); it != ctx.state_cache.end();) {
        const std::uint64_t age = ctx.frame_index - it->second.last_touched_frame;
        if (age > stale_frames) {
            it = ctx.state_cache.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace

void SetCurrentContext(Context* ctx) {
    g_current_context = ctx;
}

Context* GetCurrentContext() {
    return g_current_context;
}

void BeginFrame(Context& ctx, const Input& input, float dt) {
    ctx.frame_index += 1;
    ctx.input = input;
    ctx.dt = dt;

    ctx.hot_id = 0;
    if (!ctx.input.mouse_down[0]) {
        ctx.active_id = 0;
    }

    ctx.id_stack_count = 1;
    ctx.id_stack[0] = 0;

    ctx.cursor = Vec2{0.0f, 0.0f};
    ctx.current_panel = Rect{0.0f, 0.0f, input.framebuffer_size.x, input.framebuffer_size.y};
    ctx.current_clip = ctx.current_panel;
    ctx.next_widget_rect.reset();
    ctx.last_item_id = 0;

    ctx.draw_data.lists.clear();
    ctx.draw_data.lists.push_back(DrawList{});
    DrawList& dl = ctx.draw_data.lists[0];
    dl.vertices.reserve(4096);
    dl.indices.reserve(8192);
    dl.commands.reserve(256);

    ctx.draw_data.framebuffer_size = input.framebuffer_size;
    ctx.draw_data.dpi_scale = input.dpi_scale;

    SetCurrentContext(&ctx);
}

void EndFrame(Context& ctx) {
    // Maintain deterministic anim transitions from interaction state.
    for (auto& entry : ctx.state_cache) {
        Id id = entry.first;
        Widget_State& state = entry.second;
        const bool hovered = (ctx.hot_id == id);
        const bool active = (ctx.active_id == id);
        state.hover = Approach(state.hover, hovered ? 1.0f : 0.0f, ctx.dt, 12.0f);
        state.active = Approach(state.active, active ? 1.0f : 0.0f, ctx.dt, 18.0f);
    }

    if ((ctx.frame_index % 120U) == 0U) {
        TrimStateCache(ctx);
    }

    if (GetCurrentContext() == &ctx) {
        SetCurrentContext(nullptr);
    }
}

void SetTextMeasureFunction(Context& ctx, MeasureTextFn fn, void* user_data) {
    ctx.measure_text_fn = fn;
    ctx.measure_text_user_data = user_data;
}

const DrawData& GetDrawData(const Context& ctx) {
    return ctx.draw_data;
}

}  // namespace ui
