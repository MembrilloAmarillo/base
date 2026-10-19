/* Production lifecycle, recovery and indexed offscreen depth/stencil readback.
 * Internal access is limited to compile-time fault injection. */
#include "rendering/graphics.h"
#ifdef GRAPHICS_TESTING
#include "rendering/graphics_internal.h"
#endif
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures;
static unsigned validation_errors;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                           \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)
#define OK(call)                                                                                   \
    do {                                                                                           \
        Graphics_Result status = (call);                                                           \
        if (status != Graphics_NONE) {                                                             \
            fprintf(stderr,                                                                        \
                    "FAIL line %d: %s: %s\n",                                                      \
                    __LINE__,                                                                      \
                    #call,                                                                         \
                    graphics_result_string(status));                                               \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static VKAPI_ATTR VkBool32 VKAPI_CALL validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                 VkDebugUtilsMessageTypeFlagsEXT type,
                                                 const VkDebugUtilsMessengerCallbackDataEXT *data,
                                                 void *user) {
    (void)type;
    (void)user;
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
        ++validation_errors;
        fprintf(stderr, "%s\n", data->pMessage);
    }
    return VK_FALSE;
}

static uint32_t *read_shader(const char *path, size_t *words) {
    FILE *file = fopen(path, "rb");
    uint32_t *data = NULL;
    *words = 0U;
    if (file != NULL) {
        (void)fseek(file, 0, SEEK_END);
        long bytes = ftell(file);
        rewind(file);
        if (bytes > 0 && bytes % 4 == 0) {
            data = (uint32_t *)malloc((size_t)bytes);
            if (data != NULL && fread(data, 1U, (size_t)bytes, file) == (size_t)bytes)
                *words = (size_t)bytes / 4U;
            else {
                free(data);
                data = NULL;
            }
        }
        fclose(file);
    }
    return data;
}

