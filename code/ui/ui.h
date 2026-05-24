#ifndef UI_H
#define UI_H

#include "ui_context.hpp"
#include "ui_widgets.hpp"

namespace ui {

void BeginFrame(Context& ctx, const Input& input, float dt);
void EndFrame(Context& ctx);
void SetTextMeasureFunction(Context& ctx, MeasureTextFn fn, void* user_data);

const DrawData& GetDrawData(const Context& ctx);

}  // namespace ui

#endif  // UI_H
