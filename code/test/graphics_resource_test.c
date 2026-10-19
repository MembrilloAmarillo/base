/* Production headless initialization, GPU readback, bounded allocation and uploads. */
#include "rendering/graphics.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)
#define OK(call)                                                                                   \
    do {                                                                                           \
        Graphics_Result status = (call);                                                           \
        if (status != Graphics_NONE) {                                                             \
            fprintf(stderr,                                                                        \
                    "FAIL %s:%d: %s: %s\n",                                                        \
                    __FILE__,                                                                      \
                    __LINE__,                                                                      \
                    #call,                                                                         \
                    graphics_result_string(status));                                               \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static void read_texture(Graphics_Device_t *device,
                         Gpu_Texture_t *texture,
                         uint32_t mip,
                         const void *expected,
                         size_t size) {
    Gpu_Buffer_t readback = {0};
    Gpu_Buffer_Desc_t desc = {size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, GPU_MEMORY_READBACK};
    Graphics_Command_t *command = NULL;
    OK(graphics_create_buffer(device, &desc, &readback));
    OK(graphics_create_command(device, &command));
    if (readback.mapped != NULL && command != NULL) {
        VkImageLayout previous = texture->layout;
        OK(graphics_begin_command(command));
        OK(graphics_transition_texture(command,
                                       texture,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                       VK_ACCESS_2_TRANSFER_READ_BIT));
        uint32_t width = texture->extent.width >> mip;
        uint32_t height = texture->extent.height >> mip;
        VkBufferImageCopy copy = {
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 0U, 1U},
            .imageExtent = {width != 0U ? width : 1U, height != 0U ? height : 1U, 1U}};
        VkCommandBuffer native = graphics_native_command(command);
        vkCmdCopyImageToBuffer(native,
                               texture->image,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               readback.buffer,
                               1U,
                               &copy);
        VkBufferMemoryBarrier2 barrier = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                          .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                          .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                          .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                                          .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
                                          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                          .buffer = readback.buffer,
                                          .size = size};
        VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                       .bufferMemoryBarrierCount = 1U,
                                       .pBufferMemoryBarriers = &barrier};
        vkCmdPipelineBarrier2(native, &dependency);
        OK(graphics_transition_texture(command,
                                       texture,
                                       previous,
                                       VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                       VK_ACCESS_2_MEMORY_READ_BIT));
        OK(graphics_submit_command(command));
        OK(graphics_wait_command(command));
        CHECK(memcmp(readback.mapped, expected, size) == 0);
    }
    OK(graphics_destroy_command(&command));
    OK(graphics_destroy_buffer(device, &readback));
}

static void test_ranges(Graphics_Device_t *device) {
    Gpu_Buffer_t src = {0}, dst = {0};
    Gpu_Buffer_Desc_t desc = {64U,
                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              GPU_MEMORY_UPLOAD};
    Graphics_Command_t *command = NULL;
    uint32_t value = 0x12345678U;
    OK(graphics_create_buffer(device, &desc, &src));
    OK(graphics_create_buffer(device, &desc, &dst));
    OK(graphics_create_command(device, &command));
    if (src.mapped != NULL && dst.mapped != NULL && command != NULL) {
        memset(src.mapped, 0x33, 64U);
        memset(dst.mapped, 0, 64U);
        CHECK(graphics_write_buffer(device, &dst, 63U, &value, 4U) == Graphics_OUT_OF_RANGE);
        CHECK(graphics_write_buffer(device, &dst, UINT64_MAX, &value, 4U) == Graphics_OUT_OF_RANGE);
        CHECK(graphics_write_buffer(device, &dst, 4U, &value, SIZE_MAX) == Graphics_OUT_OF_RANGE);
        OK(graphics_write_buffer(device, &src, 8U, &value, 4U));
        CHECK(graphics_submit_command(command) == Graphics_ERROR_STATE);
        OK(graphics_begin_command(command));
        CHECK(graphics_begin_command(command) == Graphics_ERROR_STATE);
        CHECK(graphics_copy_buffer(command, &src, 60U, &dst, 0U, 8U) == Graphics_OUT_OF_RANGE);
        CHECK(graphics_copy_buffer(command, &src, 0U, &src, 0U, 4U) ==
              Graphics_ERROR_INVALID_ARGUMENT);
        OK(graphics_copy_buffer(command, &src, 8U, &dst, 16U, 4U));
        OK(graphics_submit_command(command));
        OK(graphics_wait_command(command));
        CHECK(memcmp((uint8_t *)dst.mapped + 16U, &value, 4U) == 0);
        CHECK(((uint8_t *)dst.mapped)[15] == 0U && ((uint8_t *)dst.mapped)[20] == 0U);
        OK(graphics_begin_command(command));
        OK(graphics_copy_buffer(command, &src, 0U, &dst, 0U, 64U));
        OK(graphics_cancel_command(command));
        CHECK(((uint8_t *)dst.mapped)[0] == 0U);
        CHECK(graphics_create_buffer(device, &desc, &src) == Graphics_ERROR_INVALID_ARGUMENT);
        Graphics_Device_t *same = device;
        CHECK(graphics_shutdown(&same) == Graphics_ERROR_RESOURCE && same == device);
    }
    OK(graphics_destroy_command(&command));
    OK(graphics_destroy_buffer(device, &src));
    OK(graphics_destroy_buffer(device, &dst));
    OK(graphics_destroy_buffer(device, &dst));
}