static void test_rendering(Graphics_Device_t *device) {
    Gpu_Texture_t color = {0}, depth = {0};
    Gpu_Texture_Desc_t image_desc = {16U,
                                     16U,
                                     VK_FORMAT_R8G8B8A8_UNORM,
                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                     1U};
    OK(graphics_create_texture(device, &image_desc, &color));
    image_desc.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    image_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    Graphics_Result depth_result = graphics_create_texture(device, &image_desc, &depth);
    if (depth_result == Graphics_UNSUPPORTED) {
        image_desc.format = VK_FORMAT_D24_UNORM_S8_UINT;
        depth_result = graphics_create_texture(device, &image_desc, &depth);
    }
    CHECK(depth_result == Graphics_NONE);
    Gpu_Buffer_t index = {0}, uniform = {0}, readback = {0};
    uint16_t indices[] = {0U, 1U, 2U};
    Gpu_Buffer_Desc_t buffer_desc = {
        sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, GPU_MEMORY_UPLOAD};
    OK(graphics_create_buffer(device, &buffer_desc, &index));
    OK(graphics_write_buffer(device, &index, 0U, indices, sizeof(indices)));
    VkDeviceSize alignment =
        graphics_device_properties(device)->limits.minUniformBufferOffsetAlignment;
    if (alignment < 16U)
        alignment = 16U;
    buffer_desc =
        (Gpu_Buffer_Desc_t){alignment + 16U, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, GPU_MEMORY_UPLOAD};
    OK(graphics_create_buffer(device, &buffer_desc, &uniform));
    const float white[] = {1.0f, 1.0f, 1.0f, 1.0f};
    OK(graphics_write_buffer(device, &uniform, alignment, white, sizeof(white)));
    buffer_desc =
        (Gpu_Buffer_Desc_t){16U * 16U * 4U, VK_BUFFER_USAGE_TRANSFER_DST_BIT, GPU_MEMORY_READBACK};
    OK(graphics_create_buffer(device, &buffer_desc, &readback));
    Gpu_Binding_Layout_t *layout = NULL;
    Gpu_Binding_Desc_t binding = {
        3U, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1U};
    OK(graphics_create_binding_layout(device, &binding, 1U, &layout));
    Gpu_Binding_Set_t resources = {0};
    OK(graphics_create_resources(device, layout, &resources));
    Gpu_Binding_Value_t value = {.buffer = &uniform, .offset = alignment, .size = 16U};
    Gpu_Binding_Write_t write = {&resources, 3U, 0U, 1U, &value};
    OK(graphics_update_resources(device, &write, 1U));
    size_t vertex_words = 0U, fragment_words = 0U;
    uint32_t *vertex = read_shader("code/test/shaders/graphics_test.vert.spv", &vertex_words);
    uint32_t *fragment = read_shader("code/test/shaders/graphics_test.frag.spv", &fragment_words);
    CHECK(vertex != NULL && fragment != NULL);
    Gpu_Color_Target_t target = graphics_color_target_default(color.format);
    VkPushConstantRange range = {
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U, 20U};
    Gpu_Pipeline_Desc_t pipeline_desc = {.vertex_spirv = vertex,
                                         .vertex_words = vertex_words,
                                         .fragment_spirv = fragment,
                                         .fragment_words = fragment_words,
                                         .colors = &target,
                                         .color_count = 1U,
                                         .depth_format = depth.format,
                                         .stencil_format = depth.format,
                                         .layouts = &layout,
                                         .layout_count = 1U,
                                         .push_ranges = &range,
                                         .push_range_count = 1U};
    Gpu_Pipeline_t pipeline = {0};
    if (vertex != NULL && fragment != NULL && depth.image != VK_NULL_HANDLE) {
        range.size = 3U;
        CHECK(graphics_create_pipeline(device, &pipeline_desc, &pipeline) ==
              Graphics_ERROR_INVALID_ARGUMENT);
        CHECK(pipeline.pipeline == VK_NULL_HANDLE && pipeline.layout == VK_NULL_HANDLE);
        range.size = graphics_device_properties(device)->limits.maxPushConstantsSize + 4U;
        CHECK(graphics_create_pipeline(device, &pipeline_desc, &pipeline) == Graphics_OUT_OF_RANGE);
        range.size = 20U;
        VkPushConstantRange repeated[] = {range, range};
        pipeline_desc.push_ranges = repeated;
        pipeline_desc.push_range_count = 2U;
        CHECK(graphics_create_pipeline(device, &pipeline_desc, &pipeline) ==
              Graphics_ERROR_INVALID_ARGUMENT);
        pipeline_desc.push_ranges = &range;
        pipeline_desc.push_range_count = 1U;
        OK(graphics_create_pipeline(
            device, &pipeline_desc, &pipeline)); /* NULL state and entry point defaults. */
    }
    Graphics_Command_t *command = NULL;
    OK(graphics_create_command(device, &command));
    if (command != NULL && pipeline.pipeline != VK_NULL_HANDLE) {
        CHECK(graphics_draw(command, 3U, 1U, 0U, 0U) == Graphics_ERROR_STATE);
        OK(graphics_begin_command(command));
        OK(graphics_transition_texture(command,
                                       &color,
                                       VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT));
        OK(graphics_cancel_command(command));
        CHECK(color.layout == VK_IMAGE_LAYOUT_UNDEFINED);
        OK(graphics_begin_command(command));
        Graphics_Attachment_t color_attachment = {.texture = &color,
                                                  .load = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                  .store = VK_ATTACHMENT_STORE_OP_STORE};
        Graphics_Attachment_t depth_attachment = {.texture = &depth,
                                                  .load = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                  .store = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                  .clear = {.depthStencil = {1.0f, 0U}}};
        Graphics_Render_Desc_t render = {.area = {{0, 0}, {16U, 16U}},
                                         .color_count = 1U,
                                         .colors = &color_attachment,
                                         .depth = &depth_attachment,
                                         .stencil = &depth_attachment};
        OK(graphics_begin_render(command, &render));
        CHECK(graphics_begin_render(command, &render) == Graphics_ERROR_STATE);
        CHECK(graphics_submit_command(command) == Graphics_ERROR_STATE);
        OK(graphics_set_depth_test_enabled(command,
                                           true)); /* Before binding: must survive first bind. */
        OK(graphics_set_stencil_test_enabled(command, true));
        VkStencilOpState stencil = {VK_STENCIL_OP_KEEP,
                                    VK_STENCIL_OP_REPLACE,
                                    VK_STENCIL_OP_KEEP,
                                    VK_COMPARE_OP_ALWAYS,
                                    255U,
                                    255U,
                                    7U};
        OK(graphics_set_stencil_state(command, &stencil, &stencil));
        uint64_t binds = graphics_stats(device).pipeline_binds;
        OK(graphics_bind_pipeline(command, &pipeline));
        OK(graphics_bind_pipeline(command, &pipeline));
        CHECK(graphics_stats(device).pipeline_binds == binds + 1U);
        OK(graphics_bind_resources(command, 0U, &resources));
        OK(graphics_bind_index_buffer(command, &index, 0U, VK_INDEX_TYPE_UINT16));
        CHECK(graphics_draw_indexed(command, 4U, 1U, 0U, 0, 0U) == Graphics_OUT_OF_RANGE);
        float push[] = {1.0f, 0.0f, 0.0f, 1.0f, 0.25f};
        CHECK(graphics_push_constants(command, range.stageFlags, 0U, 24U, push) ==
              Graphics_OUT_OF_RANGE);
        CHECK(graphics_push_constants(command, VK_SHADER_STAGE_VERTEX_BIT, 0U, 20U, push) ==
              Graphics_ERROR_INVALID_ARGUMENT);
        OK(graphics_push_constants(command, range.stageFlags, 0U, sizeof(push), push));
        OK(graphics_draw_indexed(command, 3U, 1U, 0U, 0, 0U));
        stencil.passOp = VK_STENCIL_OP_KEEP;
        stencil.compareOp = VK_COMPARE_OP_EQUAL;
        stencil.reference = 0U;
        stencil.writeMask = 0U;
        OK(graphics_set_stencil_state(command, &stencil, &stencil));
        OK(graphics_set_depth_test_enabled(command, false));
        push[0] = 0.0f;
        push[1] = 1.0f;
        OK(graphics_push_constants(command, range.stageFlags, 0U, sizeof(push), push));
        OK(graphics_draw_indexed(command, 3U, 1U, 0U, 0, 0U)); /* Rejected by stencil. */
        stencil.reference = 7U;
        OK(graphics_set_stencil_state(command, &stencil, &stencil));
        OK(graphics_set_depth_test_enabled(command, true));
        push[1] = 0.0f;
        push[2] = 1.0f;
        push[4] = 0.75f;
        OK(graphics_push_constants(command, range.stageFlags, 0U, sizeof(push), push));
        OK(graphics_draw_indexed(command, 3U, 1U, 0U, 0, 0U)); /* Rejected by depth. */
        OK(graphics_end_render(command));
        CHECK(graphics_end_render(command) == Graphics_ERROR_STATE);
        OK(graphics_transition_texture(command,
                                       &color,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                       VK_ACCESS_2_TRANSFER_READ_BIT));
        VkBufferImageCopy copy = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 0U, 1U},
                                  .imageExtent = {16U, 16U, 1U}};
        VkCommandBuffer native = graphics_native_command(command);
        vkCmdCopyImageToBuffer(
            native, color.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.buffer, 1U, &copy);
        VkBufferMemoryBarrier2 barrier = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                          .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                          .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                          .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                                          .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
                                          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                          .buffer = readback.buffer,
                                          .size = VK_WHOLE_SIZE};
        VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                       .bufferMemoryBarrierCount = 1U,
                                       .pBufferMemoryBarriers = &barrier};
        vkCmdPipelineBarrier2(native, &dependency);
        OK(graphics_submit_command(command));
        OK(graphics_wait_command(command));
        CHECK(color.layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        if (readback.mapped != NULL) {
            const uint8_t red[] = {255U, 0U, 0U, 255U};
            for (uint32_t i = 0U; i < 256U; ++i)
                CHECK(memcmp((uint8_t *)readback.mapped + i * 4U, red, 4U) == 0);
        }
        graphics_invalidate_state(command);
        OK(graphics_begin_command(command));
        CHECK(graphics_push_constants(command, range.stageFlags, 0U, 4U, white) ==
              Graphics_ERROR_STATE);
        OK(graphics_cancel_command(command));
    }
    OK(graphics_destroy_command(&command));
    OK(graphics_destroy_pipeline(device, &pipeline));
    OK(graphics_destroy_resources(device, &resources));
    OK(graphics_destroy_binding_layout(device, &layout));
    OK(graphics_destroy_texture(device, &color));
    OK(graphics_destroy_texture(device, &depth));
    OK(graphics_destroy_buffer(device, &index));
    OK(graphics_destroy_buffer(device, &uniform));
    OK(graphics_destroy_buffer(device, &readback));
    free(vertex);
    free(fragment);
}

