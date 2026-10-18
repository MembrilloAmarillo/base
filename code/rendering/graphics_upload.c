#include "graphics_internal.h"

static Graphics_Result batch_valid(Graphics_Device_t *device, const Gpu_Upload_Batch_t *batch) {
    Graphics_Result result = Graphics_NONE;
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        result = Graphics_NO_DEVICE;
    } else if (batch == NULL || batch->owner != device->device || batch->command == VK_NULL_HANDLE) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    }
    return result;
}

static Graphics_Result submit_command(Graphics_Device_t *device, VkCommandBuffer command, VkFence fence) {
    VkQueue queue;
    vkGetDeviceQueue(device->device, device->queue_family, 0U, &queue);
    VkCommandBufferSubmitInfo command_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = command,
    };
    VkSubmitInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1U, .pCommandBufferInfos = &command_info,
    };
    return vkQueueSubmit2(queue, 1U, &info, fence) == VK_SUCCESS ? Graphics_NONE : Graphics_ERROR_QUEUE_SUBMIT;
}

Graphics_Result graphics_submit_and_wait(Graphics_Device_t *device, VkCommandBuffer command) {
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    Graphics_Result result = Graphics_NONE;
    if (vkCreateFence(device->device, &info, NULL, &fence) != VK_SUCCESS) {
        result = Graphics_NO_FENCE;
    } else {
        result = submit_command(device, command, fence);
        if (result == Graphics_NONE && vkWaitForFences(device->device, 1U, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            /* Exceptional recovery only; normal uploads never idle the queue. */
            (void)vkDeviceWaitIdle(device->device);
            result = Graphics_ERROR_FENCE;
        }
        vkDestroyFence(device->device, fence, NULL);
    }
    return result;
}

Graphics_Result graphics_create_upload_batch(Graphics_Device_t *device, VkDeviceSize capacity, Gpu_Upload_Batch_t *out) {
    Graphics_Result result = Graphics_NONE;
    Gpu_Upload_Batch_t batch = {0};
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        result = Graphics_NO_DEVICE;
    } else if (out == NULL || out->owner != VK_NULL_HANDLE || capacity == 0U || capacity > SIZE_MAX) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    }
    if (result == Graphics_NONE) {
        Gpu_Buffer_Desc_t desc = {capacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, CPU_VISIBLE};
        result = graphics_create_buffer(device, &desc, &batch.staging);
    }
    if (result == Graphics_NONE) {
        VkCommandPoolCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = device->queue_family,
        };
        if (vkCreateCommandPool(device->device, &info, NULL, &batch.pool) != VK_SUCCESS) {
            batch.pool = VK_NULL_HANDLE;
            result = Graphics_NO_COMMAND_BUFFER;
        }
    }
    if (result == Graphics_NONE) {
        result = graphics_single_time_command_buffer(device, batch.pool, &batch.command);
    }
    if (result == Graphics_NONE) {
        VkFenceCreateInfo info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if (vkCreateFence(device->device, &info, NULL, &batch.fence) != VK_SUCCESS) {
            batch.fence = VK_NULL_HANDLE;
            result = Graphics_NO_FENCE;
        }
    }
    if (result == Graphics_NONE) {
        batch.owner = device->device;
        *out = batch;
    } else if (device != NULL && device->device != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device->device, batch.pool, NULL);
        (void)graphics_destroy_buffer(device, &batch.staging);
    }
    return result;
}

Graphics_Result graphics_begin_uploads(Graphics_Device_t *device, Gpu_Upload_Batch_t *batch) {
    Graphics_Result result = batch_valid(device, batch);
    if (result == Graphics_NONE) {
        if (batch->pending || batch->recording) {
            result = Graphics_ERROR_RESOURCE;
        } else if (vkResetCommandPool(device->device, batch->pool, 0U) != VK_SUCCESS) {
            result = Graphics_NO_COMMAND_BUFFER;
        } else {
            result = graphics_begin_command_buffer(batch->command);
            if (result == Graphics_NONE) {
                batch->recording = true;
                batch->used = 0U;
                batch->texture_count = 0U;
            }
        }
    }
    return result;
}

static Graphics_Result staging_offset(const Gpu_Upload_Batch_t *batch, size_t size, VkDeviceSize *offset) {
    Graphics_Result result = Graphics_NONE;
    VkDeviceSize padding = (4U - batch->used % 4U) % 4U;
    if (!batch->recording || batch->pending) {
        result = Graphics_ERROR_RESOURCE;
    } else if (batch->used > batch->staging.size || padding > batch->staging.size - batch->used ||
               size > batch->staging.size - batch->used - padding) {
        result = Graphics_OUT_OF_RANGE;
    } else {
        *offset = batch->used + padding;
    }
    return result;
}

