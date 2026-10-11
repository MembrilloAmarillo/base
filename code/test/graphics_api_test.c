/* A C-only client of the public API. It intentionally includes no SDL/Vulkan
 * headers. Run from the repository root with validation enabled. */
#include "../rendering/graphics.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct { float position[2], color[4], uv[2]; } Test_Vertex;
static int failures;

static void expect(Graphics_Result actual, Graphics_Result expected, const char *operation) {
    if (actual != expected) {
        fprintf(stderr, "FAIL: %s: %s (expected %s)\n", operation,
                graphics_result_string(actual), graphics_result_string(expected));
        ++failures;
    }
}
#define EXPECT(call, result) expect((call), (result), #call)
#define REQUIRE(call) do { Graphics_Result result = (call); \
    expect(result, Graphics_NONE, #call); if (result != Graphics_NONE) goto cleanup; } while (0)

static uint32_t *read_shader(const char *path, size_t *word_count) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long bytes = ftell(file);
    rewind(file);
    if (bytes <= 0 || bytes % 4) { fclose(file); return NULL; }
    uint32_t *words = malloc((size_t)bytes);
    if (words && fread(words, 1, (size_t)bytes, file) != (size_t)bytes) { free(words); words = NULL; }
    fclose(file);
    *word_count = (size_t)bytes / 4;
    return words;
}

