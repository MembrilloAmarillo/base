#include "graphics_internal.h"

static uint32_t memory_type(Graphics_Device_t *device, uint32_t mask,
                            Gpu_Alloc_Type_t kind, VkMemoryPropertyFlags *flags_out) {
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(device->physical_device, &properties);
    VkMemoryPropertyFlags required = kind == GPU_ONLY ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT :
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t selected = UINT32_MAX;
    int best = -1;
    for (uint32_t i = 0U; i < properties.memoryTypeCount; ++i) {
        VkMemoryPropertyFlags flags = properties.memoryTypes[i].propertyFlags;
        if ((mask & (1U << i)) != 0U && (flags & required) == required) {
            int score = 0;
            if (kind == CACHED && (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0U) {
                score += 2;
            }
            if (kind == CPU_VISIBLE && (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0U) {
                score += 1;
            }
            if (kind == GPU_ONLY && (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0U) {
                score += 1;
            }
            if (score > best) {
                best = score;
                selected = i;
                *flags_out = flags;
            }
        }
    }
    return selected;
}

/* First-fit over live allocations. Freed holes are immediately reusable; the
 * bounded tables avoid heap allocation and free-list fragmentation bookkeeping. */
static bool block_offset(const Graphics_Device_t *device, uint32_t block_index,
                          const VkMemoryRequirements *requirements, VkDeviceSize *offset_out) {
    VkDeviceSize offset = 0U;
    const Graphics_Memory_Block_t *block = &device->memory_blocks[block_index];
    bool searching = true;
    bool fits = false;
    while (searching) {
        VkDeviceSize padding = (requirements->alignment - offset % requirements->alignment) % requirements->alignment;
        if (offset > block->size || padding > block->size - offset) {
            searching = false;
        } else {
            offset += padding;
            if (requirements->size > block->size - offset) {
                searching = false;
            } else {
                bool overlap = false;
                for (uint32_t i = 0U; i < GRAPHICS_MAX_ALLOCATIONS && !overlap; ++i) {
                    const Graphics_Allocation_t *a = &device->allocations[i];
                    if (a->block == block_index + 1U && offset < a->offset + a->size &&
                        a->offset < offset + requirements->size) {
                        offset = a->offset + a->size;
                        overlap = true;
                    }
                }
                if (!overlap) {
                    *offset_out = offset;
                    fits = true;
                    searching = false;
                }
            }
        }
    }
    return fits;
}

Graphics_Result graphics_memory_acquire(Graphics_Device_t *device,
    const VkMemoryRequirements *requirements, Gpu_Alloc_Type_t kind,
    VkBuffer buffer, VkImage image, bool dedicated, uint32_t *allocation) {
    Graphics_Result result = Graphics_NONE;
    VkMemoryPropertyFlags flags = 0U;
    uint32_t type = memory_type(device, requirements->memoryTypeBits, kind, &flags);
    uint32_t slot = 0U;
    while (slot < GRAPHICS_MAX_ALLOCATIONS && device->allocations[slot].block != 0U) {
        ++slot;
    }
    uint32_t block_index = GRAPHICS_MAX_MEMORY_BLOCKS;
    VkDeviceSize offset = 0U;
    bool is_image = image != VK_NULL_HANDLE;
    dedicated = dedicated || requirements->size > GRAPHICS_MEMORY_BLOCK_SIZE;
    if (type == UINT32_MAX) {
        result = Graphics_NO_DEVICE_MEM;
    } else if (slot == GRAPHICS_MAX_ALLOCATIONS) {
        result = Graphics_NO_MEMORY;
    }
    if (result == Graphics_NONE && !dedicated) {
        for (uint32_t i = 0U; i < GRAPHICS_MAX_MEMORY_BLOCKS && block_index == GRAPHICS_MAX_MEMORY_BLOCKS; ++i) {
            Graphics_Memory_Block_t *block = &device->memory_blocks[i];
            if (block->memory != VK_NULL_HANDLE && !block->dedicated &&
                block->memory_type == type && block->image == is_image &&
                block_offset(device, i, requirements, &offset)) {
                block_index = i;
            }
        }
    }
    if (result == Graphics_NONE && block_index == GRAPHICS_MAX_MEMORY_BLOCKS) {
        for (uint32_t i = 0U; i < GRAPHICS_MAX_MEMORY_BLOCKS && block_index == GRAPHICS_MAX_MEMORY_BLOCKS; ++i) {
            if (device->memory_blocks[i].memory == VK_NULL_HANDLE) {
                block_index = i;
            }
        }
        if (block_index == GRAPHICS_MAX_MEMORY_BLOCKS) {
            result = Graphics_NO_MEMORY;
        } else {
            VkMemoryDedicatedAllocateInfo dedicated_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
                .image = image, .buffer = buffer,
            };
            VkMemoryAllocateFlagsInfo address_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
                .pNext = dedicated ? &dedicated_info : NULL,
                .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            };
            VkMemoryAllocateInfo info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext = is_image ? (dedicated ? (const void *)&dedicated_info : NULL) : (const void *)&address_info,
                .allocationSize = dedicated ? requirements->size : GRAPHICS_MEMORY_BLOCK_SIZE,
                .memoryTypeIndex = type,
            };
            Graphics_Memory_Block_t *block = &device->memory_blocks[block_index];
            VkDeviceMemory memory = VK_NULL_HANDLE;
            if (vkAllocateMemory(device->device, &info, NULL, &memory) != VK_SUCCESS) {
                result = Graphics_NO_DEVICE_MEM;
            } else {
                *block = (Graphics_Memory_Block_t){
                    .memory = memory, .size = info.allocationSize,
                    .memory_type = type, .image = is_image, .dedicated = dedicated,
                };
                ++device->stats.memory_allocations;
                if (!is_image && (flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                    if (vkMapMemory(device->device, memory, 0U, VK_WHOLE_SIZE, 0U, &block->mapped) != VK_SUCCESS) {
                        vkFreeMemory(device->device, memory, NULL);
                        *block = (Graphics_Memory_Block_t){0};
                        result = Graphics_NO_MEMORY;
                    }
                }
            }
        }
    }
    if (result == Graphics_NONE) {
        device->allocations[slot] = (Graphics_Allocation_t){offset, requirements->size, block_index + 1U};
        ++device->memory_blocks[block_index].live_allocations;
        *allocation = slot + 1U;
    }
    return result;
}

void graphics_memory_release(Graphics_Device_t *device, uint32_t allocation) {
    if (allocation > 0U && allocation <= GRAPHICS_MAX_ALLOCATIONS) {
        Graphics_Allocation_t *a = &device->allocations[allocation - 1U];
        if (a->block != 0U) {
            Graphics_Memory_Block_t *block = &device->memory_blocks[a->block - 1U];
            --block->live_allocations;
            if (block->dedicated) {
                if (block->mapped != NULL) {
                    vkUnmapMemory(device->device, block->memory);
                }
                vkFreeMemory(device->device, block->memory, NULL);
                *block = (Graphics_Memory_Block_t){0};
            }
            *a = (Graphics_Allocation_t){0};
        }
    }
}

Graphics_Result graphics_create_buffer(Graphics_Device_t *device, const Gpu_Buffer_Desc_t *desc, Gpu_Buffer_t *out) {
    Graphics_Result result = Graphics_NONE;
    Gpu_Buffer_t buffer = {0};
    if (device == NULL || device->device == VK_NULL_HANDLE || device->physical_device == VK_NULL_HANDLE) {
        result = Graphics_NO_DEVICE;
    } else if (desc == NULL || out == NULL || out->buffer != VK_NULL_HANDLE ||
               desc->size == 0U || desc->usage == 0U ||
               (desc->memory != CPU_VISIBLE && desc->memory != GPU_ONLY && desc->memory != CACHED)) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    }
    if (result == Graphics_NONE) {
        VkBufferCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = desc->size, .usage = desc->usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        if (vkCreateBuffer(device->device, &info, NULL, &buffer.buffer) != VK_SUCCESS) {
            buffer.buffer = VK_NULL_HANDLE;
            result = Graphics_NO_BUFFER;
        }
    }
    if (result == Graphics_NONE) {
        VkMemoryDedicatedRequirements dedicated = {.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
        VkMemoryRequirements2 requirements = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, .pNext = &dedicated};
        VkBufferMemoryRequirementsInfo2 info = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, .buffer = buffer.buffer};
        vkGetBufferMemoryRequirements2(device->device, &info, &requirements);
        result = graphics_memory_acquire(device, &requirements.memoryRequirements, desc->memory,
            buffer.buffer, VK_NULL_HANDLE, dedicated.requiresDedicatedAllocation == VK_TRUE, &buffer.allocation);
    }
    if (result == Graphics_NONE) {
        Graphics_Allocation_t *a = &device->allocations[buffer.allocation - 1U];
        Graphics_Memory_Block_t *block = &device->memory_blocks[a->block - 1U];
        buffer.memory = block->memory;
        buffer.memory_offset = a->offset;
        buffer.mapped = block->mapped != NULL ? (uint8_t *)block->mapped + a->offset : NULL;
        if (vkBindBufferMemory(device->device, buffer.buffer, buffer.memory, a->offset) != VK_SUCCESS) {
            result = Graphics_NO_DEVICE_MEM;
        }
    }
    if (result == Graphics_NONE) {
        buffer.owner = device->device;
        buffer.size = desc->size;
        buffer.usage = desc->usage;
        VkPhysicalDeviceMemoryProperties properties;
        vkGetPhysicalDeviceMemoryProperties(device->physical_device, &properties);
        uint32_t block = device->allocations[buffer.allocation - 1U].block - 1U;
        buffer.type = properties.memoryTypes[device->memory_blocks[block].memory_type].propertyFlags;
        if ((desc->usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0U) {
            VkBufferDeviceAddressInfo info = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer.buffer};
            buffer.address = vkGetBufferDeviceAddress(device->device, &info);
        }
        *out = buffer;
    } else if (buffer.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device->device, buffer.buffer, NULL);
        graphics_memory_release(device, buffer.allocation);
    }
    return result;
}

