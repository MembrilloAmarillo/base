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

/* SPIR-V sizes passed to graphics_create_pso are word counts. */
static u32* get_spirv_data(const char *file, u32 *size_out) {
    f_file f = F_OpenFile(file, RDONLY);
    i32 len = F_FileLength(&f);
    u32 *data = NULL;
    *size_out = 0U;
    if (len > 0 && (len % sizeof(u32)) == 0) {
        data = (u32*)std::malloc((size_t)len);
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

static uint32_t build_text_vertices(const FontCache *font, const char *text,
                                    vertex_data_t *vertices, size_t capacity) {
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
                    {{x0, y0}, {u0, v0}, color}, {{x1, y0}, {u1, v0}, color},
                    {{x1, y1}, {u1, v1}, color}, {{x0, y0}, {u0, v0}, color},
                    {{x1, y1}, {u1, v1}, color}, {{x0, y1}, {u0, v1}, color},
                };
                (void)std::memcpy(&vertices[count], quad, sizeof(quad));
                count += 6U;
            }
            pen_x += glyph->advance;
        }
    }
    return count;
}

static Graphics_Result draw_text(Graphics_Device_t *device, Gpu_Pipeline_t pipeline,
                                  const Gpu_Resource_t *resource, Gpu_Buffer_t *vertices,
                                  uint32_t vertex_count, uint32_t bind_repeat, uint64_t *bind_ticks) {
    Graphics_Result result = graphics_start_frame(device);
    if (result == Graphics_NONE) {
        result = graphics_begin_render(device);
    }
    if (result == Graphics_NONE) {
        result = graphics_push_viewport(device);
    }
    if (result == Graphics_NONE) {
        uint64_t start = SDL_GetPerformanceCounter();
        for (uint32_t i = 0U; i < bind_repeat && result == Graphics_NONE; ++i) {
            result = graphics_bind_pso(device, pipeline);
        }
        *bind_ticks += SDL_GetPerformanceCounter() - start;
    }
    if (result == Graphics_NONE) {
        result = graphics_set_depth_test_enabled(device, false);
    }
    if (result == Graphics_NONE) {
        result = graphics_set_stencil_test_enabled(device, false);
    }
    if (result == Graphics_NONE) {
        VkCommandBuffer cmd = device->command_buffers[device->frame_index];
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout,
                                0U, 1U, &resource->set, 0U, NULL);
        vertex_push_t push = {
            graphics_get_buffer_address(vertices),
            {(float)device->swapchain_extent.width, (float)device->swapchain_extent.height},
        };
        result = graphics_push_constant(device, &push, sizeof(push));
        if (result == Graphics_NONE) {
            result = graphics_draw(cmd, vertex_count, 1U);
        }
    }
    if (result == Graphics_NONE) {
        result = graphics_end_render(device);
    }
    if (result == Graphics_NONE) {
        result = graphics_end_frame(device);
    }
    return result;
}

