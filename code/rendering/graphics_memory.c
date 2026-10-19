#include "graphics_internal.h"

static uint32_t memory_type(const Graphics_Device_t *device, uint32_t mask, Gpu_Alloc_Type_t kind) {
    VkMemoryPropertyFlags required =
        kind == GPU_MEMORY_DEVICE
            ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t selected = UINT32_MAX;
    int best = -1;
    for (uint32_t i = 0U; i < device->memory_properties.memoryTypeCount; ++i) {
        VkMemoryPropertyFlags flags = device->memory_properties.memoryTypes[i].propertyFlags;
        if ((mask & (1U << i)) != 0U && (flags & required) == required) {
            int score = 0;
            if (kind == GPU_MEMORY_READBACK && (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0U)
                score += 2;
            if (kind == GPU_MEMORY_UPLOAD && (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0U)
                score += 1;
            if (kind == GPU_MEMORY_DEVICE && (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0U)
                score += 1;
            if (score > best) {
                best = score;
                selected = i;
            }
        }
    }
    return selected;
}

/* One traversal of this block's offset-sorted list; no global rescans. */
static bool block_offset(Graphics_Device_t *device,
                         uint32_t block_index,
                         const VkMemoryRequirements *requirements,
                         VkDeviceSize *offset_out,
                         uint32_t *previous_out) {
    const Graphics_Memory_Block_t *block = &device->memory_blocks[block_index];
    VkDeviceSize offset = 0U;
    uint32_t previous = 0U;
    uint32_t next = block->first;
    bool searching = true;
    bool fits = false;
    while (searching) {
        ++device->stats.allocation_search_steps;
        VkDeviceSize padding =
            (requirements->alignment - offset % requirements->alignment) % requirements->alignment;
        VkDeviceSize end = next != 0U ? device->allocations[next - 1U].offset : block->size;
        if (offset <= end && padding <= end - offset &&
            requirements->size <= end - offset - padding) {
            *offset_out = offset + padding;
            *previous_out = previous;
            fits = true;
            searching = false;
        } else if (next == 0U) {
            searching = false;
        } else {
            const Graphics_Allocation_t *allocation = &device->allocations[next - 1U];
            offset = allocation->offset + allocation->size;
            previous = next;
            next = allocation->next;
        }
    }
    return fits;
}

static void release_block(Graphics_Device_t *device, Graphics_Memory_Block_t *block) {
    if (block->mapped != NULL)
        vkUnmapMemory(device->device, block->memory);
    vkFreeMemory(device->device, block->memory, NULL);
    device->stats.reserved_bytes -= block->size;
    *block = (Graphics_Memory_Block_t){0};
}

Graphics_Result graphics_memory_acquire(Graphics_Device_t *device,
                                        const VkMemoryRequirements *requirements,
                                        Gpu_Alloc_Type_t kind,
                                        VkBuffer buffer,
                                        VkImage image,
                                        bool dedicated,
                                        uint32_t *allocation) {
    uint32_t type = memory_type(device, requirements->memoryTypeBits, kind);
    if (type == UINT32_MAX)
        return Graphics_NO_DEVICE_MEM;
    if (device->free_allocation == 0U)
        return Graphics_CAPACITY;
    bool is_image = image != VK_NULL_HANDLE;
    dedicated = dedicated || requirements->size > GRAPHICS_MEMORY_BLOCK_SIZE;
    uint32_t block_index = GRAPHICS_MAX_MEMORY_BLOCKS;
    uint32_t previous = 0U;
    VkDeviceSize offset = 0U;
    if (!dedicated) {
        for (uint32_t i = 0U;
             i < GRAPHICS_MAX_MEMORY_BLOCKS && block_index == GRAPHICS_MAX_MEMORY_BLOCKS;
             ++i) {
            Graphics_Memory_Block_t *block = &device->memory_blocks[i];
            if (block->memory != VK_NULL_HANDLE && !block->dedicated &&
                block->memory_type == type && block->image == is_image &&
                block_offset(device, i, requirements, &offset, &previous))
                block_index = i;
        }
    }
    Graphics_Result result = Graphics_NONE;
    if (block_index == GRAPHICS_MAX_MEMORY_BLOCKS) {
        /* Reclaim an empty block before growing the retained allocation set. */
        for (uint32_t i = 0U;
             i < GRAPHICS_MAX_MEMORY_BLOCKS && block_index == GRAPHICS_MAX_MEMORY_BLOCKS;
             ++i) {
            if (device->memory_blocks[i].memory != VK_NULL_HANDLE &&
                device->memory_blocks[i].live_allocations == 0U) {
                release_block(device, &device->memory_blocks[i]);
                block_index = i;
            }
        }
        for (uint32_t i = 0U;
             i < GRAPHICS_MAX_MEMORY_BLOCKS && block_index == GRAPHICS_MAX_MEMORY_BLOCKS;
             ++i) {
            if (device->memory_blocks[i].memory == VK_NULL_HANDLE)
                block_index = i;
        }
        if (block_index == GRAPHICS_MAX_MEMORY_BLOCKS)
            return Graphics_CAPACITY;
        VkMemoryDedicatedAllocateInfo dedicated_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
            .image = image,
            .buffer = buffer};
        VkMemoryAllocateFlagsInfo flags = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
                                           .pNext = dedicated ? &dedicated_info : NULL,
                                           .flags =
                                               device->enabled.buffer_device_address && !is_image
                                                   ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
                                                   : 0U};
        VkMemoryAllocateInfo info = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                     .pNext = &flags,
                                     .allocationSize = dedicated ? requirements->size
                                                                 : GRAPHICS_MEMORY_BLOCK_SIZE,
                                     .memoryTypeIndex = type};
        Graphics_Memory_Block_t *block = &device->memory_blocks[block_index];
        VkDeviceMemory memory = VK_NULL_HANDLE;
        result = graphics_vk_result(
            device, vkAllocateMemory(device->device, &info, NULL, &memory), Graphics_NO_DEVICE_MEM);
        if (result == Graphics_NONE) {
            *block = (Graphics_Memory_Block_t){.memory = memory,
                                               .size = info.allocationSize,
                                               .memory_type = type,
                                               .image = is_image,
                                               .dedicated = dedicated};
            ++device->stats.memory_allocations;
            device->stats.reserved_bytes += block->size;
            VkMemoryPropertyFlags properties =
                device->memory_properties.memoryTypes[type].propertyFlags;
            if (!is_image &&
                (properties &
                 (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                result = graphics_vk_result(
                    device,
                    vkMapMemory(device->device, memory, 0U, VK_WHOLE_SIZE, 0U, &block->mapped),
                    Graphics_NO_MEMORY);
                if (result != Graphics_NONE)
                    release_block(device, block);
            }
        }
        offset = 0U;
        previous = 0U;
    }
    if (result == Graphics_NONE) {
        uint32_t slot = device->free_allocation;
        Graphics_Allocation_t *entry = &device->allocations[slot - 1U];
        device->free_allocation = entry->next;
        Graphics_Memory_Block_t *block = &device->memory_blocks[block_index];
        uint32_t *link = previous != 0U ? &device->allocations[previous - 1U].next : &block->first;
        *entry = (Graphics_Allocation_t){offset, requirements->size, block_index + 1U, *link};
        *link = slot;
        ++block->live_allocations;
        *allocation = slot;
    }
    return result;
}

void graphics_memory_release(Graphics_Device_t *device, uint32_t allocation) {
    if (allocation > 0U && allocation <= GRAPHICS_MAX_ALLOCATIONS) {
        Graphics_Allocation_t *entry = &device->allocations[allocation - 1U];
        if (entry->block != 0U) {
            Graphics_Memory_Block_t *block = &device->memory_blocks[entry->block - 1U];
            uint32_t *link = &block->first;
            while (*link != 0U && *link != allocation)
                link = &device->allocations[*link - 1U].next;
            if (*link == allocation)
                *link = entry->next;
            --block->live_allocations;
            if (block->dedicated)
                release_block(device, block);
            *entry = (Graphics_Allocation_t){.next = device->free_allocation};
            device->free_allocation = allocation;
        }
    }
}

Graphics_Result graphics_create_buffer(Graphics_Device_t *device,
                                       const Gpu_Buffer_Desc_t *desc,
                                       Gpu_Buffer_t *out) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (desc == NULL || out == NULL || out->buffer != VK_NULL_HANDLE || desc->size == 0U ||
        desc->usage == 0U ||
        (desc->memory != GPU_MEMORY_UPLOAD && desc->memory != GPU_MEMORY_DEVICE &&
         desc->memory != GPU_MEMORY_READBACK) ||
        (desc->memory != GPU_MEMORY_DEVICE && desc->size > SIZE_MAX) ||
        ((desc->usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0U &&
         !device->enabled.buffer_device_address))
        return Graphics_ERROR_INVALID_ARGUMENT;
    Gpu_Buffer_t buffer = {0};
    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = desc->size, .usage = desc->usage};
    Graphics_Result result =
        graphics_vk_result(device,
                           vkCreateBuffer(device->device, &info, NULL, &buffer.buffer),
                           Graphics_ERROR_RESOURCE);
    if (result == Graphics_NONE) {
        VkMemoryDedicatedRequirements dedicated = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
        VkMemoryRequirements2 requirements = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
                                              .pNext = &dedicated};
        VkBufferMemoryRequirementsInfo2 query = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, .buffer = buffer.buffer};
        vkGetBufferMemoryRequirements2(device->device, &query, &requirements);
        result = graphics_memory_acquire(device,
                                         &requirements.memoryRequirements,
                                         desc->memory,
                                         buffer.buffer,
                                         VK_NULL_HANDLE,
                                         dedicated.requiresDedicatedAllocation == VK_TRUE,
                                         &buffer.allocation);
    }
    if (result == Graphics_NONE) {
        Graphics_Allocation_t *entry = &device->allocations[buffer.allocation - 1U];
        Graphics_Memory_Block_t *block = &device->memory_blocks[entry->block - 1U];
        buffer.mapped = block->mapped != NULL ? (uint8_t *)block->mapped + entry->offset : NULL;
        result = graphics_vk_result(
            device,
            vkBindBufferMemory(device->device, buffer.buffer, block->memory, entry->offset),
            Graphics_NO_DEVICE_MEM);
    }
    if (result == Graphics_NONE) {
        buffer.owner = device->device;
        buffer.size = desc->size;
        buffer.usage = desc->usage;
        if ((desc->usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0U) {
            VkBufferDeviceAddressInfo address = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer.buffer};
            buffer.address = vkGetBufferDeviceAddress(device->device, &address);
        }
        *out = buffer;
        ++device->live_objects;
    } else {
        vkDestroyBuffer(device->device, buffer.buffer, NULL);
        graphics_memory_release(device, buffer.allocation);
    }
    return result;
}

