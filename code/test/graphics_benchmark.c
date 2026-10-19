/* Optimized, validation-free microbenchmarks. GRAPHICS_BASELINE builds this
 * harness against the saved pre-cleanup backend for like-for-like comparison.
 * Private access is used only for diagnostic counters and GPU timestamp probes. */
#include "rendering/graphics_internal.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_COUNT 128U
#define BUFFER_BYTES 4096U
#define SAMPLES 80U
static unsigned failures;
#define OK(call)                                                                                   \
    do {                                                                                           \
        if ((call) != Graphics_NONE) {                                                             \
            fprintf(stderr, "Benchmark failed: %s at %d\n", #call, __LINE__);                      \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static double milliseconds(uint64_t ticks) {
    return (double)ticks * 1000.0 / (double)SDL_GetPerformanceFrequency();
}
static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left, b = *(const double *)right;
    return a < b ? -1 : (a > b ? 1 : 0);
}

#ifdef GRAPHICS_BASELINE
#define GPU_MEMORY_UPLOAD CPU_VISIBLE
#define GPU_MEMORY_DEVICE GPU_ONLY
static Graphics_Result bench_init(Graphics_Device_t *device) {
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                             .apiVersion = VK_API_VERSION_1_3};
    VkInstanceCreateInfo instance = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                     .pApplicationInfo = &app};
    if (vkCreateInstance(&instance, NULL, &device->instance) != VK_SUCCESS)
        return Graphics_NO_DEVICE;
    VkPhysicalDevice physical[32];
    uint32_t count = 32U;
    if (vkEnumeratePhysicalDevices(device->instance, &count, physical) != VK_SUCCESS || count == 0U)
        return Graphics_NO_DEVICE;
    device->physical_device = physical[0];
    for (uint32_t i = 0U; i < count; ++i) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical[i], &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            device->physical_device = physical[i];
            break;
        }
    }
    VkQueueFamilyProperties queues[32];
    count = 32U;
    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &count, queues);
    for (uint32_t i = 0U; i < count; ++i)
        if ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            device->queue_family = i;
            break;
        }
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                     .queueFamilyIndex = device->queue_family,
                                     .queueCount = 1U,
                                     .pQueuePriorities = &priority};
    VkPhysicalDeviceVulkan13Features f13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE};
    VkPhysicalDeviceVulkan12Features f12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &f13,
        .bufferDeviceAddress = VK_TRUE};
    VkDeviceCreateInfo info = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                               .pNext = &f12,
                               .queueCreateInfoCount = 1U,
                               .pQueueCreateInfos = &queue};
    if (vkCreateDevice(device->physical_device, &info, NULL, &device->device) != VK_SUCCESS)
        return Graphics_NO_DEVICE;
    return graphics_create_commands(
        device, device->queue_family, &device->command_pool, device->command_buffers);
}
#endif

static void benchmark_allocations(Graphics_Device_t *device) {
    Gpu_Buffer_t buffers[512] = {0};
    Gpu_Buffer_Desc_t desc = {256U,
                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              GPU_MEMORY_UPLOAD};
    uint64_t steps = device->stats.allocation_search_steps;
    uint64_t start = SDL_GetPerformanceCounter();
    for (uint32_t round = 0U; round < 8U && failures == 0U; ++round) {
        for (uint32_t i = 0U; i < 512U; ++i)
            OK(graphics_create_buffer(device, &desc, &buffers[i]));
        for (uint32_t i = 0U; i < 512U; i += 2U)
            OK(graphics_destroy_buffer(device, &buffers[i]));
        for (uint32_t i = 0U; i < 512U; i += 2U)
            OK(graphics_create_buffer(device, &desc, &buffers[i]));
        for (uint32_t i = 0U; i < 512U; ++i)
            OK(graphics_destroy_buffer(device, &buffers[i]));
    }
    printf("allocation_churn_ms=%.3f search_steps=%llu\n",
           milliseconds(SDL_GetPerformanceCounter() - start),
           (unsigned long long)(device->stats.allocation_search_steps - steps));
}

