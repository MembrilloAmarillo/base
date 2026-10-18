/* Headless Vulkan regression tests. Requires Vulkan 1.3 and the validation layer.
 * Checks actual GPU readback, allocator reuse, descriptor rollover and failures. */
#include "rendering/graphics.h"

static unsigned failures;
static unsigned validation_errors;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); ++failures; } } while (0)
#define OK(call) CHECK((call) == Graphics_NONE)

static VKAPI_ATTR VkBool32 VKAPI_CALL validation(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {
    (void)type; (void)user;
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
        ++validation_errors;
        fprintf(stderr, "%s\n", data->pMessage);
    }
    return VK_FALSE;
}

static bool init_device(Graphics_Device_t *device) {
    const char *layer = "VK_LAYER_KHRONOS_validation";
    const char *extensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME};
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
    VkDebugUtilsMessengerCreateInfoEXT debug = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = validation,
    };
    VkValidationFeatureEnableEXT sync_validation = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validation_features = {
        .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT, .pNext = &debug,
        .enabledValidationFeatureCount = 1U, .pEnabledValidationFeatures = &sync_validation,
    };
    VkInstanceCreateInfo instance = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pNext = &validation_features, .pApplicationInfo = &app,
        .enabledLayerCount = 1U, .ppEnabledLayerNames = &layer,
        .enabledExtensionCount = 2U, .ppEnabledExtensionNames = extensions,
    };
    bool ready = vkCreateInstance(&instance, NULL, &device->instance) == VK_SUCCESS;
    if (ready) {
        PFN_vkCreateDebugUtilsMessengerEXT create = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(device->instance, "vkCreateDebugUtilsMessengerEXT");
        ready = create != NULL && create(device->instance, &debug, NULL, &device->debug_messenger) == VK_SUCCESS;
    }
    uint32_t count = 8U;
    VkPhysicalDevice physical[8] = {0};
    if (ready) {
        VkResult result = vkEnumeratePhysicalDevices(device->instance, &count, physical);
        ready = (result == VK_SUCCESS || result == VK_INCOMPLETE) && count > 0U;
    }
    if (ready) {
        device->physical_device = physical[0];
        VkQueueFamilyProperties queues[32];
        uint32_t queue_count = 32U;
        vkGetPhysicalDeviceQueueFamilyProperties(physical[0], &queue_count, queues);
        device->queue_family = UINT32_MAX;
        for (uint32_t i = 0U; i < queue_count && device->queue_family == UINT32_MAX; ++i) {
            if ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
                device->queue_family = i;
            }
        }
        ready = device->queue_family != UINT32_MAX;
    }
    if (ready) {
        float priority = 1.0f;
        VkDeviceQueueCreateInfo queue = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = device->queue_family, .queueCount = 1U, .pQueuePriorities = &priority,
        };
        VkPhysicalDeviceVulkan13Features features13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE,
        };
        VkPhysicalDeviceVulkan12Features features12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &features13, .bufferDeviceAddress = VK_TRUE,
        };
        VkPhysicalDeviceFeatures features = {.samplerAnisotropy = VK_TRUE};
        VkDeviceCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &features12,
            .queueCreateInfoCount = 1U, .pQueueCreateInfos = &queue, .pEnabledFeatures = &features,
        };
        ready = vkCreateDevice(physical[0], &info, NULL, &device->device) == VK_SUCCESS;
    }
    if (ready) {
        ready = graphics_create_commands(device, device->queue_family, &device->command_pool,
                                         device->command_buffers) == Graphics_NONE;
    }
    return ready;
}

static void submit_and_wait(Graphics_Device_t *device, VkCommandBuffer command) {
    VkFence fence;
    VkFenceCreateInfo info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    CHECK(vkCreateFence(device->device, &info, NULL, &fence) == VK_SUCCESS);
    VkCommandBufferSubmitInfo command_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = command,
    };
    VkSubmitInfo2 submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1U, .pCommandBufferInfos = &command_info,
    };
    VkQueue queue;
    vkGetDeviceQueue(device->device, device->queue_family, 0U, &queue);
    CHECK(vkQueueSubmit2(queue, 1U, &submit, fence) == VK_SUCCESS);
    CHECK(vkWaitForFences(device->device, 1U, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS);
    vkDestroyFence(device->device, fence, NULL);
}

