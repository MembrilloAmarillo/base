#ifndef GRAPHICS_INTERNAL_H
#define GRAPHICS_INTERNAL_H

#include "graphics.h"

Graphics_Result graphics_memory_acquire(Graphics_Device_t *device,
    const VkMemoryRequirements *requirements, Gpu_Alloc_Type_t memory,
    VkBuffer buffer, VkImage image, bool dedicated, uint32_t *allocation);
void graphics_memory_release(Graphics_Device_t *device, uint32_t allocation);
uint32_t graphics_format_bytes(VkFormat format);
Graphics_Result graphics_submit_and_wait(Graphics_Device_t *device, VkCommandBuffer command);

static inline uint32_t graphics_descriptor_type_index(VkDescriptorType type) {
    uint32_t index = 6U;
    switch (type) {
        case VK_DESCRIPTOR_TYPE_SAMPLER: index = 0U; break;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: index = 1U; break;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: index = 2U; break;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: index = 3U; break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: index = 4U; break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: index = 5U; break;
        default: break;
    }
    return index;
}

#endif
