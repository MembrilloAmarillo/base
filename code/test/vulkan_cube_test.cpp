#include <array>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "../memory/memory.h"
#include "../memory/allocator.h"
#include "../util/strings.h"

#define WINDOW_CREATION_IMPL
#include "../rendering/vk_render.h"
#include "../rendering/vk_descriptor.hpp"

#define MEMORY_IMPL
#include "../memory/memory.h"
#include "../memory/allocator.cpp"
#include "../vector/DynamicVector.cpp"

#define STRINGS_IMPL
#include "../util/strings.h"

#include "../rendering/vk_render.cpp"
#include "../rendering/vk_buffer.cpp"
#include "../rendering/vk_image.cpp"
#include "../rendering/vk_descriptor.cpp"
#include "../rendering/vk_instance.cpp"
#include "../rendering/vk_device.cpp"

#define VMA_IMPLEMENTATION
#include "../third-party/vk_mem_alloc.h"

#ifndef global
#define global static
#endif
#define LOAD_FONT_IMPL
#include "../font/load_font_ft2.h"

#include "../ui/ui.h"
#include "../ui/backend/vulkan/vk_ui_renderer.hpp"

#include "../ui/ui.cpp"
#include "../ui/ui_widgets.cpp"
#include "../ui/backend/vulkan/vk_ui_renderer.cpp"

struct Cube_Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;
};

struct Ui_Vertex {
    glm::vec2 pos;
    glm::vec4 color;
    glm::vec2 uv;
};

struct Cube_Push {
    glm::mat4 mvp;
    glm::vec4 tint;
};

static U8_String Debug_Name(const char* text) {
    i64 len = static_cast<i64>(std::strlen(text));
    return U8_String{reinterpret_cast<u8*>(const_cast<char*>(text)), len, len};
}

static const char* cube_shader = R"(
struct PushConstants {
    float4x4 mvp;
    float4 tint;
};
[[vk::push_constant]] PushConstants pc;

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float3 color    : COLOR0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float3 color    : COLOR0;
};

[shader("vertex")]
VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.position = mul(pc.mvp, float4(input.position, 1.0f));
    output.normal = input.normal;
    output.color = input.color * pc.tint.rgb;
    return output;
}

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_TARGET {
    float3 n = normalize(input.normal);
    float3 l = normalize(float3(0.35f, 0.65f, 0.72f));
    float light = saturate(dot(n, l)) * 0.72f + 0.28f;
    return float4(input.color * light, 1.0f);
}
)";

static const char* ui_shader = R"(
struct VSInput {
    float2 position : POSITION;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
};

[[vk::binding(0, 0)]] Texture2D<float4> font_atlas;
[[vk::binding(1, 0)]] SamplerState font_sampler;

[shader("vertex")]
VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 0.0f, 1.0f);
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_TARGET {
    if (input.uv.x < 0.0f || input.uv.y < 0.0f) {
        return input.color;
    }
    float alpha = font_atlas.Sample(font_sampler, input.uv).r * input.color.a;
    if (alpha <= 0.01f) {
        discard;
    }
    return float4(input.color.rgb, alpha);
}
)";

static void Add_Face(std::vector<Cube_Vertex>& vertices,
                     glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                     glm::vec3 normal, glm::vec3 color) {
    vertices.push_back({a, normal, color});
    vertices.push_back({b, normal, color});
    vertices.push_back({c, normal, color});
    vertices.push_back({a, normal, color});
    vertices.push_back({c, normal, color});
    vertices.push_back({d, normal, color});
}

