#include "graphics_internal.h"

Graphics_Result graphics_destroy_resources(Graphics_Device_t *device, Gpu_Resource_t *resource) {
    Graphics_Result result = Graphics_NONE;
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        result = Graphics_NO_DEVICE;
    } else if (resource == NULL || (resource->owner != VK_NULL_HANDLE && resource->owner != device->device)) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    } else {
        if (resource->set != VK_NULL_HANDLE) {
            if (vkFreeDescriptorSets(device->device, resource->pool, 1U, &resource->set) != VK_SUCCESS) {
                result = Graphics_ERROR_RESOURCE;
            } else {
                for (uint32_t i = 0U; i < GRAPHICS_MAX_DESCRIPTOR_POOLS; ++i) {
                    Graphics_Descriptor_Pool_t *pool = &device->descriptor_pools[i];
                    if (pool->pool == resource->pool) {
                        --pool->live_sets;
                        for (uint32_t b = 0U; b < resource->binding_count; ++b) {
                            uint32_t index = graphics_descriptor_type_index(resource->bindings[b].descriptorType);
                            if (index < 6U) {
                                pool->available[index] += resource->bindings[b].descriptorCount;
                            }
                        }
                    }
                }
            }
        }
        if (result == Graphics_NONE) {
            vkDestroyDescriptorSetLayout(device->device, resource->layout, NULL);
            *resource = (Gpu_Resource_t){0};
        }
    }
    return result;
}

Graphics_Result graphics_destroy_pipeline(Graphics_Device_t *device, Gpu_Pipeline_t *pipeline) {
    Graphics_Result result = Graphics_NONE;
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        result = Graphics_NO_DEVICE;
    } else if (pipeline == NULL || (pipeline->owner != VK_NULL_HANDLE && pipeline->owner != device->device)) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    } else {
        vkDestroyPipeline(device->device, pipeline->pipeline, NULL);
        vkDestroyPipelineLayout(device->device, pipeline->layout, NULL);
        *pipeline = (Gpu_Pipeline_t){0};
        graphics_invalidate_state(device);
    }
    return result;
}

Graphics_Result graphics_destroy_sampler(Graphics_Device_t *device, VkSampler *sampler) {
    Graphics_Result result = Graphics_NONE;
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        result = Graphics_NO_DEVICE;
    } else if (sampler == NULL) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    } else {
        vkDestroySampler(device->device, *sampler, NULL);
        *sampler = VK_NULL_HANDLE;
    }
    return result;
}

Graphics_Result graphics_shutdown(Graphics_Device_t *device) {
    Graphics_Result result = Graphics_NONE;
    if (device == NULL) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    } else {
        for (uint32_t i = 0U; i < GRAPHICS_MAX_MEMORY_BLOCKS; ++i) {
            if (device->memory_blocks[i].live_allocations != 0U) {
                result = Graphics_ERROR_RESOURCE;
            }
        }
        for (uint32_t i = 0U; i < GRAPHICS_MAX_DESCRIPTOR_POOLS; ++i) {
            if (device->descriptor_pools[i].live_sets != 0U) {
                result = Graphics_ERROR_RESOURCE;
            }
        }
    }
    if (result == Graphics_NONE && device->device != VK_NULL_HANDLE) {
        VkResult wait_result = vkDeviceWaitIdle(device->device);
        if (wait_result != VK_SUCCESS && wait_result != VK_ERROR_DEVICE_LOST) {
            result = Graphics_ERROR_FENCE;
        } else {
            vkDestroyCommandPool(device->device, device->command_pool, NULL);
            vkDestroyPipelineCache(device->device, device->pipeline_cache, NULL);
            for (uint32_t i = 0U; i < GRAPHICS_MAX_DESCRIPTOR_POOLS; ++i) {
                vkDestroyDescriptorPool(device->device, device->descriptor_pools[i].pool, NULL);
            }
            for (uint32_t i = 0U; i < GRAPHICS_MAX_MEMORY_BLOCKS; ++i) {
                Graphics_Memory_Block_t *block = &device->memory_blocks[i];
                if (block->mapped != NULL) {
                    vkUnmapMemory(device->device, block->memory);
                }
                vkFreeMemory(device->device, block->memory, NULL);
            }
            for (uint32_t i = 0U; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                vkDestroyFence(device->device, device->fences[i], NULL);
                vkDestroySemaphore(device->device, device->image_semaphores[i], NULL);
            }
            for (uint32_t i = 0U; i < device->swapchain_image_count; ++i) {
                vkDestroySemaphore(device->device, device->render_complete_semaphores[i], NULL);
                vkDestroyImageView(device->device, device->swapchain_img_views[i], NULL);
            }
            vkDestroyImageView(device->device, device->depth_img_view, NULL);
            vkDestroyImage(device->device, device->depth_image, NULL);
            vkFreeMemory(device->device, device->depth_img_mem, NULL);
            if (device->swapchain != VK_NULL_HANDLE) {
                vkDestroySwapchainKHR(device->device, device->swapchain, NULL);
            }
            vkDestroyDevice(device->device, NULL);
        }
    }
    if (result == Graphics_NONE) {
        if (device->instance != VK_NULL_HANDLE) {
            if (device->surface != VK_NULL_HANDLE) {
                vkDestroySurfaceKHR(device->instance, device->surface, NULL);
            }
            if (device->debug_messenger != VK_NULL_HANDLE) {
                PFN_vkDestroyDebugUtilsMessengerEXT destroy = (PFN_vkDestroyDebugUtilsMessengerEXT)
                    vkGetInstanceProcAddr(device->instance, "vkDestroyDebugUtilsMessengerEXT");
                if (destroy != NULL) {
                    destroy(device->instance, device->debug_messenger, NULL);
                }
            }
            vkDestroyInstance(device->instance, NULL);
        }
        SDL_DestroyWindow(device->window);
        SDL_Quit();
        *device = (Graphics_Device_t){0};
    }
    return result;
}