static void test_ranges(Graphics_Device_t *device) {
    uint32_t storage[2] = {0U};
    uint32_t value = 0x12345678U;
    Small_Vector_t vector = small_vector_alloc((uint8_t *)storage, 2U, uint32_t);
    CHECK(small_vector_push(&vector, &value, uint32_t));
    CHECK(small_vector_push(&vector, &value, uint32_t));
    CHECK(!small_vector_push(&vector, &value, uint32_t));
    CHECK(vector.len == sizeof(storage) && storage[0] == value && storage[1] == value);
    Small_Vector_t overflow = small_vector_alloc((uint8_t *)storage, SIZE_MAX, uint32_t);
    CHECK(overflow.capacity == 0U && !small_vector_push(&overflow, &value, uint32_t));

    Gpu_Buffer_t src = {0}, dst = {0};
    Gpu_Buffer_Desc_t desc = {64U, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, CPU_VISIBLE};
    OK(graphics_create_buffer(device, &desc, &src));
    OK(graphics_create_buffer(device, &desc, &dst));
    if (src.mapped != NULL && dst.mapped != NULL) {
        memset(src.mapped, 0x33, 64U);
        memset(dst.mapped, 0, 64U);
        CHECK(graphics_memcpy(device, &dst, &value, (Gpu_Range_t){63U, 4U}) == Graphics_OUT_OF_RANGE);
        CHECK(graphics_memcpy(device, &dst, &value, (Gpu_Range_t){SIZE_MAX, 4U}) == Graphics_OUT_OF_RANGE);
        CHECK(graphics_memcpy(device, &dst, &value, (Gpu_Range_t){4U, SIZE_MAX}) == Graphics_OUT_OF_RANGE);
        CHECK(((uint8_t *)dst.mapped)[63] == 0U);
        OK(graphics_memcpy(device, &src, &value, (Gpu_Range_t){8U, 4U}));
        OK(graphics_copy(device, &dst, &src, (Gpu_Range_t){8U, 4U}, VK_NULL_HANDLE));
        CHECK(memcmp((uint8_t *)dst.mapped + 8U, &value, 4U) == 0);
        CHECK(((uint8_t *)dst.mapped)[7] == 0U && ((uint8_t *)dst.mapped)[12] == 0U);
        CHECK(graphics_copy(device, &dst, &src, (Gpu_Range_t){64U, 4U}, VK_NULL_HANDLE) == Graphics_OUT_OF_RANGE);
        CHECK(graphics_copy(device, &src, &src, (Gpu_Range_t){0U, 4U}, VK_NULL_HANDLE) == Graphics_ERROR_INVALID_ARGUMENT);
        VkCommandBuffer cmd;
        OK(graphics_single_time_command_buffer(device, device->command_pool, &cmd));
        OK(graphics_begin_command_buffer(cmd));
        OK(graphics_copy(device, &dst, &src, (Gpu_Range_t){0U, 64U}, cmd));
        OK(graphics_end_command_buffer(cmd));
        submit_and_wait(device, cmd);
        CHECK(memcmp(dst.mapped, src.mapped, 64U) == 0);
        vkFreeCommandBuffers(device->device, device->command_pool, 1U, &cmd);
        VkBuffer saved = src.buffer;
        CHECK(graphics_create_buffer(device, &desc, &src) == Graphics_ERROR_INVALID_ARGUMENT);
        CHECK(src.buffer == saved);
        CHECK(graphics_shutdown(device) == Graphics_ERROR_RESOURCE);
    }
    OK(graphics_destroy_buffer(device, &src));
    OK(graphics_destroy_buffer(device, &dst));
    OK(graphics_destroy_buffer(device, &dst));
}

static void test_memory(Graphics_Device_t *device) {
    Gpu_Buffer_t buffers[GRAPHICS_MAX_ALLOCATIONS] = {0};
    Gpu_Buffer_Desc_t desc = {256U, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, CPU_VISIBLE};
    uint64_t before = device->stats.memory_allocations;
    for (uint32_t i = 0U; i < GRAPHICS_MAX_ALLOCATIONS; ++i) {
        OK(graphics_create_buffer(device, &desc, &buffers[i]));
        if (buffers[i].buffer != VK_NULL_HANDLE) {
            VkMemoryRequirements req;
            vkGetBufferMemoryRequirements(device->device, buffers[i].buffer, &req);
            CHECK(buffers[i].memory_offset % req.alignment == 0U);
            CHECK(buffers[i].memory == buffers[0].memory);
            if (i != 0U) {
                CHECK(buffers[i].memory_offset >= buffers[i - 1U].memory_offset + desc.size);
            }
        }
    }
    CHECK(device->stats.memory_allocations - before <= 1U);
    Gpu_Buffer_t extra = {0};
    CHECK(graphics_create_buffer(device, &desc, &extra) == Graphics_NO_MEMORY);
    CHECK(extra.buffer == VK_NULL_HANDLE);
    VkDeviceSize hole = buffers[10].memory_offset;
    OK(graphics_destroy_buffer(device, &buffers[10]));
    OK(graphics_create_buffer(device, &desc, &extra));
    CHECK(extra.memory_offset == hole);
    OK(graphics_destroy_buffer(device, &extra));
    for (uint32_t i = 0U; i < GRAPHICS_MAX_ALLOCATIONS; ++i) {
        OK(graphics_destroy_buffer(device, &buffers[i]));
    }
    desc.size = GRAPHICS_MEMORY_BLOCK_SIZE + 4096U;
    OK(graphics_create_buffer(device, &desc, &extra));
    CHECK(extra.memory_offset == 0U);
    OK(graphics_destroy_buffer(device, &extra));
}