static std::vector<Cube_Vertex> Make_Cube() {
    std::vector<Cube_Vertex> vertices;
    vertices.reserve(36);

    const float s = 1.0f;
    Add_Face(vertices, {-s,-s, s}, { s,-s, s}, { s, s, s}, {-s, s, s}, {0,0,1},  {0.95f, 0.28f, 0.22f});
    Add_Face(vertices, { s,-s,-s}, {-s,-s,-s}, {-s, s,-s}, { s, s,-s}, {0,0,-1}, {0.25f, 0.55f, 0.95f});
    Add_Face(vertices, {-s,-s,-s}, {-s,-s, s}, {-s, s, s}, {-s, s,-s}, {-1,0,0}, {0.30f, 0.80f, 0.42f});
    Add_Face(vertices, { s,-s, s}, { s,-s,-s}, { s, s,-s}, { s, s, s}, {1,0,0},  {0.95f, 0.75f, 0.24f});
    Add_Face(vertices, {-s, s, s}, { s, s, s}, { s, s,-s}, {-s, s,-s}, {0,1,0},  {0.65f, 0.40f, 0.95f});
    Add_Face(vertices, {-s,-s,-s}, { s,-s,-s}, { s,-s, s}, {-s,-s, s}, {0,-1,0}, {0.22f, 0.78f, 0.82f});

    return vertices;
}

static void Add_Textured_Rect(std::vector<Ui_Vertex>& out, float x, float y, float w, float h,
                              float screen_w, float screen_h, glm::vec4 color,
                              glm::vec2 uv0, glm::vec2 uv1) {
    float x0 = 2.0f * x / screen_w - 1.0f;
    float x1 = 2.0f * (x + w) / screen_w - 1.0f;
    float y0 = 2.0f * y / screen_h - 1.0f;
    float y1 = 2.0f * (y + h) / screen_h - 1.0f;

    out.push_back({{x0, y0}, color, {uv0.x, uv0.y}});
    out.push_back({{x1, y0}, color, {uv1.x, uv0.y}});
    out.push_back({{x1, y1}, color, {uv1.x, uv1.y}});
    out.push_back({{x0, y0}, color, {uv0.x, uv0.y}});
    out.push_back({{x1, y1}, color, {uv1.x, uv1.y}});
    out.push_back({{x0, y1}, color, {uv0.x, uv1.y}});
}

static void Add_Text(std::vector<Ui_Vertex>& out, FontCache* font, const char* text,
                     float x, float y, float scale, float screen_w, float screen_h,
                     glm::vec4 color) {
    float cursor_x = x;
    float baseline_y = y + static_cast<float>(font->ascent) * scale;
    for (const unsigned char* at = reinterpret_cast<const unsigned char*>(text); *at; ++at) {
        unsigned char c = *at;
        if (c == '\n') {
            cursor_x = x;
            baseline_y += font->line_height * scale;
            continue;
        }
        if (c < 32 || c >= 32 + 96) {
            c = ' ';
        }

        f_Glyph& glyph = font->glyph[F_GetGlyphFromIdx(font, c)];
        if (glyph.width > 0.0f && glyph.height > 0.0f) {
            float dst_x = cursor_x + glyph.x_off * scale;
            float dst_y = baseline_y - glyph.y_off * scale;
            float dst_w = glyph.width * scale;
            float dst_h = glyph.height * scale;
            glm::vec2 uv0{
                glyph.x / static_cast<float>(font->BitmapWidth),
                glyph.y / static_cast<float>(font->BitmapHeight)
            };
            glm::vec2 uv1{
                (glyph.x + glyph.width) / static_cast<float>(font->BitmapWidth),
                (glyph.y + glyph.height) / static_cast<float>(font->BitmapHeight)
            };
            Add_Textured_Rect(out, dst_x, dst_y, dst_w, dst_h, screen_w, screen_h, color, uv0, uv1);
        }
        cursor_x += glyph.advance * scale;
    }
}

static ui::AsciiFont Build_Ui_Ascii_Font(const FontCache& font) {
    ui::AsciiFont out{};
    out.line_height = font.line_height;
    out.nominal_size = font.FontSize;
    for (int code = 32; code < 128; ++code) {
        const u32 gi = F_GetGlyphFromIdx(const_cast<FontCache*>(&font), static_cast<u64>(code));
        const f_Glyph& g = font.glyph[gi];
        ui::AsciiFontGlyph dst{};
        dst.u0 = g.x / static_cast<float>(font.BitmapWidth);
        dst.v0 = g.y / static_cast<float>(font.BitmapHeight);
        dst.u1 = (g.x + g.width) / static_cast<float>(font.BitmapWidth);
        dst.v1 = (g.y + g.height) / static_cast<float>(font.BitmapHeight);
        dst.width = g.width;
        dst.height = g.height;
        dst.x_off = g.x_off;
        dst.y_off = g.y_off;
        dst.advance = g.advance;
        dst.valid = (g.width > 0.0f && g.height > 0.0f);
        out.glyphs[code] = dst;
    }
    return out;
}

