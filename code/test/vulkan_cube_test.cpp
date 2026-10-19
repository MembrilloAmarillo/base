#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <SDL3/SDL.h> // Input stays separate from the rendering API.

#include "../util/types.h"
#include "../io/files.h"
#include "../rendering/graphics.h"
#include "../font/load_font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third-party/stb_truetype.h"

#define FILES_IMPL
#include "../io/files.h"

#define LOAD_FONT_IMPL
#include "../font/load_font.h"

typedef struct vertex_data {
    vec2 pos;
    vec2 uv;
    vec4 color;
} vertex_data_t;

typedef struct vertex_push {
    VkDeviceAddress address;
    vec2 viewport_size;
} vertex_push_t;

static_assert(sizeof(vertex_data_t) == 32 && offsetof(vertex_data_t, color) == 16);
static_assert(sizeof(vertex_push_t) == 16 && offsetof(vertex_push_t, viewport_size) == 8);

/* SPIR-V sizes passed to graphics_create_pipeline are word counts. */
static u32 *get_spirv_data(const char *file, u32 *size_out) {
    f_file f = F_OpenFile(file, RDONLY);
    i32 len = F_FileLength(&f);
    u32 *data = NULL;
    *size_out = 0U;
    if (len > 0 && (len % sizeof(u32)) == 0) {
        data = (u32 *)std::malloc((size_t)len);
        if (data != NULL) {
            F_SetFileData(&f, data);
            if (F_FileRead(&f) == len) {
                *size_out = (u32)len / sizeof(u32);
            } else {
                std::free(data);
                data = NULL;
            }
        }
    }
    F_CloseFile(&f);
    return data;
}

static uint32_t build_text_vertices(const FontCache *font,
                                    const char *text,
                                    vertex_data_t *vertices,
                                    size_t capacity) {
    uint32_t count = 0U;
    const float left = 48.0f;
    float pen_x = left;
    float baseline = 48.0f + (float)font->ascent * font->scale;
    const vec4 color = {0.8f, 0.9f, 1.0f, 1.0f};
    for (size_t i = 0U; text[i] != '\0'; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\n') {
            pen_x = left;
            baseline += font->line_height;
        } else if (ch >= 32U && ch <= 126U) {
            const f_Glyph *glyph = &font->glyph[ch - 32U];
            if (glyph->width > 0.0f && glyph->height > 0.0f && count + 6U <= capacity) {
                float x0 = pen_x + glyph->x_off;
                float y0 = baseline + glyph->y_off;
                float x1 = x0 + glyph->width;
                float y1 = y0 + glyph->height;
                float u0 = glyph->x / (float)font->BitmapWidth;
                float v0 = glyph->y / (float)font->BitmapHeight;
                float u1 = (glyph->x + glyph->width) / (float)font->BitmapWidth;
                float v1 = (glyph->y + glyph->height) / (float)font->BitmapHeight;
                const vertex_data_t quad[6] = {
                    {{x0, y0}, {u0, v0}, color},
                    {{x1, y0}, {u1, v0}, color},
                    {{x1, y1}, {u1, v1}, color},
                    {{x0, y0}, {u0, v0}, color},
                    {{x1, y1}, {u1, v1}, color},
                    {{x0, y1}, {u0, v1}, color},
                };
                (void)std::memcpy(&vertices[count], quad, sizeof(quad));
                count += 6U;
            }
            pen_x += glyph->advance;
        }
    }
    return count;
}