static void test_memory(Graphics_Device_t *device) {
    Gpu_Buffer_t buffers[512] = {0};
    Gpu_Buffer_Desc_t desc = {256U,
                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              GPU_MEMORY_UPLOAD};
    uint64_t before = graphics_stats(device).memory_allocations;
    for (uint32_t i = 0U; i < 512U; ++i) {
        OK(graphics_create_buffer(device, &desc, &buffers[i]));
        if (buffers[i].mapped != NULL)
            memset(buffers[i].mapped, (int)(i % 251U), 256U);
    }
    CHECK(graphics_stats(device).memory_allocations - before <= 1U);
    Gpu_Buffer_t extra = {0};
    CHECK(graphics_create_buffer(device, &desc, &extra) == Graphics_CAPACITY);
    CHECK(extra.buffer == VK_NULL_HANDLE);
    for (uint32_t i = 0U; i < 512U; i += 2U)
        OK(graphics_destroy_buffer(device, &buffers[i]));
    before = graphics_stats(device).memory_allocations;
    for (uint32_t i = 0U; i < 512U; i += 2U)
        OK(graphics_create_buffer(device, &desc, &buffers[i]));
    CHECK(graphics_stats(device).memory_allocations == before);
    for (uint32_t i = 1U; i < 512U; i += 2U) {
        if (buffers[i].mapped != NULL)
            CHECK(((uint8_t *)buffers[i].mapped)[0] == i % 251U &&
                  ((uint8_t *)buffers[i].mapped)[255] == i % 251U);
    }
    for (uint32_t i = 0U; i < 512U; ++i)
        OK(graphics_destroy_buffer(device, &buffers[i]));
    desc.size = 16U * 1024U * 1024U + 4096U;
    OK(graphics_create_buffer(device, &desc, &extra));
    OK(graphics_destroy_buffer(device, &extra));
    /* Repeatedly change empty block class; table slots and reserved memory must stay bounded. */
    Gpu_Texture_Desc_t image_desc = {8U, 8U, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, 1U};
    for (uint32_t i = 0U; i < 40U; ++i) {
        Gpu_Texture_t texture = {0};
        OK(graphics_create_texture(device, &image_desc, &texture));
        OK(graphics_destroy_texture(device, &texture));
        desc.size = 256U;
        OK(graphics_create_buffer(device, &desc, &extra));
        OK(graphics_destroy_buffer(device, &extra));
    }
    CHECK(graphics_stats(device).reserved_bytes <= 32U * 1024U * 1024U);
}