Graphics_Result graphics_upload_texture(Graphics_Device_t *device, Gpu_Upload_Batch_t *batch,
    Gpu_Texture_t *texture, const void *data, size_t size) {
    Graphics_Result result = batch_valid(device, batch);
    VkDeviceSize offset = 0U;
    uint32_t index = 0U;
    if (result == Graphics_NONE) {
        if (texture == NULL || data == NULL || texture->owner != device->device || texture->image == VK_NULL_HANDLE ||
            (texture->usage & (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) !=
                (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT) ||
            texture->extent.width == 0U || texture->extent.height == 0U ||
            texture->extent.depth != 1U || texture->mip_levels != 1U || texture->array_layers != 1U ||
            (texture->layout != VK_IMAGE_LAYOUT_UNDEFINED && texture->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
            result = Graphics_ERROR_INVALID_ARGUMENT;
        } else {
            uint32_t bytes = graphics_format_bytes(texture->format);
            size_t width = texture->extent.width;
            size_t height = texture->extent.height;
            if (bytes == 0U || width > SIZE_MAX / bytes || height > SIZE_MAX / (width * bytes) ||
                size != width * height * bytes) {
                result = Graphics_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    if (result == Graphics_NONE) {
        result = staging_offset(batch, size, &offset);
        while (index < batch->texture_count && batch->textures[index] != texture) {
            ++index;
        }
        if (index == GRAPHICS_MAX_UPLOAD_TEXTURES) {
            result = Graphics_OUT_OF_RANGE;
        }
    }
    if (result == Graphics_NONE) {
        VkImageLayout old_layout = index < batch->texture_count ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : texture->layout;
        (void)memcpy((uint8_t *)batch->staging.mapped + offset, data, size);
        graphics_issue_image_barrier(batch->command, texture->image, texture->barrier_aspects,
            old_layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            old_layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_ACCESS_2_NONE : VK_ACCESS_2_MEMORY_READ_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            old_layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy copy = {
            .bufferOffset = offset,
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 0U, 1U},
            .imageExtent = texture->extent,
        };
        vkCmdCopyBufferToImage(batch->command, batch->staging.buffer, texture->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U, &copy);
        graphics_issue_image_barrier(batch->command, texture->image, texture->barrier_aspects,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (index == batch->texture_count) {
            batch->textures[batch->texture_count++] = texture;
        }
        batch->used = offset + size;
    }
    return result;
}

Graphics_Result graphics_upload_buffer(Graphics_Device_t *device, Gpu_Upload_Batch_t *batch,
    Gpu_Buffer_t *buffer, const void *data, Gpu_Range_t range) {
    Graphics_Result result = batch_valid(device, batch);
    VkDeviceSize offset = 0U;
    if (result == Graphics_NONE) {
        if (buffer == NULL || data == NULL || buffer->owner != device->device || buffer->buffer == VK_NULL_HANDLE ||
            buffer->buffer == batch->staging.buffer || (buffer->usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0U || range.size == 0U) {
            result = Graphics_ERROR_INVALID_ARGUMENT;
        } else if (range.offset > buffer->size || range.size > buffer->size - range.offset) {
            result = Graphics_OUT_OF_RANGE;
        } else {
            result = staging_offset(batch, range.size, &offset);
        }
    }
    if (result == Graphics_NONE) {
        (void)memcpy((uint8_t *)batch->staging.mapped + offset, data, range.size);
        graphics_issue_memory_barrier(batch->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        VkBufferCopy copy = {offset, range.offset, range.size};
        vkCmdCopyBuffer(batch->command, batch->staging.buffer, buffer->buffer, 1U, &copy);
        graphics_issue_memory_barrier(batch->command, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
            VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_READ_BIT);
        batch->used = offset + range.size;
    }
    return result;
}

Graphics_Result graphics_submit_uploads(Graphics_Device_t *device, Gpu_Upload_Batch_t *batch) {
    Graphics_Result result = batch_valid(device, batch);
    if (result == Graphics_NONE && (!batch->recording || batch->pending || batch->used == 0U)) {
        result = Graphics_ERROR_RESOURCE;
    }
    if (result == Graphics_NONE) {
        result = graphics_end_command_buffer(batch->command);
        batch->recording = false;
        if (result == Graphics_NONE && vkResetFences(device->device, 1U, &batch->fence) != VK_SUCCESS) {
            result = Graphics_ERROR_FENCE;
        }
        if (result == Graphics_NONE) {
            result = submit_command(device, batch->command, batch->fence);
        }
        if (result == Graphics_NONE) {
            batch->pending = true;
            for (uint32_t i = 0U; i < batch->texture_count; ++i) {
                batch->textures[i]->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            ++device->stats.upload_submissions;
            device->stats.uploaded_bytes += batch->used;
        }
    }
    return result;
}

Graphics_Result graphics_wait_uploads(Graphics_Device_t *device, Gpu_Upload_Batch_t *batch) {
    Graphics_Result result = batch_valid(device, batch);
    if (result == Graphics_NONE && batch->pending) {
        if (vkWaitForFences(device->device, 1U, &batch->fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            result = Graphics_ERROR_FENCE;
        } else {
            batch->pending = false;
        }
    }
    return result;
}

Graphics_Result graphics_cancel_uploads(Graphics_Device_t *device, Gpu_Upload_Batch_t *batch) {
    Graphics_Result result = batch_valid(device, batch);
    if (result == Graphics_NONE) {
        if (batch->pending) {
            result = Graphics_ERROR_RESOURCE;
        } else if (vkResetCommandPool(device->device, batch->pool, 0U) != VK_SUCCESS) {
            result = Graphics_NO_COMMAND_BUFFER;
        } else {
            batch->recording = false;
            batch->used = 0U;
            batch->texture_count = 0U;
        }
    }
    return result;
}

Graphics_Result graphics_destroy_upload_batch(Graphics_Device_t *device, Gpu_Upload_Batch_t *batch) {
    Graphics_Result result = Graphics_NONE;
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        result = Graphics_NO_DEVICE;
    } else if (batch == NULL || (batch->owner != VK_NULL_HANDLE && batch->owner != device->device)) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    } else if (batch->owner != VK_NULL_HANDLE) {
        result = graphics_wait_uploads(device, batch);
        if (result == Graphics_NONE) {
            vkDestroyCommandPool(device->device, batch->pool, NULL);
            vkDestroyFence(device->device, batch->fence, NULL);
            result = graphics_destroy_buffer(device, &batch->staging);
            *batch = (Gpu_Upload_Batch_t){0};
        }
    }
    return result;
}