static void benchmark_descriptors(Graphics_Device_t *device) {
#ifdef GRAPHICS_BASELINE
    Gpu_Binding_Desc_t binding = {UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1U};
    Gpu_Resource_Create_Info_t desc = {1U, &binding};
    Gpu_Resource_t sets[70] = {0};
#else
    Gpu_Binding_Desc_t binding = {
        0U, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1U};
    Gpu_Binding_Layout_t *layout = NULL;
    OK(graphics_create_binding_layout(device, &binding, 1U, &layout));
    Gpu_Binding_Set_t sets[70] = {0};
#endif
    uint64_t start = SDL_GetPerformanceCounter();
    for (uint32_t round = 0U; round < 40U && failures == 0U; ++round) {
        for (uint32_t i = 0U; i < 70U; ++i) {
#ifdef GRAPHICS_BASELINE
            OK(graphics_create_resources(device, &desc, &sets[i]));
#else
            OK(graphics_create_resources(device, layout, &sets[i]));
#endif
        }
        for (uint32_t i = 0U; i < 70U; ++i)
            OK(graphics_destroy_resources(device, &sets[i]));
    }
    printf("descriptor_churn_ms=%.3f layouts=%llu set_bytes=%llu\n",
           milliseconds(SDL_GetPerformanceCounter() - start),
           (unsigned long long)device->stats.descriptor_layout_creations,
           (unsigned long long)sizeof(sets[0]));
#ifndef GRAPHICS_BASELINE
    OK(graphics_destroy_binding_layout(device, &layout));
#endif
}

static void benchmark_uploads(Graphics_Device_t *device) {
    Gpu_Buffer_t buffers[BUFFER_COUNT] = {0};
    Gpu_Buffer_Desc_t desc = {BUFFER_BYTES,
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              GPU_MEMORY_DEVICE};
    for (uint32_t i = 0U; i < BUFFER_COUNT; ++i)
        OK(graphics_create_buffer(device, &desc, &buffers[i]));
    uint8_t data[BUFFER_BYTES];
    memset(data, 0x55, sizeof(data));
#ifdef GRAPHICS_BASELINE
    Gpu_Upload_Batch_t storage = {0};
    Gpu_Upload_Batch_t *batch = &storage;
    OK(graphics_create_upload_batch(device, BUFFER_COUNT * BUFFER_BYTES, batch));
#else
    Gpu_Upload_Batch_t *batch = NULL;
    OK(graphics_create_upload_batch(device, BUFFER_COUNT * BUFFER_BYTES, &batch));
#endif
    VkQueryPool query = VK_NULL_HANDLE;
    VkQueryPoolCreateInfo query_info = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                        .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                        .queryCount = 2U};
    if (vkCreateQueryPool(device->device, &query_info, NULL, &query) != VK_SUCCESS) {
        ++failures;
    }
#ifndef GRAPHICS_BASELINE
    batch->profile_queries = query;
#endif
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device->physical_device, &properties);
    VkQueueFamilyProperties queues[32];
    uint32_t queue_count = 32U;
    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &queue_count, queues);
    uint32_t valid_bits =
        device->queue_family < queue_count ? queues[device->queue_family].timestampValidBits : 0U;
    uint64_t timestamp_mask = valid_bits == 64U ? UINT64_MAX : (UINT64_C(1) << valid_bits) - 1U;
    double cpu[SAMPLES] = {0}, gpu[SAMPLES] = {0};
    uint64_t barriers = device->stats.upload_barrier_calls;
    for (uint32_t round = 0U; round < SAMPLES + 8U && failures == 0U; ++round) {
        uint64_t start = SDL_GetPerformanceCounter();
#ifdef GRAPHICS_BASELINE
        OK(graphics_begin_uploads(device, batch));
        vkCmdResetQueryPool(batch->command, query, 0U, 2U);
        vkCmdWriteTimestamp2(batch->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, query, 0U);
#else
        OK(graphics_begin_uploads(batch));
#endif
        for (uint32_t i = 0U; i < BUFFER_COUNT; ++i) {
#ifdef GRAPHICS_BASELINE
            OK(graphics_upload_buffer(
                device, batch, &buffers[i], data, (Gpu_Range_t){0U, BUFFER_BYTES}));
#else
            OK(graphics_upload_buffer(batch,
                                      &buffers[i],
                                      0U,
                                      data,
                                      BUFFER_BYTES,
                                      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT |
                                          VK_PIPELINE_STAGE_2_HOST_BIT,
                                      VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT |
                                          VK_ACCESS_2_HOST_READ_BIT));
#endif
        }
#ifdef GRAPHICS_BASELINE
        vkCmdWriteTimestamp2(batch->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, query, 1U);
        OK(graphics_submit_uploads(device, batch));
#else
        OK(graphics_submit_uploads(batch));
#endif
        double cpu_ms = milliseconds(SDL_GetPerformanceCounter() - start);
#ifdef GRAPHICS_BASELINE
        OK(graphics_wait_uploads(device, batch));
#else
        OK(graphics_wait_uploads(batch));
#endif
        uint64_t times[2] = {0};
        if (vkGetQueryPoolResults(device->device,
                                  query,
                                  0U,
                                  2U,
                                  sizeof(times),
                                  times,
                                  sizeof(times[0]),
                                  VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS)
            ++failures;
        if (round >= 8U) {
            cpu[round - 8U] = cpu_ms;
            gpu[round - 8U] = (double)((times[1] - times[0]) & timestamp_mask) *
                              properties.limits.timestampPeriod / 1000000.0;
        }
    }
    qsort(cpu, SAMPLES, sizeof(cpu[0]), compare_double);
    qsort(gpu, SAMPLES, sizeof(gpu[0]), compare_double);
    printf(
        "upload_512KiB_128copies cpu_p50_ms=%.4f cpu_p95_ms=%.4f gpu_p50_ms=%.4f gpu_p95_ms=%.4f barriers_per_batch=%llu\n",
        cpu[SAMPLES / 2U],
        cpu[SAMPLES * 95U / 100U],
        gpu[SAMPLES / 2U],
        gpu[SAMPLES * 95U / 100U],
        (unsigned long long)((device->stats.upload_barrier_calls - barriers) / (SAMPLES + 8U)));
#ifdef GRAPHICS_BASELINE
    OK(graphics_destroy_upload_batch(device, batch));
#else
    OK(graphics_destroy_upload_batch(&batch));
#endif
    vkDestroyQueryPool(device->device, query, NULL);
    for (uint32_t i = 0U; i < BUFFER_COUNT; ++i)
        OK(graphics_destroy_buffer(device, &buffers[i]));
}