static Graphics_Result draw_text(Graphics_Swapchain_t *swapchain,
                                 const Gpu_Pipeline_t *pipeline,
                                 const Gpu_Binding_Set_t *resource,
                                 Gpu_Buffer_t *vertices,
                                 uint32_t vertex_count,
                                 uint32_t bind_repeat,
                                 uint64_t *bind_ticks) {
    Graphics_Command_t *command = NULL;
    Graphics_Result result = graphics_start_frame(swapchain, &command);
    if (result == Graphics_NONE) {
        Graphics_Attachment_t color = {};
        color.texture = graphics_swapchain_target(swapchain);
        color.load = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.store = VK_ATTACHMENT_STORE_OP_STORE;
        color.clear.color = {{0.01f, 0.01f, 0.01f, 1.0f}};
        Graphics_Render_Desc_t render = {};
        render.area.extent = graphics_swapchain_extent(swapchain);
        render.color_count = 1U;
        render.colors = &color;
        result = graphics_begin_render(command, &render);
    }
    if (result == Graphics_NONE) {
        uint64_t start = SDL_GetPerformanceCounter();
        for (uint32_t i = 0U; i < bind_repeat && result == Graphics_NONE; ++i) {
            result = graphics_bind_pipeline(command, pipeline);
        }
        *bind_ticks += SDL_GetPerformanceCounter() - start;
    }
    if (result == Graphics_NONE) {
        result = graphics_set_depth_test_enabled(command, false);
    }
    if (result == Graphics_NONE) {
        result = graphics_set_stencil_test_enabled(command, false);
    }
    if (result == Graphics_NONE) {
        result = graphics_bind_resources(command, 0U, resource);
        VkExtent2D extent = graphics_swapchain_extent(swapchain);
        vertex_push_t push = {
            vertices->address,
            {(float)extent.width, (float)extent.height},
        };
        if (result == Graphics_NONE)
            result = graphics_push_constants(
                command, VK_SHADER_STAGE_VERTEX_BIT, 0U, sizeof(push), &push);
        if (result == Graphics_NONE) {
            result = graphics_draw(command, vertex_count, 1U, 0U, 0U);
        }
    }
    if (result == Graphics_NONE) {
        result = graphics_end_render(command);
    }
    if (result == Graphics_NONE) {
        result = graphics_end_frame(swapchain);
    }
    if (result != Graphics_NONE && graphics_swapchain_target(swapchain) != NULL)
        (void)graphics_cancel_frame(swapchain);
    return result;
}

static Graphics_Result create_text_pipeline(Graphics_Device_t *device,
                                            Graphics_Swapchain_t *swapchain,
                                            Gpu_Binding_Layout_t *layout,
                                            const u32 *vertex,
                                            size_t vertex_words,
                                            const u32 *fragment,
                                            size_t fragment_words,
                                            Gpu_Pipeline_t *pipeline) {
    Gpu_Color_Target_t color = graphics_color_target_default(graphics_swapchain_format(swapchain));
    color.blend.blendEnable = VK_TRUE;
    color.blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color.blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color.blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkPushConstantRange range = {VK_SHADER_STAGE_VERTEX_BIT, 0U, sizeof(vertex_push_t)};
    Gpu_Pipeline_Desc_t desc = {.vertex_spirv = vertex,
                                .vertex_words = vertex_words,
                                .fragment_spirv = fragment,
                                .fragment_words = fragment_words,
                                .colors = &color,
                                .color_count = 1U,
                                .layouts = &layout,
                                .layout_count = 1U,
                                .push_ranges = &range,
                                .push_range_count = 1U};
    return graphics_create_pipeline(device, &desc, pipeline);
}

