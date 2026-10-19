#include <cassert>
#include <cstdio>

#include "../ui/ui.h"
#include "../ui/ui.cpp"
#include "../ui/ui_widgets.cpp"

int main() {
    ui::Context ctx{};
    ui::Input input{};
    input.framebuffer_size = ui::Vec2{1280.0f, 720.0f};
    input.mouse_pos = ui::Vec2{40.0f, 120.0f};
    input.mouse_pressed[0] = true;
    input.mouse_released[0] = true;

    ui::BeginFrame(ctx, input, 1.0f / 60.0f);
    ui::Panel("FACTORY", ui::Rect{16.0f, 16.0f, 360.0f, 260.0f});
    ui::Text("Pressure 12.6 bar");
    ui::SetNextWidgetRect(ui::Rect{30.0f, 108.0f, 200.0f, 36.0f});
    bool clicked = ui::Button("MACHINE #1");
    ui::EndFrame(ctx);

    const ui::DrawData& draw = ui::GetDrawData(ctx);
    assert(clicked);
    assert(!draw.lists.empty());
    assert(!draw.lists[0].vertices.empty());
    assert(!draw.lists[0].indices.empty());
    assert(!draw.lists[0].commands.empty());
    assert(ctx.hot_id != 0);
    assert(ctx.state_cache.size() >= 3);

    std::printf("ui_core_test passed: vertices=%zu indices=%zu commands=%zu states=%zu\n",
                draw.lists[0].vertices.size(),
                draw.lists[0].indices.size(),
                draw.lists[0].commands.size(),
                ctx.state_cache.size());
    return 0;
}