Graphics_Result graphics_destroy_buffer(Graphics_Device_t *device, Gpu_Buffer_t *buffer) {
    Graphics_Result result = Graphics_NONE;
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        result = Graphics_NO_DEVICE;
    } else if (buffer == NULL || (buffer->owner != VK_NULL_HANDLE && buffer->owner != device->device)) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    } else {
        vkDestroyBuffer(device->device, buffer->buffer, NULL);
        graphics_memory_release(device, buffer->allocation);
        *buffer = (Gpu_Buffer_t){0};
    }
    return result;
}

uint32_t graphics_format_bytes(VkFormat format) {
    uint32_t bytes = 0U;
    switch (format) {
        case VK_FORMAT_R8_UNORM: bytes = 1U; break;
        case VK_FORMAT_R8G8_UNORM: bytes = 2U; break;
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB: bytes = 4U; break;
        default: break;
    }
    return bytes;
}

Graphics_Result graphics_create_texture(Graphics_Device_t *device, const Gpu_Texture_Desc_t *desc, Gpu_Texture_t *out) {
    Graphics_Result result = Graphics_NONE;
    Gpu_Texture_t texture = {0};
    if (device == NULL || device->device == VK_NULL_HANDLE || device->physical_device == VK_NULL_HANDLE) {
        result = Graphics_NO_DEVICE;
    } else if (desc == NULL || out == NULL || out->image != VK_NULL_HANDLE ||
               desc->width == 0U || desc->height == 0U || graphics_format_bytes((VkFormat)desc->format) == 0U ||
               desc->usage == 0U || (desc->usage & ~(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0U ||
               (desc->usage & (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) == 0U) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    }
    if (result == Graphics_NONE) {
        VkImageFormatProperties properties;
        VkResult supported = vkGetPhysicalDeviceImageFormatProperties(device->physical_device,
            (VkFormat)desc->format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, desc->usage, 0U, &properties);
        if (supported != VK_SUCCESS || desc->width > properties.maxExtent.width || desc->height > properties.maxExtent.height) {
            result = Graphics_ERROR_INVALID_ARGUMENT;
        }
    }
    if (result == Graphics_NONE) {
        texture.format = (VkFormat)desc->format;
        texture.extent = (VkExtent3D){desc->width, desc->height, 1U};
        texture.usage = desc->usage;
        texture.mip_levels = 1U;
        texture.array_layers = 1U;
        texture.barrier_aspects = VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageCreateInfo info = graphics_img_create_info(texture.format, texture.extent,
            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, desc->usage);
        if (vkCreateImage(device->device, &info, NULL, &texture.image) != VK_SUCCESS) {
            texture.image = VK_NULL_HANDLE;
            result = Graphics_ERROR_RESOURCE;
        }
    }
    if (result == Graphics_NONE) {
        VkMemoryDedicatedRequirements dedicated = {.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
        VkMemoryRequirements2 requirements = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, .pNext = &dedicated};
        VkImageMemoryRequirementsInfo2 info = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, .image = texture.image};
        vkGetImageMemoryRequirements2(device->device, &info, &requirements);
        result = graphics_memory_acquire(device, &requirements.memoryRequirements, GPU_ONLY,
            VK_NULL_HANDLE, texture.image, dedicated.requiresDedicatedAllocation == VK_TRUE, &texture.allocation);
    }
    if (result == Graphics_NONE) {
        Graphics_Allocation_t *a = &device->allocations[texture.allocation - 1U];
        texture.memory = device->memory_blocks[a->block - 1U].memory;
        texture.memory_offset = a->offset;
        if (vkBindImageMemory(device->device, texture.image, texture.memory, a->offset) != VK_SUCCESS) {
            result = Graphics_NO_DEVICE_MEM;
        }
    }
    if (result == Graphics_NONE) {
        VkImageViewCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = texture.image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = texture.format,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, 0U, 1U},
        };
        if (vkCreateImageView(device->device, &info, NULL, &texture.view) != VK_SUCCESS) {
            texture.view = VK_NULL_HANDLE;
            result = Graphics_NO_IMAGE_VIEW;
        }
    }
    if (result == Graphics_NONE) {
        texture.owner = device->device;
        *out = texture;
    } else if (texture.image != VK_NULL_HANDLE) {
        vkDestroyImage(device->device, texture.image, NULL);
        graphics_memory_release(device, texture.allocation);
    }
    return result;
}

Graphics_Result graphics_destroy_texture(Graphics_Device_t *device, Gpu_Texture_t *texture) {
    Graphics_Result result = Graphics_NONE;
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        result = Graphics_NO_DEVICE;
    } else if (texture == NULL || (texture->owner != VK_NULL_HANDLE && texture->owner != device->device)) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    } else {
        vkDestroyImageView(device->device, texture->view, NULL);
        vkDestroyImage(device->device, texture->image, NULL);
        graphics_memory_release(device, texture->allocation);
        *texture = (Gpu_Texture_t){0};
    }
    return result;
}