static void test_uploads(Graphics_Device_t *device) {
    Gpu_Texture_t texture = {0};
    Gpu_Texture_Desc_t desc = {2U,
                               2U,
                               VK_FORMAT_R8_UNORM,
                               VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                               2U};
    OK(graphics_create_texture(device, &desc, &texture));
    Gpu_Upload_Batch_t *batch = NULL;
    OK(graphics_create_upload_batch(device, 64U, &batch));
    uint8_t pixels[4] = {1U, 2U, 3U, 4U};
    uint8_t mip = 77U;
    uint8_t patch = 99U;
    if (batch != NULL) {
        OK(graphics_begin_uploads(batch));
        CHECK(graphics_upload_texture(batch, &texture, NULL, pixels, 3U) ==
              Graphics_ERROR_INVALID_ARGUMENT);
        OK(graphics_upload_texture(batch, &texture, NULL, pixels, sizeof(pixels)));
        OK(graphics_cancel_uploads(batch));
        CHECK(texture.layout == VK_IMAGE_LAYOUT_UNDEFINED);
        OK(graphics_begin_uploads(batch));
        OK(graphics_upload_texture(batch, &texture, NULL, pixels, sizeof(pixels)));
        Gpu_Texture_Upload_t region = {.mip_level = 1U,
                                       .final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                       .consumer_stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                       .consumer_access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT};
        OK(graphics_upload_texture(batch, &texture, &region, &mip, 1U));
        region.mip_level = 0U;
        region.offset = (VkOffset2D){1, 1};
        region.extent = (VkExtent2D){1U, 1U};
        OK(graphics_upload_texture(batch, &texture, &region, &patch, 1U));
        uint64_t barriers = graphics_stats(device).upload_barrier_calls;
        OK(graphics_submit_uploads(batch));
        CHECK(graphics_cancel_uploads(batch) == Graphics_BUSY);
        CHECK(graphics_stats(device).upload_barrier_calls - barriers ==
              3U); /* pre, one overlapping write, post */
        OK(graphics_wait_uploads(batch));
        OK(graphics_poll_uploads(batch));
        pixels[3] = patch;
        read_texture(device, &texture, 0U, pixels, sizeof(pixels));
        read_texture(device, &texture, 1U, &mip, 1U);
    }
    OK(graphics_destroy_upload_batch(&batch));
    OK(graphics_destroy_texture(device, &texture));

    /* Creation is independent of the CPU upload format table. */
    desc =
        (Gpu_Texture_Desc_t){2U,
                             2U,
                             VK_FORMAT_R16G16B16A16_SFLOAT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                             1U};
    uint16_t half[16] = {0x3c00U, 0U, 0U, 0x3c00U};
    OK(graphics_create_texture(device, &desc, &texture));
    OK(graphics_upload_texture_wait(device, &texture, half, sizeof(half)));
    read_texture(device, &texture, 0U, half, sizeof(half));
    OK(graphics_destroy_texture(device, &texture));

    Gpu_Buffer_t buffers[3] = {0};
    Gpu_Upload_Batch_t *ring[3] = {0};
    Gpu_Buffer_Desc_t buffer_desc = {16U, VK_BUFFER_USAGE_TRANSFER_DST_BIT, GPU_MEMORY_READBACK};
    for (uint32_t i = 0U; i < 3U; ++i) {
        OK(graphics_create_buffer(device, &buffer_desc, &buffers[i]));
        OK(graphics_create_upload_batch(device, 32U, &ring[i]));
    }
    uint32_t expected[3] = {0};
    for (uint32_t frame = 0U; frame < 30U; ++frame) {
        uint32_t slot = frame % 3U;
        Graphics_Result ready = graphics_poll_uploads(ring[slot]);
        CHECK(ready == Graphics_NONE || ready == Graphics_BUSY);
        if (ready == Graphics_BUSY)
            OK(graphics_wait_uploads(ring[slot]));
        OK(graphics_begin_uploads(ring[slot]));
        expected[slot] = frame + 1U;
        OK(graphics_upload_buffer(ring[slot],
                                  &buffers[slot],
                                  0U,
                                  &expected[slot],
                                  4U,
                                  VK_PIPELINE_STAGE_2_HOST_BIT,
                                  VK_ACCESS_2_HOST_READ_BIT));
        /* Last overlapping update must win. */
        ++expected[slot];
        OK(graphics_upload_buffer(ring[slot],
                                  &buffers[slot],
                                  0U,
                                  &expected[slot],
                                  4U,
                                  VK_PIPELINE_STAGE_2_HOST_BIT,
                                  VK_ACCESS_2_HOST_READ_BIT));
        OK(graphics_submit_uploads(ring[slot]));
    }
    for (uint32_t i = 0U; i < 3U; ++i) {
        OK(graphics_wait_uploads(ring[i]));
        CHECK(buffers[i].mapped != NULL && memcmp(buffers[i].mapped, &expected[i], 4U) == 0);
        OK(graphics_destroy_upload_batch(&ring[i]));
        OK(graphics_destroy_buffer(device, &buffers[i]));
    }
}