struct Ui_Text_Measure_State {
    const FontCache* font = nullptr;
};

static ui::Vec2 Measure_Text_With_FreeType(std::string_view text, float font_size, void* user_data) {
    const Ui_Text_Measure_State* state = static_cast<const Ui_Text_Measure_State*>(user_data);
    if (state == nullptr || state->font == nullptr) {
        return ui::Vec2{0.0f, font_size};
    }

    FontCache* font = const_cast<FontCache*>(state->font);
    const float base_size = (font->FontSize > 0.0f) ? font->FontSize : font_size;
    const float scale = (base_size > 0.0f) ? (font_size / base_size) : 1.0f;
    const float line_height = F_TextHeight(font) * scale;

    std::size_t line_start = 0;
    float max_width = 0.0f;
    int line_count = 1;

    while (line_start <= text.size()) {
        const std::size_t newline = text.find('\n', line_start);
        const std::size_t line_end = (newline == std::string_view::npos) ? text.size() : newline;
        const std::size_t len = line_end - line_start;
        const char* line_ptr = text.data() + line_start;
        const float width = F_TextWidth(font, line_ptr, static_cast<int>(len)) * scale;
        max_width = std::max(max_width, width);

        if (newline == std::string_view::npos) {
            break;
        }
        line_count += 1;
        line_start = newline + 1;
    }

    return ui::Vec2{max_width, line_height * static_cast<float>(line_count)};
}

static U32 Parse_Frame_Count(int argc, char** argv) {
    U32 frames = std::numeric_limits<U32>::max();
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--interactive") == 0) {
            return std::numeric_limits<U32>::max();
        }
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = static_cast<U32>(std::strtoul(argv[++i], nullptr, 10));
        }
    }
    return frames;
}

static ui::Input Build_Ui_Input(api_window& window, ui_input input_flags,
                                bool& left_mouse_down, vec2& previous_mouse_pos) {
    if (input_flags & Input_LeftClickPress) {
        left_mouse_down = true;
    }
    if (input_flags & Input_LeftClickRelease) {
        left_mouse_down = false;
    }

    ui::Input in{};
    in.framebuffer_size = ui::Vec2{
        static_cast<float>(window.Width),
        static_cast<float>(window.Height)
    };

    const vec2 mouse = GetMousePosition(&window);
    in.mouse_pos = ui::Vec2{mouse.x, mouse.y};
    in.mouse_delta = ui::Vec2{
        mouse.x - previous_mouse_pos.x,
        mouse.y - previous_mouse_pos.y
    };
    previous_mouse_pos = mouse;

    in.mouse_down[0] = left_mouse_down;
    in.mouse_pressed[0] = (input_flags & Input_LeftClickPress) != 0;
    in.mouse_released[0] = (input_flags & Input_LeftClickRelease) != 0;
    in.wheel_delta = (input_flags & Input_MiddleMouseUp) ? 1.0f :
                     ((input_flags & Input_MiddleMouseDown) ? -1.0f : 0.0f);
    in.ctrl = (input_flags & Input_Ctrl) != 0;
    in.shift = (input_flags & Input_Shift) != 0;
    in.alt = (input_flags & Input_Alt) != 0;

    return in;
}

static void Draw_Factory_Dashboard(float smoothed_ms) {
    ui::Panel("FACTORY CONTROL", ui::Rect{300.0f, 150.0f, 500.0f, 500.0f});

    char frame_text[64];
    std::snprintf(frame_text, sizeof(frame_text), "%.2f ms", smoothed_ms);
    ui::MetricCard({
        .label = "Frame Time",
        .value = frame_text,
        .accent = ui::Color::Hex(0x20c878)
    });
    ui::MetricCard({
        .label = "Steam Quantity",
        .value = "500.21 t",
        .accent = ui::Color::Hex(0xff6a1a)
    });
    ui::MetricCard({
        .label = "Staff",
        .value = "24 / 30",
        .accent = ui::Color::Hex(0x4f8cff)
    });

    ui::Text("ESC closes");
    (void)ui::Button("Machine #1");
}