int main(void) {
    Graphics_Device_t *device = NULL, *other_device = NULL;
    Gpu_Buffer_t *vertices = NULL, *indices = NULL, *indices16 = NULL, *uniform = NULL, *storage = NULL, *foreign = NULL, *tiny = NULL;
    Gpu_Texture_t *texture = NULL;
    Gpu_Sampler_t *sampler = NULL;
    Gpu_Resource_t *resources = NULL;
    Gpu_Pipeline_t *pipeline = NULL;
    uint32_t *vertex_shader = NULL, *fragment_shader = NULL;
    size_t vertex_words = 0, fragment_words = 0;
    const uint8_t pixels[4] = {255, 255, 255, 255};
    const uint32_t index_data[] = {0, 1, 2};
    const uint16_t index_data16[] = {0, 1, 2, 0};
    const float buffer_data[4] = {1, 2, 3, 4};
    const Test_Vertex vertex_data[] = {
        {{-0.5f,-0.5f}, {1,0,0,1}, {-1,-1}},
        {{0.5f,-0.5f}, {0,1,0,1}, {-1,-1}},
        {{0,0.5f}, {0,0,1,1}, {-1,-1}}
    };

    Graphics_Device_Desc_t desc = graphics_device_desc_default();
    desc.title = "Graphics API tests"; desc.width = 320; desc.height = 240; desc.validation = true;
    Graphics_Device_Desc_t invalid_desc = desc;
    invalid_desc.width = 0;
    EXPECT(graphics_init(&device, &invalid_desc), Graphics_ERROR_INVALID_ARGUMENT);
    if (device) { ++failures; goto cleanup; }
    REQUIRE(graphics_init(&device, &desc));
    EXPECT(graphics_init(&device, &desc), Graphics_ERROR_INVALID_ARGUMENT);
    EXPECT(graphics_end_frame(device), Graphics_ERROR_INVALID_ARGUMENT);
    EXPECT(graphics_begin_render(device, NULL), Graphics_ERROR_INVALID_ARGUMENT);

    Gpu_Buffer_Desc_t buffer_desc = {32, GPU_BUFFER_VERTEX, CPU_VISIBLE, true};
    REQUIRE(graphics_create_buffer(device, &vertices, &buffer_desc, NULL));
    EXPECT(graphics_update_buffer(device, vertices, 0, vertex_data, sizeof(vertex_data)), Graphics_ERROR_INVALID_ARGUMENT);
    buffer_desc = (Gpu_Buffer_Desc_t){sizeof(index_data), GPU_BUFFER_INDEX, GPU_ONLY, false};
    REQUIRE(graphics_create_buffer(device, &indices, &buffer_desc, index_data));
    REQUIRE(graphics_update_buffer(device, indices, 0, index_data, sizeof(index_data)));
    EXPECT(graphics_update_buffer(device, indices, 1, index_data, 4), Graphics_ERROR_INVALID_ARGUMENT);
    EXPECT(graphics_update_buffer(device, indices, SIZE_MAX, index_data, 4), Graphics_ERROR_INVALID_ARGUMENT);
    buffer_desc.size = sizeof(index_data16);
    REQUIRE(graphics_create_buffer(device, &indices16, &buffer_desc, index_data16));
    buffer_desc = (Gpu_Buffer_Desc_t){1, GPU_BUFFER_STORAGE, GPU_ONLY, false};
    REQUIRE(graphics_create_buffer(device, &tiny, &buffer_desc, pixels));
    REQUIRE(graphics_update_buffer(device, tiny, 0, pixels, 1));
    REQUIRE(graphics_destroy_buffer(device, &tiny));
    buffer_desc = (Gpu_Buffer_Desc_t){sizeof(buffer_data), GPU_BUFFER_UNIFORM, CPU_VISIBLE, true};
    REQUIRE(graphics_create_buffer(device, &uniform, &buffer_desc, buffer_data));
    buffer_desc.usage = GPU_BUFFER_STORAGE; buffer_desc.dynamic = false;
    REQUIRE(graphics_create_buffer(device, &storage, &buffer_desc, buffer_data));

    Gpu_Texture_Desc_t texture_desc = {2, 2, FORMAT_R8_UNORM};
    EXPECT(graphics_create_texture(device, &texture, &texture_desc, pixels, 3), Graphics_ERROR_INVALID_ARGUMENT);
    REQUIRE(graphics_create_texture(device, &texture, &texture_desc, pixels, sizeof(pixels)));
    REQUIRE(graphics_create_sampler(device, &sampler, NULL));
    Gpu_Resource_Binding_t bindings[] = {
        {0, GPU_RESOURCE_TEXTURE, GPU_STAGE_FRAGMENT},
        {1, GPU_RESOURCE_UNIFORM_BUFFER, GPU_STAGE_VERTEX},
        {2, GPU_RESOURCE_STORAGE_BUFFER, GPU_STAGE_VERTEX}
    };
    Gpu_Resource_Desc_t resource_desc = {bindings, 3};
    Gpu_Resource_Binding_t duplicate_bindings[] = {bindings[0], bindings[0]};
    Gpu_Resource_Desc_t invalid_resources = {duplicate_bindings, 2};
    EXPECT(graphics_create_resources(device, &resources, &invalid_resources), Graphics_ERROR_INVALID_ARGUMENT);
    REQUIRE(graphics_create_resources(device, &resources, &resource_desc));
    Gpu_Resource_Write_t writes[] = {
        {.binding = 0, .texture = texture, .sampler = sampler},
        {.binding = 1, .buffer = uniform, .size = sizeof(buffer_data)},
        {.binding = 2, .buffer = storage, .size = sizeof(buffer_data)}
    };
    REQUIRE(graphics_update_resources(device, resources, writes, 3));
    Gpu_Resource_Write_t invalid_write = writes[1];
    invalid_write.offset = 1;
    EXPECT(graphics_update_resources(device, resources, &invalid_write, 1), Graphics_ERROR_INVALID_ARGUMENT);
    EXPECT(graphics_destroy_texture(device, &texture), Graphics_ERROR_INVALID_ARGUMENT);
    EXPECT(graphics_destroy_sampler(device, &sampler), Graphics_ERROR_INVALID_ARGUMENT);
    EXPECT(graphics_destroy_buffer(device, &uniform), Graphics_ERROR_INVALID_ARGUMENT);

    /* Handles from another device cannot be bound, updated, or destroyed here. */
    REQUIRE(graphics_init(&other_device, &desc));
    REQUIRE(graphics_create_buffer(other_device, &foreign, &buffer_desc, buffer_data));
    invalid_write = writes[2]; invalid_write.buffer = foreign;
    EXPECT(graphics_update_resources(device, resources, &invalid_write, 1), Graphics_ERROR_INVALID_ARGUMENT);
    EXPECT(graphics_destroy_buffer(device, &foreign), Graphics_ERROR_INVALID_ARGUMENT);
    graphics_shutdown(&other_device); foreign = NULL;

    vertex_shader = read_shader("code/test/shaders/cube_ui.vert.spv", &vertex_words);
    fragment_shader = read_shader("code/test/shaders/cube_ui.frag.spv", &fragment_words);
    if (!vertex_shader || !fragment_shader) { fprintf(stderr, "Cannot read test shaders\n"); ++failures; goto cleanup; }
    const Gpu_Vertex_Attribute_t attributes[] = {
        {0, FORMAT_RG32_FLOAT, offsetof(Test_Vertex, position)},
        {1, FORMAT_RGBA32_FLOAT, offsetof(Test_Vertex, color)},
        {2, FORMAT_RG32_FLOAT, offsetof(Test_Vertex, uv)}
    };
    Gpu_PSO_Desc_t pipeline_desc = graphics_pso_desc_default();
    pipeline_desc.first_stage_spirv = vertex_shader; pipeline_desc.first_stage_spirv_size = vertex_words;
    pipeline_desc.fragment_spirv = fragment_shader; pipeline_desc.fragment_spirv_size = fragment_words;
    pipeline_desc.vertex_stride = sizeof(Test_Vertex); pipeline_desc.attributes = attributes; pipeline_desc.attribute_count = 3;
    pipeline_desc.resources = &resources; pipeline_desc.n_resources = 1;
    Gpu_Range_t constant_range = {0, 4};
    pipeline_desc.ranges = &constant_range; pipeline_desc.n_ranges = 1;
    REQUIRE(graphics_create_pso(device, &pipeline, &pipeline_desc));
    EXPECT(graphics_destroy_resources(device, &resources), Graphics_ERROR_INVALID_ARGUMENT);

    for (unsigned frame = 0; frame < 4; ++frame) {
        REQUIRE(graphics_start_frame(device));
        EXPECT(graphics_start_frame(device), Graphics_ERROR_INVALID_ARGUMENT);
        EXPECT(graphics_destroy_buffer(device, &indices), Graphics_ERROR_INVALID_ARGUMENT);
        EXPECT(graphics_update_buffer(device, indices, 0, index_data, sizeof(index_data)), Graphics_ERROR_INVALID_ARGUMENT);
        /* Grow both dynamic copies, including one referenced by a resource set. */
        REQUIRE(graphics_update_buffer(device, vertices, 0, vertex_data, sizeof(vertex_data)));
        float larger_uniform[8] = {0};
        REQUIRE(graphics_update_buffer(device, uniform, 0, larger_uniform, sizeof(larger_uniform)));
        REQUIRE(graphics_begin_render(device, NULL));
        Graphics_Viewport_t viewport = {0, 0, 320, 240, 0, 1};
        REQUIRE(graphics_set_viewport(device, &viewport));
        viewport.width = -1;
        EXPECT(graphics_set_viewport(device, &viewport), Graphics_ERROR_INVALID_ARGUMENT);
        EXPECT(graphics_begin_render(device, NULL), Graphics_ERROR_INVALID_ARGUMENT);
        EXPECT(graphics_end_frame(device), Graphics_ERROR_INVALID_ARGUMENT);
        EXPECT(graphics_update_buffer(device, vertices, 0, vertex_data, sizeof(vertex_data)), Graphics_ERROR_INVALID_ARGUMENT);
        EXPECT(graphics_draw(device, 3, 0), Graphics_ERROR_INVALID_ARGUMENT);
        REQUIRE(graphics_bind_pso(device, pipeline));
        REQUIRE(graphics_push_constant(device, buffer_data, 4));
        EXPECT(graphics_push_constant(device, buffer_data, 8), Graphics_ERROR_INVALID_ARGUMENT);
        REQUIRE(graphics_bind_resources(device, 0, resources));
        REQUIRE(graphics_bind_resources(device, 0, resources)); /* Rebind must not invalidate the command buffer. */
        REQUIRE(graphics_bind_vertex_buffer(device, vertices, 0));
        EXPECT(graphics_bind_vertex_buffer(device, indices, 0), Graphics_ERROR_INVALID_ARGUMENT);
        REQUIRE(graphics_bind_index_buffer(device, indices, GPU_INDEX_UINT32, 0));
        EXPECT(graphics_draw_indexed(device, 4, 0, 0), Graphics_ERROR_INVALID_ARGUMENT);
        EXPECT(graphics_draw(device, 100, 0), Graphics_ERROR_INVALID_ARGUMENT);
        REQUIRE(graphics_set_scissor(device, (Graphics_Rect_t){-20, -20, 1000, 1000}));
        REQUIRE(graphics_draw_indexed(device, 3, 0, 0));
        REQUIRE(graphics_bind_index_buffer(device, indices16, GPU_INDEX_UINT16, 0));
        REQUIRE(graphics_draw_indexed(device, 3, 0, 0));
        REQUIRE(graphics_set_scissor(device, (Graphics_Rect_t){0, 0, 0, 0}));
        REQUIRE(graphics_draw(device, 3, 0));
        REQUIRE(graphics_end_render(device));
        EXPECT(graphics_begin_render(device, NULL), Graphics_ERROR_INVALID_ARGUMENT);
        EXPECT(graphics_update_buffer(device, vertices, 0, vertex_data, sizeof(vertex_data)), Graphics_ERROR_INVALID_ARGUMENT);
        REQUIRE(graphics_end_frame(device));
    }

    REQUIRE(graphics_destroy_pso(device, &pipeline));
    REQUIRE(graphics_destroy_resources(device, &resources));
    REQUIRE(graphics_destroy_sampler(device, &sampler));
    REQUIRE(graphics_destroy_texture(device, &texture));
    REQUIRE(graphics_destroy_buffer(device, &uniform));
    REQUIRE(graphics_destroy_buffer(device, &uniform)); /* NULL-safe. */
    REQUIRE(graphics_destroy_buffer(device, &storage));
    /* Leave vertices/indices alive to exercise device-owned cleanup. */
cleanup:
    free(vertex_shader); free(fragment_shader);
    graphics_shutdown(&other_device);
    graphics_shutdown(&device);
    graphics_shutdown(&device);
    if (!failures) puts("Graphics API tests passed.");
    return failures ? 1 : 0;
}