#ifndef GRAPHICS_BASELINE
static void benchmark_streaming(Graphics_Device_t *device) {
    Gpu_Buffer_t destinations[3] = {0};
    Gpu_Upload_Batch_t *batches[3] = {0};
    Gpu_Buffer_Desc_t desc = {
        BUFFER_COUNT * BUFFER_BYTES, VK_BUFFER_USAGE_TRANSFER_DST_BIT, GPU_MEMORY_DEVICE};
    uint8_t *data = (uint8_t *)calloc(1U, (size_t)desc.size);
    if (data == NULL) {
        ++failures;
        return;
    }
    for (uint32_t i = 0U; i < 3U; ++i) {
        OK(graphics_create_buffer(device, &desc, &destinations[i]));
        OK(graphics_create_upload_batch(device, desc.size, &batches[i]));
    }
    unsigned waits = 0U;
    uint64_t wait_ticks = 0U;
    uint64_t start = SDL_GetPerformanceCounter();
    for (uint32_t frame = 0U; frame < 240U && failures == 0U; ++frame) {
        uint32_t slot = frame % 3U;
        Graphics_Result ready = graphics_poll_uploads(batches[slot]);
        if (ready == Graphics_BUSY) {
            ++waits;
            uint64_t wait_start = SDL_GetPerformanceCounter();
            OK(graphics_wait_uploads(batches[slot]));
            wait_ticks += SDL_GetPerformanceCounter() - wait_start;
        } else if (ready != Graphics_NONE)
            ++failures;
        OK(graphics_begin_uploads(batches[slot]));
        OK(graphics_upload_buffer(batches[slot],
                                  &destinations[slot],
                                  0U,
                                  data,
                                  (size_t)desc.size,
                                  VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                  VK_ACCESS_2_MEMORY_READ_BIT));
        OK(graphics_submit_uploads(batches[slot]));
    }
    for (uint32_t i = 0U; i < 3U; ++i)
        OK(graphics_wait_uploads(batches[i]));
    double elapsed = milliseconds(SDL_GetPerformanceCounter() - start);
    printf("streaming_3batches_120MiB_ms=%.3f throughput_MiB_s=%.1f reuse_waits=%u wait_ms=%.3f\n",
           elapsed,
           120000.0 / elapsed,
           waits,
           milliseconds(wait_ticks));
    for (uint32_t i = 0U; i < 3U; ++i) {
        OK(graphics_destroy_upload_batch(&batches[i]));
        OK(graphics_destroy_buffer(device, &destinations[i]));
    }
    free(data);
}
#endif

int main(void) {
    (void)setvbuf(stdout, NULL, _IONBF, 0U);
#ifdef GRAPHICS_BASELINE
    Graphics_Device_t storage = {0};
    Graphics_Device_t *device = &storage;
    OK(bench_init(device));
#else
    Graphics_Device_t *device = NULL;
    Graphics_Device_Desc_t desc = {.buffer_device_address = true};
    OK(graphics_init(&desc, &device));
#endif
    if (failures == 0U) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device->physical_device, &properties);
        printf("GPU: %s; optimized build; validation disabled\n", properties.deviceName);
        benchmark_allocations(device);
        benchmark_descriptors(device);
        if (properties.limits.timestampComputeAndGraphics)
            benchmark_uploads(device);
        else
            puts("GPU timestamps unsupported; upload benchmark skipped.");
#ifndef GRAPHICS_BASELINE
        benchmark_streaming(device);
#endif
    }
#ifdef GRAPHICS_BASELINE
    OK(graphics_shutdown(device));
#else
    OK(graphics_shutdown(&device));
#endif
    return failures == 0U ? 0 : 1;
}