static void test_descriptors(Graphics_Device_t *device) {
    Gpu_Binding_Layout_t *layout = NULL;
    Gpu_Binding_Desc_t bindings[] = {
        {0U, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2U},
        {4U, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1U},
        {9U, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1U}};
    uint64_t layouts = graphics_stats(device).descriptor_layout_creations;
    OK(graphics_create_binding_layout(device, bindings, 3U, &layout));
    Gpu_Binding_Set_t sets[70] = {0};
    uint64_t pools = graphics_stats(device).descriptor_pool_creations;
    for (uint32_t i = 0U; i < 70U; ++i)
        OK(graphics_create_resources(device, layout, &sets[i]));
    CHECK(graphics_stats(device).descriptor_pool_creations - pools == 2U);
    CHECK(graphics_stats(device).descriptor_layout_creations - layouts == 1U);
    CHECK(graphics_destroy_binding_layout(device, &layout) == Graphics_ERROR_RESOURCE);
    Gpu_Buffer_t buffer = {0};
    VkDeviceSize alignment =
        graphics_device_properties(device)->limits.minUniformBufferOffsetAlignment;
    if (alignment < 16U)
        alignment = 16U;
    Gpu_Buffer_Desc_t desc = {alignment * 2U,
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              GPU_MEMORY_UPLOAD};
    OK(graphics_create_buffer(device, &desc, &buffer));
    Gpu_Texture_t texture = {0};
    Gpu_Texture_Desc_t image = {1U, 1U, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, 1U};
    Gpu_Sampler_t sampler = {0};
    OK(graphics_create_texture(device, &image, &texture));
    OK(graphics_create_sampler(device, NULL, &sampler));
    Gpu_Binding_Value_t values[4] = {{.texture = &texture,
                                      .sampler = &sampler,
                                      .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                     {.texture = &texture,
                                      .sampler = &sampler,
                                      .image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                     {.buffer = &buffer, .offset = alignment, .size = 16U},
                                     {.buffer = &buffer, .size = 16U}};
    Gpu_Binding_Write_t writes[] = {{&sets[0], 0U, 0U, 2U, values},
                                    {&sets[0], 4U, 0U, 1U, &values[2]},
                                    {&sets[0], 9U, 0U, 1U, &values[3]}};
    uint64_t calls = graphics_stats(device).descriptor_update_calls;
    OK(graphics_update_resources(device, writes, 3U));
    CHECK(graphics_stats(device).descriptor_update_calls == calls + 1U);
    writes[2].first_element = 1U;
    CHECK(graphics_update_resources(device, writes, 3U) == Graphics_OUT_OF_RANGE);
    CHECK(graphics_stats(device).descriptor_update_calls == calls + 1U);
    OK(graphics_destroy_resources(device, &sets[0]));
    OK(graphics_create_resources(device, layout, &sets[0]));
    CHECK(graphics_stats(device).descriptor_pool_creations - pools == 2U);
    for (uint32_t i = 0U; i < 70U; ++i)
        OK(graphics_destroy_resources(device, &sets[i]));
    OK(graphics_destroy_binding_layout(device, &layout));
    bindings[1].binding = 0U;
    CHECK(graphics_create_binding_layout(device, bindings, 3U, &layout) ==
          Graphics_ERROR_INVALID_ARGUMENT);
    CHECK(layout == NULL);
    Graphics_Device_t *other = NULL;
    Graphics_Device_Desc_t other_desc = {.validation = true, .synchronization_validation = true};
    OK(graphics_init(&other_desc, &other));
    if (other != NULL) {
        CHECK(graphics_destroy_buffer(other, &buffer) == Graphics_ERROR_INVALID_ARGUMENT);
        CHECK(graphics_destroy_texture(other, &texture) == Graphics_ERROR_INVALID_ARGUMENT);
        CHECK(graphics_destroy_sampler(other, &sampler) == Graphics_ERROR_INVALID_ARGUMENT);
        OK(graphics_shutdown(&other));
    }
    OK(graphics_destroy_sampler(device, &sampler));
    OK(graphics_destroy_texture(device, &texture));
    OK(graphics_destroy_buffer(device, &buffer));
}

int main(void) {
    (void)setvbuf(stdout, NULL, _IONBF, 0U);
    Graphics_Device_t *device = NULL;
    Graphics_Device_Desc_t desc = {.validation = true, .synchronization_validation = true};
    OK(graphics_init(&desc, &device));
    if (device != NULL) {
        puts("Testing ranges and independent command contexts...");
        test_ranges(device);
        puts("Testing fragmentation, capacity and empty-block reuse...");
        test_memory(device);
        puts("Testing mip/region uploads, overlapping writes and rotating batches...");
        test_uploads(device);
        puts("Testing shared layouts and atomic batched descriptor writes...");
        test_descriptors(device);
        CHECK(graphics_stats(device).validation_errors == 0U);
    }
    OK(graphics_shutdown(&device));
    OK(graphics_shutdown(&device));
    printf("Graphics resource tests: %u failures\n", failures);
    return failures == 0U ? 0 : 1;
}