int main(int argc, char** argv) {
    uint32_t frame_limit = 0U; // Interactive by default; --frames N is a finite smoke test.
    uint32_t bind_repeat = 1U;
    uint64_t bind_ticks = 0U;
    bool resize_test = false;
    Graphics_Result tResult = Graphics_NONE;
    for (int i = 1; i < argc && tResult == Graphics_NONE; ++i) {
        if (std::strcmp(argv[i], "--interactive") == 0) {
            frame_limit = 0U;
        } else if (std::strcmp(argv[i], "--resize-test") == 0) {
            resize_test = true;
        } else if ((std::strcmp(argv[i], "--frames") == 0 || std::strcmp(argv[i], "--bind-repeat") == 0) && i + 1 < argc) {
            bool repeat_option = std::strcmp(argv[i], "--bind-repeat") == 0;
            char *end = NULL;
            errno = 0;
            unsigned long value = std::strtoul(argv[++i], &end, 10);
            if (errno != 0 || *end != '\0' || argv[i][0] == '-' || value == 0UL || value > UINT32_MAX) {
                tResult = Graphics_ERROR_INVALID_ARGUMENT;
            } else if (repeat_option) {
                bind_repeat = (uint32_t)value;
            } else {
                frame_limit = (uint32_t)value;
            }
        } else {
            tResult = Graphics_ERROR_INVALID_ARGUMENT;
        }
    }
    if (tResult != Graphics_NONE) {
        std::fprintf(stderr, "Usage: vulkan_cube_test [--frames N | --interactive] [--bind-repeat N] [--resize-test]\n");
    }

    constexpr uint32_t atlas_size = 512U;
    static uint8_t font_bitmap[atlas_size * atlas_size] = {0};
    const char text[] = "Hello, Vulkan!\nText rendered with load_font.h\nRoboto Mono - sampled font atlas";
    vertex_data_t vertices[(sizeof(text) - 1U) * 6U] = {};
    FontCache font = {};
    Graphics_Device_t tDevice = {0};
    Gpu_Resource_t tResource = {0};
    Gpu_Texture_t texture = {0};
    Gpu_Buffer_t text_buffer = {0};
    Gpu_Pipeline_t text_pipeline = {0};
    Gpu_Upload_Batch_t uploads = {0};
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
            std::fprintf(stderr, "Run from the repository root: data/RobotoMono.ttf is required.\n");
            tResult = Graphics_ERROR_RESOURCE;
        }
    }
    if (tResult == Graphics_NONE) {
        font = F_BuildFont(32.0f, atlas_size, atlas_size, font_bitmap, "./data/RobotoMono.ttf");
        vertex_count = build_text_vertices(&font, text, vertices, ArrayCount(vertices));
        vert_spirv = get_spirv_data("./data/Sample.vert.spv", &vert_size);
        frag_spirv = get_spirv_data("./data/Sample.frag.spv", &frag_size);
        if (vert_spirv == NULL || frag_spirv == NULL || vertex_count == 0U) {
            tResult = Graphics_ERROR_RESOURCE;
        }
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_init(&tDevice);
    }
    if (tResult == Graphics_NONE) {
        (void)SDL_SetWindowTitle(tDevice.window, "Vulkan font atlas - load_font.h");
        Gpu_Texture_Desc_t desc = {atlas_size, atlas_size, FORMAT_R8_UNORM,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};
        tResult = graphics_create_texture(&tDevice, &desc, &texture);
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_create_sampler(&tDevice, &texture.sampler);
    }
    if (tResult == Graphics_NONE) {
        Gpu_Buffer_Desc_t desc = {vertex_count * sizeof(vertex_data_t),
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT, GPU_ONLY};
        tResult = graphics_create_buffer(&tDevice, &desc, &text_buffer);
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_create_upload_batch(&tDevice, sizeof(font_bitmap) + text_buffer.size, &uploads);
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_begin_uploads(&tDevice, &uploads);
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_upload_texture(&tDevice, &uploads, &texture, font_bitmap, sizeof(font_bitmap));
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_upload_buffer(&tDevice, &uploads, &text_buffer, vertices, {0U, (size_t)text_buffer.size});
    }
    if (tResult == Graphics_NONE) {
        tResult = graphics_submit_uploads(&tDevice, &uploads);
    }
    if (tResult == Graphics_NONE) {
        Gpu_Binding_Desc_t bindings[] = {{IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1U}};
        Gpu_Resource_Create_Info_t info = {1U, bindings};
        tResult = graphics_create_resources(&tDevice, &info, &tResource);
    }
    if (tResult == Graphics_NONE) {
        Gpu_Resource_Desc_t write = {
            .resource = &tResource, .dst_binding = 0U, .descriptor_count = 1U,
            .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .textures = &texture,
        };
        tResult = graphics_update_resources(&tDevice, write);
    }
    if (tResult == Graphics_NONE) {
        Color_Target_Desc_t color_target = graphics_color_target_desc_default();
        color_target.blend.enabled = true;
        color_target.blend.color = {BLEND_FACTOR_SOURCE_ALPHA, BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA, BLEND_OP_ADD};
        color_target.blend.alpha = {BLEND_FACTOR_ONE, BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA, BLEND_OP_ADD};
        Gpu_Range_t ranges[] = {{0U, sizeof(vertex_push_t)}};
        Gpu_PSO_Desc_t pso_desc = graphics_pso_desc(
            .first_stage_spirv = vert_spirv,
            .first_stage_spirv_size = vert_size,
            .fragment_spirv = frag_spirv,
            .fragment_spirv_size = frag_size,
            .color_targets = &color_target,
            .color_target_count = 1U,
            .depth_format = tDevice.depth_format,
            .first_stage_entry = "main",
            .fragment_entry = "main",
            .set_layout = true,
            .resources = &tResource,
            .n_resources = 1U,
            .ranges = ranges,
            .n_ranges = 1U);
        tResult = graphics_create_pso(&tDevice, &text_pipeline, &pso_desc);
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
            tResult = graphics_update_swapchain(&tDevice);
            if (tResult == Graphics_SWAPCHAIN_OUT_OF_DATE) {
                tResult = Graphics_NONE;
                SDL_Delay(10U);
            } else if (tResult == Graphics_NONE) {
                resize_pending = false;
            }
        }
        if (running && !resize_pending && tResult == Graphics_NONE) {
            tResult = draw_text(&tDevice, text_pipeline, &tResource, &text_buffer, vertex_count, bind_repeat, &bind_ticks);
            if (tResult == Graphics_SWAPCHAIN_OUT_OF_DATE) {
                resize_pending = true;
                tResult = Graphics_NONE;
            } else if (tResult == Graphics_NONE) {
                ++frames;
                if (resize_test && (frames == 30U || frames == 60U)) {
                    int width = frames == 30U ? 960 : 1280;
                    int height = frames == 30U ? 540 : 720;
                    if (!SDL_SetWindowSize(tDevice.window, width, height)) {
                        tResult = Graphics_NO_WINDOW;
                    }
                    resize_pending = true;
                }
            }
        }
    }

    if (tDevice.device != VK_NULL_HANDLE) {
        VkResult idle_result = vkDeviceWaitIdle(tDevice.device);
        if (idle_result != VK_SUCCESS && tResult == Graphics_NONE) {
            tResult = Graphics_ERROR_FENCE;
        }
    }
    std::printf("Stats: memory=%llu pools=%llu uploads=%llu bytes=%llu binds=%llu state=%llu bind_cpu_ms=%.3f\n",
        (unsigned long long)tDevice.stats.memory_allocations,
        (unsigned long long)tDevice.stats.descriptor_pool_creations,
        (unsigned long long)tDevice.stats.upload_submissions,
        (unsigned long long)tDevice.stats.uploaded_bytes,
        (unsigned long long)tDevice.stats.pipeline_binds,
        (unsigned long long)tDevice.stats.dynamic_state_commands,
        (double)bind_ticks * 1000.0 / (double)SDL_GetPerformanceFrequency());
    if (tDevice.device != VK_NULL_HANDLE) {
        if (uploads.owner != VK_NULL_HANDLE) {
            (void)graphics_destroy_upload_batch(&tDevice, &uploads);
        }
        (void)graphics_destroy_pipeline(&tDevice, &text_pipeline);
        (void)graphics_destroy_resources(&tDevice, &tResource);
        (void)graphics_destroy_sampler(&tDevice, &texture.sampler);
        (void)graphics_destroy_texture(&tDevice, &texture);
        (void)graphics_destroy_buffer(&tDevice, &text_buffer);
    }
    Graphics_Result shutdown_result = graphics_shutdown(&tDevice);
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