int main(int argc, char** argv) {
    Arena* arena = ArenaAllocDefault();
    Allocator allocator = Mem_Allocator::Make(arena);

    Vk_Render renderer;
    renderer.Init(1280, 720, &allocator);

    {
    std::vector<Cube_Vertex> cube_vertices = Make_Cube();
    auto cube_usage = static_cast<Vk_Buffer<Cube_Vertex>::Buffer_Usage>(Vk_Buffer<Cube_Vertex>::Vertex);
    Vk_Buffer<Cube_Vertex>::Buffer_Description cube_desc{};
    cube_desc.debug_name = Debug_Name("cube vertices");
    cube_desc.usage = cube_usage;
    cube_desc.size = cube_vertices.size() * sizeof(Cube_Vertex);
    cube_desc.vertex_stride = sizeof(Cube_Vertex);
    cube_desc.initial_data = cube_vertices.data();
    Vk_Buffer<Cube_Vertex> cube_buffer(&allocator, &renderer, cube_desc);

    constexpr U64 max_ui_vertices = 32768;
    auto ui_usage = static_cast<Vk_Buffer<Ui_Vertex>::Buffer_Usage>(
        Vk_Buffer<Ui_Vertex>::Vertex | Vk_Buffer<Ui_Vertex>::Upload);
    Vk_Buffer<Ui_Vertex>::Buffer_Description ui_desc{};
    ui_desc.debug_name = Debug_Name("ui text vertices");
    ui_desc.usage = ui_usage;
    ui_desc.size = max_ui_vertices * sizeof(Ui_Vertex);
    ui_desc.vertex_stride = sizeof(Ui_Vertex);
    Vk_Buffer<Ui_Vertex> ui_buffers[Vk_Render::max_frames_in_flight] = {
        Vk_Buffer<Ui_Vertex>(&allocator, &renderer, ui_desc),
        Vk_Buffer<Ui_Vertex>(&allocator, &renderer, ui_desc)
    };

    constexpr U32 font_atlas_width = 1024;
    constexpr U32 font_atlas_height = 1024;
    std::vector<u8> font_atlas(font_atlas_width * font_atlas_height);
    FontCache font = F_BuildFont(
        36.0f,
        font_atlas_width,
        font_atlas_height,
        font_atlas.data(),
        "data/TinosNerdFontPropo.ttf");
    ui::SetAsciiFont(Build_Ui_Ascii_Font(font));
    Vk_Texture font_texture(&allocator);
    VkDescriptorImageInfo font_desc =
        font_texture.Load_From_R8_Data(&renderer, font_atlas.data(), font_atlas_width, font_atlas_height);
    Descriptor font_set = Descriptor::Builder(renderer.Get_Device_Object(), &allocator, 2)
        .Add_Binding(Descriptor_Type::Sampled_Image, 1, static_cast<Shader_Stage_Flags>(Shader_Stages::Fragment))
        .Add_Binding(Descriptor_Type::Sampler, 1, static_cast<Shader_Stage_Flags>(Shader_Stages::Fragment))
        .Enable_Update_After_Bind(false)
        .Enable_Partially_Bound(false)
        .Build();

    VkDescriptorImageInfo sampled_image_info{
        .sampler = VK_NULL_HANDLE,
        .imageView = font_desc.imageView,
        .imageLayout = font_desc.imageLayout
    };
    VkDescriptorImageInfo sampler_info{
        .sampler = font_desc.sampler,
        .imageView = VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    auto font_descriptor_writes = dyn_vector<VkWriteDescriptorSet>::Init(&allocator, 2);
    font_descriptor_writes.AppendByCopy(font_set.Make_Image_Write(0, &sampled_image_info, Descriptor_Type::Sampled_Image));
    font_descriptor_writes.AppendByCopy(font_set.Make_Image_Write(1, &sampler_info, Descriptor_Type::Sampler));
    font_set.Update(font_descriptor_writes);
    font_descriptor_writes.Destroy();

    Vk_Pipeline cube_pipeline(&allocator, renderer.Get_Slang_Global_Session());
    cube_pipeline.Create_Session(renderer.Get_Slang_Session_Description());
    cube_pipeline.Load_Module_From_Source_String(renderer.Get_Device(), "cube_test_3d", "cube_test_3d.slang", cube_shader);
    cube_pipeline
        .Set_Shader_Stages(Shader_Loader::VERTEX | Shader_Loader::FRAGMENT)
        .Push_Dynamic_State(VK_DYNAMIC_STATE_VIEWPORT)
        .Push_Dynamic_State(VK_DYNAMIC_STATE_SCISSOR)
        .Push_Vertex_Binding(0, sizeof(Cube_Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
        .Push_Vertex_Attributes(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Cube_Vertex, pos))
        .Push_Vertex_Attributes(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Cube_Vertex, normal))
        .Push_Vertex_Attributes(2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Cube_Vertex, color))
        .Set_Push_Constant_Range({VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Cube_Push)})
        .Set_Cull_Mode(VK_CULL_MODE_NONE)
        .Set_Depth()
        .Create(renderer.Get_Device(), renderer.Get_Swapchain_Image_Format(), renderer.Get_Depth_Image_Format());

    Vk_Pipeline ui_pipeline(&allocator, renderer.Get_Slang_Global_Session());
    ui_pipeline.Create_Session(renderer.Get_Slang_Session_Description());
    ui_pipeline.Load_Module_From_Source_String(renderer.Get_Device(), "cube_test_ui", "cube_test_ui.slang", ui_shader);
    ui_pipeline
        .Set_Shader_Stages(Shader_Loader::VERTEX | Shader_Loader::FRAGMENT)
        .Push_Dynamic_State(VK_DYNAMIC_STATE_VIEWPORT)
        .Push_Dynamic_State(VK_DYNAMIC_STATE_SCISSOR)
        .Push_Vertex_Binding(0, sizeof(Ui_Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
        .Push_Vertex_Attributes(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Ui_Vertex, pos))
        .Push_Vertex_Attributes(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Ui_Vertex, color))
        .Push_Vertex_Attributes(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Ui_Vertex, uv))
        .Push_Descriptor_Set_Layout(font_set.Get_Handle_Layout())
        .Enable_Alpha_Blend()
        .Set_Cull_Mode(VK_CULL_MODE_NONE)
        .Create(renderer.Get_Device(), renderer.Get_Swapchain_Image_Format(), renderer.Get_Depth_Image_Format());

    ui::Context ui_ctx{};
    Ui_Text_Measure_State text_measure_state{&font};
    ui::SetTextMeasureFunction(ui_ctx, Measure_Text_With_FreeType, &text_measure_state);
    VulkanUiRenderer ui_renderer{};
    ui_renderer.Init(*renderer.Get_Device_Object(), renderer.Get_Swapchain_Image_Format());
    bool left_mouse_down = false;
    vec2 previous_mouse_pos = GetMousePosition(&renderer.window);

    using clock = std::chrono::steady_clock;
    auto previous_time = clock::now();
    float smoothed_ms = 16.67f;
    U32 max_frames = Parse_Frame_Count(argc, argv);

    ui_input input = Input_None;
    for (U32 frame = 0; frame < max_frames && !(input & StopUI); ++frame) {
        input = GetNextEvent(&renderer.window);
        if (input & StopUI) {
            break;
        }
        if (input & FrameBufferResized) {
            renderer.update_swapchain = true;
        }

        auto now = clock::now();
        float frame_ms = std::chrono::duration<float, std::milli>(now - previous_time).count();
        previous_time = now;
        smoothed_ms = smoothed_ms * 0.92f + frame_ms * 0.08f;

        ui::Input ui_input_state = Build_Ui_Input(renderer.window, input, left_mouse_down, previous_mouse_pos);
        ui::BeginFrame(ui_ctx, ui_input_state, frame_ms * 0.001f);
        Draw_Factory_Dashboard(smoothed_ms);
        ui::EndFrame(ui_ctx);

        renderer.Begin_Frame();

        std::vector<Ui_Vertex> ui_vertices;
        ui_vertices.reserve(8192);
        char timing_text[64];
        std::snprintf(timing_text, sizeof(timing_text), "FRAME %.2f MS", smoothed_ms);
        Add_Text(ui_vertices, &font, "VULKAN CUBE TEST", 22.0f, 20.0f, 1.0f,
                 static_cast<float>(renderer.window.Width), static_cast<float>(renderer.window.Height),
                 {0.92f, 0.96f, 1.0f, 1.0f});
        Add_Text(ui_vertices, &font, timing_text, 22.0f, 58.0f, 0.85f,
                 static_cast<float>(renderer.window.Width), static_cast<float>(renderer.window.Height),
                 {0.72f, 0.95f, 0.78f, 1.0f});
        Add_Text(ui_vertices, &font, "ESC CLOSES", 22.0f, static_cast<float>(renderer.window.Height) - 48.0f, 0.7f,
                 static_cast<float>(renderer.window.Width), static_cast<float>(renderer.window.Height),
                 {0.70f, 0.75f, 0.82f, 1.0f});

        if (ui_vertices.size() > max_ui_vertices) {
            ui_vertices.resize(max_ui_vertices);
        }

        U32 frame_slot = renderer.Get_Current_Frame_Idx();
        Vk_Buffer<Ui_Vertex>& ui_buffer = ui_buffers[frame_slot];
        if (!ui_vertices.empty()) {
            size_t byte_count = ui_vertices.size() * sizeof(Ui_Vertex);
            std::memcpy(ui_buffer.cpu_mapped_ptr, ui_vertices.data(), byte_count);
            if (!ui_buffer.is_host_coherent) {
                vmaFlushAllocation(renderer.Get_Vma_Allocator(), ui_buffer.buffer_allocation, 0, byte_count);
            }
        }

        renderer.Begin_Rendering({{0.025f, 0.03f, 0.04f, 1.0f}});
        renderer.Set_Viewport();
        renderer.Set_Scissor();

        float t = std::chrono::duration<float>(now.time_since_epoch()).count();
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), t * 0.85f, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, t * 0.45f, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, -5.0f),
                                     glm::vec3(0.0f, 0.0f, 0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));
        float aspect = static_cast<float>(renderer.window.Width) / static_cast<float>(renderer.window.Height);
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
        proj[1][1] *= -1.0f;
        Cube_Push cube_push{proj * view * model, {1.0f, 1.0f, 1.0f, 1.0f}};

        renderer.Bind_Pipeline(cube_pipeline.pipeline, cube_pipeline.pipeline_layout);
        renderer.Push_Constants(cube_pipeline.pipeline_layout,
                                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                0, sizeof(Cube_Push), &cube_push);
        renderer.Bind_Vertex_Buffer(cube_buffer.Get_Buffer());
        renderer.Draw(static_cast<u32>(cube_vertices.size()));

        renderer.Bind_Pipeline(ui_pipeline.pipeline, ui_pipeline.pipeline_layout);
        VkDescriptorSet font_descriptor_set = font_set.Get_Set();
        renderer.Bind_Descriptor_Sets(ui_pipeline.pipeline_layout, 0, 1, &font_descriptor_set);
        ui_renderer.Render(renderer.Get_Current_Cmd_Buffer(), ui::GetDrawData(ui_ctx));

        if (!ui_vertices.empty()) {
            renderer.Bind_Vertex_Buffer(ui_buffer.Get_Buffer());
            renderer.Draw(static_cast<u32>(ui_vertices.size()));
        }

        renderer.End_Rendering();
        renderer.End_Frame();
    }

    vkDeviceWaitIdle(renderer.Get_Device());
    ui_renderer.Shutdown();
    ui_pipeline.Destroy(renderer.Get_Device());
    cube_pipeline.Destroy(renderer.Get_Device());
    font_texture.Destroy(&renderer);
    }

    renderer.Shutdown();
    ArenaRelease(arena);
    return 0;
}