Graphics_Result graphics_destroy_buffer(Graphics_Device_t *device, Gpu_Buffer_t *buffer) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (buffer == NULL || (buffer->owner != VK_NULL_HANDLE && buffer->owner != device->device))
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (buffer->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device->device, buffer->buffer, NULL);
        graphics_memory_release(device, buffer->allocation);
        --device->live_objects;
    }
    *buffer = (Gpu_Buffer_t){0};
    return Graphics_NONE;
}

Graphics_Result graphics_write_buffer(Graphics_Device_t *device,
                                      Gpu_Buffer_t *buffer,
                                      VkDeviceSize offset,
                                      const void *data,
                                      size_t size) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (buffer == NULL || data == NULL || buffer->owner != device->device ||
        buffer->buffer == VK_NULL_HANDLE)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (offset > buffer->size || size > buffer->size - offset || offset > SIZE_MAX)
        return Graphics_OUT_OF_RANGE;
    if (buffer->mapped == NULL)
        return Graphics_NO_MAPPEABLE;
    (void)memcpy((uint8_t *)buffer->mapped + (size_t)offset, data, size);
    return Graphics_NONE;
}

uint32_t graphics_format_bytes(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_UINT:
        return 1U;
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SFLOAT:
        return 2U;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32_UINT:
        return 4U;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32_SFLOAT:
        return 8U;
    case VK_FORMAT_R32G32B32_SFLOAT:
        return 12U;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return 16U;
    default:
        return 0U;
    }
}

