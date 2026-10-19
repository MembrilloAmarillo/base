#include "graphics_internal.h"

void graphics_device_cleanup(Graphics_Device_t *device) {
    if (device->device != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(device->device, device->pipeline_cache, NULL);
        for (uint32_t i = 0U; i < GRAPHICS_MAX_DESCRIPTOR_POOLS; ++i)
            vkDestroyDescriptorPool(device->device, device->descriptor_pools[i].pool, NULL);
        for (uint32_t i = 0U; i < GRAPHICS_MAX_MEMORY_BLOCKS; ++i) {
            Graphics_Memory_Block_t *block = &device->memory_blocks[i];
            if (block->mapped != NULL)
                vkUnmapMemory(device->device, block->memory);
            vkFreeMemory(device->device, block->memory, NULL);
        }
        vkDestroyDevice(device->device, NULL);
    }
    if (device->instance != VK_NULL_HANDLE) {
        if (device->debug_messenger != VK_NULL_HANDLE) {
            PFN_vkDestroyDebugUtilsMessengerEXT destroy =
                (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                    device->instance, "vkDestroyDebugUtilsMessengerEXT");
            if (destroy != NULL)
                destroy(device->instance, device->debug_messenger, NULL);
        }
        vkDestroyInstance(device->instance, NULL);
    }
    free(device);
}

Graphics_Result graphics_shutdown(Graphics_Device_t **device) {
    if (device == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (*device == NULL)
        return Graphics_NONE;
    if ((*device)->live_objects != 0U)
        return Graphics_ERROR_RESOURCE;
    Graphics_Result result = graphics_wait_idle(*device);
    if (result == Graphics_NONE || result == Graphics_DEVICE_LOST) {
        graphics_device_cleanup(*device);
        *device = NULL;
    }
    return result;
}

Graphics_Result graphics_destroy_pipeline(Graphics_Device_t *device, Gpu_Pipeline_t *pipeline) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (pipeline == NULL ||
        (pipeline->owner != VK_NULL_HANDLE && pipeline->owner != device->device))
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (pipeline->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device->device, pipeline->pipeline, NULL);
        vkDestroyPipelineLayout(device->device, pipeline->layout, NULL);
        for (uint32_t i = 0U; i < pipeline->layout_count; ++i)
            --pipeline->layouts[i]->references;
        --device->live_objects;
    }
    *pipeline = (Gpu_Pipeline_t){0};
    return Graphics_NONE;
}

Graphics_Result graphics_create_sampler(Graphics_Device_t *device,
                                        const VkSamplerCreateInfo *desc,
                                        Gpu_Sampler_t *out) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (out == NULL || out->sampler != VK_NULL_HANDLE)
        return Graphics_ERROR_INVALID_ARGUMENT;
    VkSamplerCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxLod = VK_LOD_CLAMP_NONE,
    };
    if (desc != NULL)
        info = *desc;
    if (info.sType != VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO || info.pNext != NULL ||
        info.flags != 0U ||
        (info.anisotropyEnable &&
         (!device->enabled.sampler_anisotropy || info.maxAnisotropy < 1.0f ||
          info.maxAnisotropy > device->properties.limits.maxSamplerAnisotropy)))
        return Graphics_ERROR_INVALID_ARGUMENT;
    Gpu_Sampler_t sampler = {0};
    Graphics_Result result =
        graphics_vk_result(device,
                           vkCreateSampler(device->device, &info, NULL, &sampler.sampler),
                           Graphics_ERROR_RESOURCE);
    if (result == Graphics_NONE) {
        sampler.owner = device->device;
        *out = sampler;
        ++device->live_objects;
    }
    return result;
}

Graphics_Result graphics_destroy_sampler(Graphics_Device_t *device, Gpu_Sampler_t *sampler) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (sampler == NULL || (sampler->owner != VK_NULL_HANDLE && sampler->owner != device->device))
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (sampler->sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device->device, sampler->sampler, NULL);
        --device->live_objects;
    }
    *sampler = (Gpu_Sampler_t){0};
    return Graphics_NONE;
}