int main(int argc, char **argv) {
    uint32_t frame_limit = 0U; // Interactive by default; --frames N is a finite smoke test.
    uint32_t bind_repeat = 1U;
    uint64_t bind_ticks = 0U;
    bool resize_test = false;
    bool validation = true;
    Graphics_Result tResult = Graphics_NONE;

    constexpr uint32_t atlas_size = 512U;
    static uint8_t font_bitmap[atlas_size * atlas_size] = {0};
    const char text[] =
        "Hello, Vulkan!\nText rendered with load_font.h\nRoboto Mono - sampled font atlas";
    vertex_data_t vertices[(sizeof(text) - 1U) * 6U] = {};
    FontCache font = {};
    Graphics_Device_t *device = NULL;
    Graphics_Swapchain_t *swapchain = NULL;
    SDL_Window *window = NULL;
    Gpu_Binding_Layout_t *layout = NULL;
    Gpu_Binding_Set_t resources = {};
    Gpu_Sampler_t sampler = {};
    Gpu_Texture_t texture = {0};
    Gpu_Buffer_t text_buffer = {0};
    Gpu_Pipeline_t text_pipeline = {0};
    Gpu_Upload_Batch_t *uploads = NULL;
    u32 *vert_spirv = NULL;
    u32 *frag_spirv = NULL;
    u32 vert_size = 0U;
    u32 frag_size = 0U;
    uint32_t vertex_count = 0U;

    if (tResult == Graphics_NONE) {
        f_file font_file = F_OpenFile("./data/RobotoMono.ttf", RDONLY);
        bool font_exists = F_FileLength(&font_file) > 0;
        F_CloseFile(&font_file);
        if (!font_exists) {
            std::fprintf(stderr,
                         "Run from the repository root: data/RobotoMono.ttf is required.\n");
            tResult = Graphics_ERROR_RESOURCE;
        }
    }
    if (tResult == Graphics_NONE) {
        font = F_BuildFont(28.0f, atlas_size, atlas_size, font_bitmap, "./data/RobotoMono.ttf");
        vertex_count = build_text_vertices(&font, text, vertices, ArrayCount(vertices));
        vert_spirv = get_spirv_data("./data/Sample.vert.spv", &vert_size);
        frag_spirv = get_spirv_data("./data/Sample.frag.spv", &frag_size);
        if (vert_spirv == NULL || frag_spirv == NULL || vertex_count == 0U) {
            tResult = Graphics_ERROR_RESOURCE;
        }
    }
    if (tResult == Graphics_NONE) {
        if (!SDL_Init(SDL_INIT_VIDEO))
            tResult = Graphics_NO_WINDOW;
        else {
            window = SDL_CreateWindow("Vulkan font atlas - load_font.h",
                                      1280,
                                      720,
                                      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
            if (window == NULL)
                tResult = Graphics_NO_WINDOW;
        }
    }
    if (tResult == Graphics_NONE) {
        Graphics_Device_Desc_t desc = {};
        desc.window = window;
        desc.validation = validation;
        desc.synchronization_validation = validation;
        desc.buffer_device_address = true;
        tResult = graphics_init(&desc, &device);
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_create_swapchain(device, window, &swapchain);
    }
    if (tResult == Graphics_NONE) {
        Gpu_Texture_Desc_t desc = {atlas_size,
                                   atlas_size,
                                   VK_FORMAT_R8_UNORM,
                                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};
        tResult = graphics_create_texture(device, &desc, &texture);
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_create_sampler(device, NULL, &sampler);
    }
    if (tResult == Graphics_NONE) {
        Gpu_Buffer_Desc_t desc = {vertex_count * sizeof(vertex_data_t),
                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  GPU_MEMORY_DEVICE};
        tResult = graphics_create_buffer(device, &desc, &text_buffer);
    }
    if (tResult == Graphics_NONE) {
        tResult =
            graphics_create_upload_batch(device, sizeof(font_bitmap) + text_buffer.size, &uploads);
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_begin_uploads(uploads);
    }
    if (tResult == Graphics_NONE) {
        tResult =
            graphics_upload_texture(uploads, &texture, NULL, font_bitmap, sizeof(font_bitmap));
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_upload_buffer(uploads,
                                         &text_buffer,
                                         0U,
                                         vertices,
                                         (size_t)text_buffer.size,
                                         VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                                         VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_submit_uploads(uploads);
    }
    if (tResult == Graphics_NONE) {
        Gpu_Binding_Desc_t binding = {
            0U, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1U};
        tResult = graphics_create_binding_layout(device, &binding, 1U, &layout);
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_create_resources(device, layout, &resources);
    }
    if (tResult == Graphics_NONE) {
        Gpu_Binding_Value_t value = {};
        value.texture = &texture;
        value.sampler = &sampler;
        value.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        Gpu_Binding_Write_t write = {&resources, 0U, 0U, 1U, &value};
        tResult = graphics_update_resources(device, &write, 1U);
    }
    if (tResult == Graphics_NONE) {
        tResult = create_text_pipeline(device,
                                       swapchain,
                                       layout,
                                       vert_spirv,
                                       vert_size,
                                       frag_spirv,
                                       frag_size,
                                       &text_pipeline);
    }

    uint32_t frames = 0U;
    bool running = tResult == Graphics_NONE;
    bool resize_pending = false;
    while (running && tResult == Graphics_NONE && (frame_limit == 0U || frames < frame_limit)) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                resize_pending = true;
            }
        }
        if (running && resize_pending) {
            tResult = graphics_update_swapchain(swapchain);
            if (tResult == Graphics_SWAPCHAIN_OUT_OF_DATE) {
                tResult = Graphics_NONE;
                SDL_Delay(10U);
            } else if (tResult == Graphics_NONE) {
                resize_pending = false;
                if (text_pipeline.colors[0] != graphics_swapchain_format(swapchain)) {
                    tResult = graphics_destroy_pipeline(device, &text_pipeline);
                    if (tResult == Graphics_NONE)
                        tResult = create_text_pipeline(device,
                                                       swapchain,
                                                       layout,
                                                       vert_spirv,
                                                       vert_size,
                                                       frag_spirv,
                                                       frag_size,
                                                       &text_pipeline);
                }
            }
        }
        if (running && !resize_pending && tResult == Graphics_NONE) {
            tResult = draw_text(swapchain,
                                &text_pipeline,
                                &resources,
                                &text_buffer,
                                vertex_count,
                                bind_repeat,
                                &bind_ticks);
            if (tResult == Graphics_SWAPCHAIN_OUT_OF_DATE) {
                resize_pending = true;
                tResult = Graphics_NONE;
            } else if (tResult == Graphics_NONE) {
                ++frames;
                if (resize_test && (frames == 30U || frames == 60U)) {
                    int width = frames == 30U ? 960 : 1280;
                    int height = frames == 30U ? 540 : 720;
                    if (!SDL_SetWindowSize(window, width, height)) {
                        tResult = Graphics_NO_WINDOW;
                    }
                    resize_pending = true;
                }
            }
        }
    }

    if (device != NULL) {
        Graphics_Result idle = graphics_wait_idle(device);
        if (tResult == Graphics_NONE)
            tResult = idle;
    }
    Graphics_Stats_t stats = graphics_stats(device);
    std::printf(
        "Stats: memory=%llu pools=%llu uploads=%llu bytes=%llu binds=%llu state=%llu bind_cpu_ms=%.3f\n",
        (unsigned long long)stats.memory_allocations,
        (unsigned long long)stats.descriptor_pool_creations,
        (unsigned long long)stats.upload_submissions,
        (unsigned long long)stats.uploaded_bytes,
        (unsigned long long)stats.pipeline_binds,
        (unsigned long long)stats.dynamic_state_commands,
        (double)bind_ticks * 1000.0 / (double)SDL_GetPerformanceFrequency());
    if (device != NULL) {
        (void)graphics_destroy_upload_batch(&uploads);
        (void)graphics_destroy_pipeline(device, &text_pipeline);
        (void)graphics_destroy_resources(device, &resources);
        (void)graphics_destroy_binding_layout(device, &layout);
        (void)graphics_destroy_sampler(device, &sampler);
        (void)graphics_destroy_texture(device, &texture);
        (void)graphics_destroy_buffer(device, &text_buffer);
        (void)graphics_destroy_swapchain(&swapchain);
        if (graphics_stats(device).validation_errors != 0U)
            tResult = Graphics_ERROR_RESOURCE;
    }
    Graphics_Result shutdown_result = graphics_shutdown(&device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (tResult == Graphics_NONE) {
        tResult = shutdown_result;
    }
    std::free(font.kerning);
    std::free(vert_spirv);
    std::free(frag_spirv);
    if (tResult != Graphics_NONE) {
        std::fprintf(stderr, "Font demo failed: %d\n", (int)tResult);
    } else {
        std::printf("Rendered %u text frames (%u glyph vertices).\n", frames, vertex_count);
    }
    return tResult == Graphics_NONE ? 0 : 1;
}