static void test_frames(Graphics_Device_t *device, SDL_Window *window) {
    Graphics_Swapchain_t *swapchain = NULL;
#ifdef GRAPHICS_TESTING
    graphics_test_fault(GRAPHICS_FAIL_VIEW);
    CHECK(graphics_create_swapchain(device, window, &swapchain) == Graphics_NO_MEMORY &&
          swapchain == NULL);
#endif
    OK(graphics_create_swapchain(device, window, &swapchain));
    if (swapchain == NULL)
        return;
    CHECK(graphics_end_frame(swapchain) == Graphics_ERROR_STATE);
    CHECK(graphics_cancel_frame(swapchain) == Graphics_ERROR_STATE);
    for (uint32_t frame = 0U; frame < 12U; ++frame) {
        Graphics_Command_t *command = NULL;
        Graphics_Result result = graphics_start_frame(swapchain, &command);
        if (result == Graphics_SWAPCHAIN_OUT_OF_DATE) {
            OK(graphics_update_swapchain(swapchain));
            result = graphics_start_frame(swapchain, &command);
        }
        CHECK(result == Graphics_NONE);
        if (result != Graphics_NONE)
            break;
        Graphics_Command_t *second = NULL;
        CHECK(graphics_start_frame(swapchain, &second) == Graphics_ERROR_STATE && second == NULL);
        CHECK(graphics_update_swapchain(swapchain) == Graphics_ERROR_STATE);
        CHECK(graphics_destroy_swapchain(&swapchain) == Graphics_ERROR_STATE);
        Gpu_Texture_t *target = graphics_swapchain_target(swapchain);
        CHECK(graphics_destroy_texture(device, target) == Graphics_ERROR_INVALID_ARGUMENT);
        Graphics_Attachment_t attachment = {.texture = target,
                                            .load = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                            .store = VK_ATTACHMENT_STORE_OP_STORE,
                                            .clear = {.color = {{0.1f, 0.2f, 0.3f, 1.0f}}}};
        Graphics_Render_Desc_t render = {.area = {{0, 0}, graphics_swapchain_extent(swapchain)},
                                         .color_count = 1U,
                                         .colors = &attachment};
        OK(graphics_begin_render(command, &render));
        CHECK(graphics_end_frame(swapchain) == Graphics_ERROR_STATE);
        if (frame % 2U == 0U)
            result = graphics_cancel_frame(swapchain);
        else {
            OK(graphics_end_render(command));
            result = graphics_end_frame(swapchain);
        }
        CHECK(result == Graphics_NONE || result == Graphics_SWAPCHAIN_OUT_OF_DATE);
        if (result == Graphics_SWAPCHAIN_OUT_OF_DATE)
            OK(graphics_update_swapchain(swapchain));
        if (frame == 3U || frame == 7U) {
            CHECK(SDL_SetWindowSize(window, frame == 3U ? 400 : 320, frame == 3U ? 300 : 240));
            SDL_PumpEvents();
            OK(graphics_update_swapchain(swapchain));
        }
    }
#ifdef GRAPHICS_TESTING
    graphics_test_fault(GRAPHICS_FAIL_SWAPCHAIN);
    CHECK(graphics_update_swapchain(swapchain) == Graphics_NO_MEMORY);
    Graphics_Command_t *command = NULL;
    CHECK(graphics_start_frame(swapchain, &command) == Graphics_SWAPCHAIN_OUT_OF_DATE);
    OK(graphics_update_swapchain(swapchain));
    graphics_test_fault(GRAPHICS_FAIL_BEGIN);
    CHECK(graphics_start_frame(swapchain, &command) == Graphics_NO_COMMAND_BUFFER);
    CHECK(command == NULL);
    OK(graphics_update_swapchain(swapchain));
    OK(graphics_start_frame(swapchain, &command));
    graphics_test_fault(GRAPHICS_FAIL_SUBMIT);
    CHECK(graphics_end_frame(swapchain) == Graphics_ERROR_QUEUE_SUBMIT);
    command = NULL;
    CHECK(graphics_start_frame(swapchain, &command) == Graphics_SWAPCHAIN_OUT_OF_DATE);
    OK(graphics_update_swapchain(swapchain));
    OK(graphics_start_frame(swapchain, &command));
    Graphics_Result cancelled = graphics_cancel_frame(swapchain);
    CHECK(cancelled == Graphics_NONE || cancelled == Graphics_SWAPCHAIN_OUT_OF_DATE);
#endif
    CHECK(SDL_MinimizeWindow(window));
    SDL_PumpEvents();
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0U)
        CHECK(graphics_update_swapchain(swapchain) == Graphics_SWAPCHAIN_OUT_OF_DATE);
    CHECK(SDL_RestoreWindow(window));
    SDL_PumpEvents();
    OK(graphics_update_swapchain(swapchain));
    OK(graphics_destroy_swapchain(&swapchain));
}