static void read_texture(Graphics_Device_t *device, Gpu_Texture_t *texture, const void *expected, size_t size) {
    Gpu_Buffer_Desc_t desc = {size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, CPU_VISIBLE};
    Gpu_Buffer_t readback = {0};
    OK(graphics_create_buffer(device, &desc, &readback));
    VkCommandBuffer cmd;
    OK(graphics_single_time_command_buffer(device, device->command_pool, &cmd));
    OK(graphics_begin_command_buffer(cmd));
    graphics_issue_image_barrier(cmd, texture->image, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
        texture->layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy copy = {
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 0U, 1U}, .imageExtent = texture->extent,
    };
    vkCmdCopyImageToBuffer(cmd, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.buffer, 1U, &copy);
    graphics_issue_memory_barrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    graphics_issue_image_barrier(cmd, texture->image, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->layout);
    OK(graphics_end_command_buffer(cmd));
    submit_and_wait(device, cmd);
    CHECK(readback.mapped != NULL && memcmp(readback.mapped, expected, size) == 0);
    vkFreeCommandBuffers(device->device, device->command_pool, 1U, &cmd);
    OK(graphics_destroy_buffer(device, &readback));
}

static void test_uploads_and_resources(Graphics_Device_t *device) {
    Gpu_Texture_t textures[2] = {0};
    const uint8_t r8[4] = {0U, 64U, 128U, 255U};
    const uint8_t rgba[16] = {1U, 2U, 3U, 255U, 4U, 5U, 6U, 128U, 7U, 8U, 9U, 64U, 10U, 11U, 12U, 0U};
    const uint32_t buffer_data[4] = {1U, 2U, 3U, 4U};
    Gpu_Texture_Desc_t desc = {2U, 2U, FORMAT_R8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT};
    OK(graphics_create_texture(device, &desc, &textures[0]));
    desc.format = FORMAT_RGBA8_SRGB;
    OK(graphics_create_texture(device, &desc, &textures[1]));
    CHECK(textures[0].memory == textures[1].memory);
    Gpu_Buffer_t buffer = {0};
    Gpu_Buffer_Desc_t buffer_desc = {sizeof(buffer_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, CPU_VISIBLE};
    OK(graphics_create_buffer(device, &buffer_desc, &buffer));
    CHECK(buffer.memory != textures[0].memory);
    Gpu_Upload_Batch_t batch = {0};
    OK(graphics_create_upload_batch(device, 40U, &batch));
    uint64_t allocations = device->stats.memory_allocations;
    uint64_t submissions = device->stats.upload_submissions;
    OK(graphics_begin_uploads(device, &batch));
    CHECK(graphics_upload_texture(device, &batch, &textures[0], r8, 3U) == Graphics_ERROR_INVALID_ARGUMENT);
    CHECK(batch.used == 0U);
    OK(graphics_upload_texture(device, &batch, &textures[0], r8, sizeof(r8)));
    OK(graphics_cancel_uploads(device, &batch));
    CHECK(textures[0].layout == VK_IMAGE_LAYOUT_UNDEFINED);
    OK(graphics_begin_uploads(device, &batch));
    OK(graphics_upload_texture(device, &batch, &textures[0], r8, sizeof(r8)));
    OK(graphics_upload_texture(device, &batch, &textures[1], rgba, sizeof(rgba)));
    OK(graphics_upload_buffer(device, &batch, &buffer, buffer_data, (Gpu_Range_t){0U, sizeof(buffer_data)}));
    OK(graphics_upload_texture(device, &batch, &textures[0], r8, sizeof(r8)));
    CHECK(batch.used == 40U);
    CHECK(graphics_upload_texture(device, &batch, &textures[0], r8, sizeof(r8)) == Graphics_OUT_OF_RANGE);
    OK(graphics_submit_uploads(device, &batch));
    CHECK(graphics_begin_uploads(device, &batch) == Graphics_ERROR_RESOURCE);
    CHECK(graphics_cancel_uploads(device, &batch) == Graphics_ERROR_RESOURCE);
    CHECK(device->stats.upload_submissions == submissions + 1U);
    OK(graphics_wait_uploads(device, &batch));
    CHECK(memcmp(buffer.mapped, buffer_data, sizeof(buffer_data)) == 0);
    read_texture(device, &textures[0], r8, sizeof(r8));
    read_texture(device, &textures[1], rgba, sizeof(rgba));
    OK(graphics_begin_uploads(device, &batch));
    OK(graphics_upload_texture(device, &batch, &textures[0], r8, sizeof(r8)));
    OK(graphics_submit_uploads(device, &batch));
    OK(graphics_wait_uploads(device, &batch));
    CHECK(device->stats.memory_allocations == allocations);

    VkSampler sampler = VK_NULL_HANDLE;
    OK(graphics_create_sampler(device, &sampler));
    textures[0].sampler = sampler;
    textures[1].sampler = sampler;
    Gpu_Binding_Desc_t bindings[] = {
        {IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2U},
        {UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1U},
        {STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1U},
    };
    Gpu_Resource_Create_Info_t resource_desc = {3U, bindings};
    Gpu_Resource_t resources[70] = {0};
    uint64_t pools = device->stats.descriptor_pool_creations;
    for (uint32_t i = 0U; i < 70U; ++i) {
        OK(graphics_create_resources(device, &resource_desc, &resources[i]));
    }
    CHECK(device->stats.descriptor_pool_creations - pools == 2U);
    CHECK(resources[0].pool == resources[63].pool && resources[0].pool != resources[64].pool);
    Gpu_Resource_Desc_t write = {
        .resource = &resources[0], .descriptor_count = 2U,
        .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .textures = textures,
    };
    OK(graphics_update_resources(device, write)); // buffers intentionally NULL
    write.dst_array_elem = 1U;
    CHECK(graphics_update_resources(device, write) == Graphics_OUT_OF_RANGE);
    write.dst_array_elem = 0U;
    write.descriptor_count = 65U;
    CHECK(graphics_update_resources(device, write) == Graphics_OUT_OF_RANGE);
    write.descriptor_count = 1U;
    write.textures = NULL;
    CHECK(graphics_update_resources(device, write) == Graphics_ERROR_INVALID_ARGUMENT);
    write.dst_binding = 1U;
    write.descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.buffers = &buffer;
    OK(graphics_update_resources(device, write)); // textures intentionally NULL
    write.dst_binding = 2U;
    write.descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    OK(graphics_update_resources(device, write));
    OK(graphics_destroy_resources(device, &resources[0]));
    OK(graphics_create_resources(device, &resource_desc, &resources[0]));
    CHECK(device->stats.descriptor_pool_creations - pools == 2U);
    for (uint32_t i = 0U; i < 70U; ++i) {
        OK(graphics_destroy_resources(device, &resources[i]));
    }
    Gpu_Binding_Desc_t array_binding = {UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 8U};
    Gpu_Resource_Create_Info_t array_desc = {1U, &array_binding};
    for (uint32_t i = 0U; i < 20U; ++i) {
        OK(graphics_create_resources(device, &array_desc, &resources[i]));
    }
    CHECK(resources[0].pool != resources[16].pool); // descriptor capacity, not maxSets
    CHECK(device->stats.descriptor_pool_creations - pools == 2U); // existing pools reused
    for (uint32_t i = 0U; i < 20U; ++i) {
        OK(graphics_destroy_resources(device, &resources[i]));
    }
    bindings[0].count = 0U;
    CHECK(graphics_create_resources(device, &resource_desc, &resources[0]) == Graphics_ERROR_INVALID_ARGUMENT);
    OK(graphics_destroy_upload_batch(device, &batch));
    OK(graphics_destroy_upload_batch(device, &batch));
    OK(graphics_destroy_buffer(device, &buffer));
    OK(graphics_destroy_texture(device, &textures[0]));
    // The borrowed sampler remains alive after its first texture is destroyed.
    CHECK(sampler != VK_NULL_HANDLE);
    OK(graphics_destroy_texture(device, &textures[1]));
    OK(graphics_destroy_sampler(device, &sampler));
}

int main(void) {
    (void)setvbuf(stdout, NULL, _IONBF, 0U);
    puts("Initializing headless Vulkan device...");
    Graphics_Device_t device = {0};
    bool ready = init_device(&device);
    CHECK(ready);
    if (ready) {
        puts("Testing ranges and GPU copies...");
        test_ranges(&device);
        puts("Testing memory reuse and capacity...");
        test_memory(&device);
        puts("Testing uploads, readback and descriptors...");
        test_uploads_and_resources(&device);
    }
    printf("Counters: memory allocations=%llu descriptor pools=%llu uploads=%llu\n",
        (unsigned long long)device.stats.memory_allocations,
        (unsigned long long)device.stats.descriptor_pool_creations,
        (unsigned long long)device.stats.upload_submissions);
    OK(graphics_shutdown(&device));
    CHECK(validation_errors == 0U);
    printf("Graphics regression tests: %u failures, %u Vulkan validation errors.\n", failures, validation_errors);
    return failures == 0U ? 0 : 1;
}