VkImageAspectFlags graphics_format_aspects(VkFormat format) {
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_S8_UINT:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

Graphics_Result graphics_create_texture(Graphics_Device_t *device,
                                        const Gpu_Texture_Desc_t *desc,
                                        Gpu_Texture_t *out) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (desc == NULL || out == NULL || out->image != VK_NULL_HANDLE || desc->width == 0U ||
        desc->height == 0U || desc->format == VK_FORMAT_UNDEFINED || desc->usage == 0U ||
        (desc->usage &
         ~(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
           VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0U ||
        (desc->usage &
         (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) == 0U)
        return Graphics_ERROR_INVALID_ARGUMENT;
    uint32_t mips = desc->mip_levels != 0U ? desc->mip_levels : 1U;
    uint32_t max_mips = 1U;
    uint32_t dimension = desc->width > desc->height ? desc->width : desc->height;
    while (dimension > 1U) {
        ++max_mips;
        dimension >>= 1U;
    }
    if (mips > max_mips)
        return Graphics_OUT_OF_RANGE;
    VkImageFormatProperties properties;
    VkResult supported = vkGetPhysicalDeviceImageFormatProperties(device->physical_device,
                                                                  desc->format,
                                                                  VK_IMAGE_TYPE_2D,
                                                                  VK_IMAGE_TILING_OPTIMAL,
                                                                  desc->usage,
                                                                  0U,
                                                                  &properties);
    if (supported != VK_SUCCESS)
        return graphics_vk_result(device, supported, Graphics_UNSUPPORTED);
    if (desc->width > properties.maxExtent.width || desc->height > properties.maxExtent.height ||
        mips > properties.maxMipLevels)
        return Graphics_OUT_OF_RANGE;
    Gpu_Texture_t texture = {.format = desc->format,
                             .extent = {desc->width, desc->height, 1U},
                             .usage = desc->usage,
                             .mip_levels = mips,
                             .aspects = graphics_format_aspects(desc->format)};
    VkImageCreateInfo info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                              .imageType = VK_IMAGE_TYPE_2D,
                              .format = texture.format,
                              .extent = texture.extent,
                              .mipLevels = mips,
                              .arrayLayers = 1U,
                              .samples = VK_SAMPLE_COUNT_1_BIT,
                              .tiling = VK_IMAGE_TILING_OPTIMAL,
                              .usage = desc->usage};
    Graphics_Result result =
        graphics_vk_result(device,
                           vkCreateImage(device->device, &info, NULL, &texture.image),
                           Graphics_ERROR_RESOURCE);
    if (result == Graphics_NONE) {
        VkMemoryDedicatedRequirements dedicated = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
        VkMemoryRequirements2 requirements = {.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
                                              .pNext = &dedicated};
        VkImageMemoryRequirementsInfo2 query = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, .image = texture.image};
        vkGetImageMemoryRequirements2(device->device, &query, &requirements);
        result = graphics_memory_acquire(device,
                                         &requirements.memoryRequirements,
                                         GPU_MEMORY_DEVICE,
                                         VK_NULL_HANDLE,
                                         texture.image,
                                         dedicated.requiresDedicatedAllocation == VK_TRUE,
                                         &texture.allocation);
    }
    if (result == Graphics_NONE) {
        Graphics_Allocation_t *entry = &device->allocations[texture.allocation - 1U];
        result =
            graphics_vk_result(device,
                               vkBindImageMemory(device->device,
                                                 texture.image,
                                                 device->memory_blocks[entry->block - 1U].memory,
                                                 entry->offset),
                               Graphics_NO_DEVICE_MEM);
    }
    if (result == Graphics_NONE) {
        VkImageViewCreateInfo view = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                      .image = texture.image,
                                      .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                      .format = texture.format,
                                      .subresourceRange = {texture.aspects, 0U, mips, 0U, 1U}};
        result = graphics_vk_result(device,
                                    vkCreateImageView(device->device, &view, NULL, &texture.view),
                                    Graphics_ERROR_RESOURCE);
    }
    if (result == Graphics_NONE) {
        texture.owner = device->device;
        *out = texture;
        ++device->live_objects;
    } else {
        vkDestroyImage(device->device, texture.image, NULL);
        graphics_memory_release(device, texture.allocation);
    }
    return result;
}

Graphics_Result graphics_destroy_texture(Graphics_Device_t *device, Gpu_Texture_t *texture) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (texture == NULL || (texture->owner != VK_NULL_HANDLE && texture->owner != device->device) ||
        (texture->image != VK_NULL_HANDLE && texture->allocation == 0U))
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (texture->image != VK_NULL_HANDLE) {
        vkDestroyImageView(device->device, texture->view, NULL);
        vkDestroyImage(device->device, texture->image, NULL);
        graphics_memory_release(device, texture->allocation);
        --device->live_objects;
    }
    *texture = (Gpu_Texture_t){0};
    return Graphics_NONE;
}