int main(void) {
    (void)setvbuf(stdout, NULL, _IONBF, 0U);
    Graphics_Device_Desc_t desc = {
        .validation = true, .synchronization_validation = true, .debug_callback = validation};
    Graphics_Device_t *device = NULL;
    Graphics_Device_Desc_t invalid = {.synchronization_validation = true};
    CHECK(graphics_init(&invalid, &device) == Graphics_ERROR_INVALID_ARGUMENT && device == NULL);
#ifdef GRAPHICS_TESTING
    graphics_test_fault(GRAPHICS_FAIL_INSTANCE);
    CHECK(graphics_init(&desc, &device) == Graphics_NO_MEMORY && device == NULL);
    graphics_test_fault(GRAPHICS_FAIL_DEVICE);
    CHECK(graphics_init(&desc, &device) == Graphics_NO_MEMORY && device == NULL);
#endif
    OK(graphics_init(&desc, &device));
    if (device != NULL) {
        CHECK(graphics_init(&desc, &device) == Graphics_ERROR_INVALID_ARGUMENT);
        puts("Testing indexed offscreen rendering, depth/stencil and push constants...");
        test_rendering(device);
        OK(graphics_shutdown(&device));
    }
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow(
        "Graphics lifecycle regression", 320, 240, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    CHECK(window != NULL);
    if (window != NULL) {
        desc.window = window;
        OK(graphics_init(&desc, &device));
        if (device != NULL) {
            puts("Testing frame cancellation, swapchain replacement and injected failures...");
            test_frames(device, window);
            OK(graphics_shutdown(&device));
        }
        CHECK(SDL_GetWindowID(window) !=
              0U); /* Borrowed SDL ownership survived graphics shutdown. */
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    CHECK(validation_errors == 0U);
    printf("Graphics API tests: %u failures, %u validation errors\n", failures, validation_errors);
    return failures == 0U ? 0 : 1;
}
