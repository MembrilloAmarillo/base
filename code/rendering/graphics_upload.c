#include "graphics_internal.h"

Graphics_Result graphics_create_upload_batch(Graphics_Device_t *device,
                                             VkDeviceSize capacity,
                                             Gpu_Upload_Batch_t **out) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (out == NULL || *out != NULL || capacity == 0U || capacity > SIZE_MAX)
        return Graphics_ERROR_INVALID_ARGUMENT;
    Gpu_Upload_Batch_t *batch = (Gpu_Upload_Batch_t *)calloc(1U, sizeof(*batch));
    if (batch == NULL)
        return Graphics_NO_MEMORY;
    batch->device = device;
    Gpu_Buffer_Desc_t desc = {capacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, GPU_MEMORY_UPLOAD};
    Graphics_Result result = graphics_create_buffer(device, &desc, &batch->staging);
    if (result == Graphics_NONE)
        result = graphics_create_command(device, &batch->command);
    if (result == Graphics_NONE) {
        *out = batch;
        ++device->live_objects;
    } else {
        (void)graphics_destroy_buffer(device, &batch->staging);
        free(batch);
    }
    return result;
}

Graphics_Result graphics_poll_uploads(Gpu_Upload_Batch_t *batch) {
    return batch != NULL ? graphics_poll_command(batch->command) : Graphics_ERROR_INVALID_ARGUMENT;
}

Graphics_Result graphics_wait_uploads(Gpu_Upload_Batch_t *batch) {
    return batch != NULL ? graphics_wait_command(batch->command) : Graphics_ERROR_INVALID_ARGUMENT;
}

Graphics_Result graphics_begin_uploads(Gpu_Upload_Batch_t *batch) {
    if (batch == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (batch->recording)
        return Graphics_ERROR_STATE;
    Graphics_Result result = graphics_poll_command(batch->command);
    if (result == Graphics_NONE && batch->command->phase != GRAPHICS_COMMAND_IDLE)
        result = Graphics_ERROR_STATE;
    if (result == Graphics_NONE) {
        batch->recording = true;
        batch->used = 0U;
        batch->count = 0U;
    }
    return result;
}

static Graphics_Result staging_offset(const Gpu_Upload_Batch_t *batch,
                                      size_t size,
                                      uint32_t alignment,
                                      VkDeviceSize *offset) {
    if (!batch->recording)
        return Graphics_ERROR_STATE;
    if (batch->count == GRAPHICS_MAX_UPLOADS)
        return Graphics_CAPACITY;
    VkDeviceSize padding = (alignment - batch->used % alignment) % alignment;
    if (batch->used > batch->staging.size || padding > batch->staging.size - batch->used ||
        size > batch->staging.size - batch->used - padding)
        return Graphics_OUT_OF_RANGE;
    *offset = batch->used + padding;
    return Graphics_NONE;
}

Graphics_Result graphics_upload_buffer(Gpu_Upload_Batch_t *batch,
                                       Gpu_Buffer_t *buffer,
                                       VkDeviceSize offset,
                                       const void *data,
                                       size_t size,
                                       VkPipelineStageFlags2 consumer_stages,
                                       VkAccessFlags2 consumer_access) {
    if (batch == NULL || buffer == NULL || data == NULL || size == 0U || consumer_stages == 0U ||
        buffer->owner != batch->device->device || buffer->buffer == VK_NULL_HANDLE ||
        buffer == &batch->staging || (buffer->usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0U)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (offset > buffer->size || size > buffer->size - offset)
        return Graphics_OUT_OF_RANGE;
    VkDeviceSize staging = 0U;
    Graphics_Result result = staging_offset(batch, size, 4U, &staging);
    if (result == Graphics_NONE) {
        memcpy((uint8_t *)batch->staging.mapped + staging, data, size);
        batch->records[batch->count++] = (Graphics_Upload_Record_t){.buffer = buffer,
                                                                    .staging_offset = staging,
                                                                    .offset = offset,
                                                                    .size = size,
                                                                    .stages = consumer_stages,
                                                                    .access = consumer_access};
        batch->used = staging + size;
    }
    return result;
}

Graphics_Result graphics_upload_texture(Gpu_Upload_Batch_t *batch,
                                        Gpu_Texture_t *texture,
                                        const Gpu_Texture_Upload_t *region,
                                        const void *data,
                                        size_t size) {
    if (batch == NULL || texture == NULL || data == NULL ||
        texture->owner != batch->device->device || texture->image == VK_NULL_HANDLE ||
        texture->aspects != VK_IMAGE_ASPECT_COLOR_BIT ||
        (texture->usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0U)
        return Graphics_ERROR_INVALID_ARGUMENT;
    Gpu_Texture_Upload_t upload = {.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                   .consumer_stages = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                                                      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                   .consumer_access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT};
    if (region != NULL)
        upload = *region;
    if (upload.mip_level >= texture->mip_levels || upload.offset.x < 0 || upload.offset.y < 0 ||
        upload.consumer_stages == 0U ||
        (upload.final_layout != VK_IMAGE_LAYOUT_GENERAL &&
         upload.final_layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) ||
        (upload.final_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
         (texture->usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0U))
        return Graphics_ERROR_INVALID_ARGUMENT;
    uint32_t width = texture->extent.width >> upload.mip_level;
    uint32_t height = texture->extent.height >> upload.mip_level;
    if (width == 0U)
        width = 1U;
    if (height == 0U)
        height = 1U;
    if (upload.extent.width == 0U && upload.extent.height == 0U)
        upload.extent = (VkExtent2D){width, height};
    if ((uint32_t)upload.offset.x > width ||
        upload.extent.width > width - (uint32_t)upload.offset.x ||
        (uint32_t)upload.offset.y > height ||
        upload.extent.height > height - (uint32_t)upload.offset.y || upload.extent.width == 0U ||
        upload.extent.height == 0U)
        return Graphics_OUT_OF_RANGE;
    uint32_t bytes = graphics_format_bytes(texture->format);
    if (bytes == 0U)
        return Graphics_UNSUPPORTED;
    if (upload.extent.width > SIZE_MAX / bytes ||
        upload.extent.height > SIZE_MAX / ((size_t)upload.extent.width * bytes) ||
        size != (size_t)upload.extent.width * upload.extent.height * bytes)
        return Graphics_ERROR_INVALID_ARGUMENT;
    for (uint32_t i = 0U; i < batch->count; ++i) {
        if (batch->records[i].texture == texture &&
            batch->records[i].region.final_layout != upload.final_layout)
            return Graphics_ERROR_INVALID_ARGUMENT;
    }
    VkDeviceSize staging = 0U;
    Graphics_Result result = staging_offset(batch, size, bytes > 4U ? bytes : 4U, &staging);
    if (result == Graphics_NONE) {
        memcpy((uint8_t *)batch->staging.mapped + staging, data, size);
        batch->records[batch->count++] =
            (Graphics_Upload_Record_t){.texture = texture,
                                       .staging_offset = staging,
                                       .size = size,
                                       .region = upload,
                                       .stages = upload.consumer_stages,
                                       .access = upload.consumer_access};
        batch->used = staging + size;
    }
    return result;
}

static bool overlaps(const Graphics_Upload_Record_t *a, const Graphics_Upload_Record_t *b) {
    if (a->buffer != NULL && a->buffer == b->buffer)
        return a->offset < b->offset + b->size && b->offset < a->offset + a->size;
    if (a->texture != NULL && a->texture == b->texture &&
        a->region.mip_level == b->region.mip_level) {
        return (uint32_t)a->region.offset.x <
                   (uint32_t)b->region.offset.x + b->region.extent.width &&
               (uint32_t)b->region.offset.x <
                   (uint32_t)a->region.offset.x + a->region.extent.width &&
               (uint32_t)a->region.offset.y <
                   (uint32_t)b->region.offset.y + b->region.extent.height &&
               (uint32_t)b->region.offset.y <
                   (uint32_t)a->region.offset.y + a->region.extent.height;
    }
    return false;
}

Graphics_Result graphics_submit_uploads(Gpu_Upload_Batch_t *batch) {
    if (batch == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!batch->recording || batch->count == 0U)
        return Graphics_ERROR_STATE;
    Graphics_Command_t *command = batch->command;
    Graphics_Result result = graphics_begin_command(command);
    if (result != Graphics_NONE)
        return result;
#ifdef GRAPHICS_BENCHMARK
    if (batch->profile_queries != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(command->command, batch->profile_queries, 0U, 2U);
        vkCmdWriteTimestamp2(
            command->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, batch->profile_queries, 0U);
    }
#endif
    VkBufferMemoryBarrier2 *buffers = batch->buffer_barriers;
    VkImageMemoryBarrier2 *images = batch->image_barriers;
    Gpu_Texture_t *textures[GRAPHICS_MAX_UPLOADS] = {0};
    VkImageLayout final_layouts[GRAPHICS_MAX_UPLOADS] = {0};
    VkPipelineStageFlags2 image_stages[GRAPHICS_MAX_UPLOADS] = {0};
    VkAccessFlags2 image_access[GRAPHICS_MAX_UPLOADS] = {0};
    uint32_t heads[GRAPHICS_MAX_UPLOADS * 2U] = {0};
    uint32_t previous[GRAPHICS_MAX_UPLOADS] = {0};
    uint32_t image_index[GRAPHICS_MAX_UPLOADS] = {0};
    uint32_t buffer_count = 0U, image_count = 0U;
    for (uint32_t i = 0U; i < batch->count; ++i) {
        Graphics_Upload_Record_t *record = &batch->records[i];
        uintptr_t key =
            (uintptr_t)(record->buffer != NULL ? (void *)record->buffer : (void *)record->texture);
        uint32_t slot = (uint32_t)((key >> 4U) ^ (key >> 12U)) % (GRAPHICS_MAX_UPLOADS * 2U);
        /* At most 128 keys in 256 slots. Only uploads of the same resource
         * need overlap checks; independent uploads take expected linear work. */
        bool found = false;
        while (!found) {
            uint32_t head = heads[slot];
            found = head == 0U || (batch->records[head - 1U].buffer == record->buffer &&
                                   batch->records[head - 1U].texture == record->texture);
            if (!found)
                slot = (slot + 1U) % (GRAPHICS_MAX_UPLOADS * 2U);
        }
        previous[i] = heads[slot];
        heads[slot] = i + 1U;
        if (record->buffer != NULL) {
            buffers[buffer_count++] = (VkBufferMemoryBarrier2){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = record->buffer->buffer,
                .offset = record->offset,
                .size = record->size};
        } else {
            uint32_t index = previous[i] != 0U ? image_index[previous[i] - 1U] : image_count;
            image_index[i] = index;
            if (index == image_count) {
                Gpu_Texture_t *texture = record->texture;
                textures[index] = texture;
                final_layouts[index] = record->region.final_layout;
                images[index] = (VkImageMemoryBarrier2){
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask = texture->layout == VK_IMAGE_LAYOUT_UNDEFINED
                                        ? VK_PIPELINE_STAGE_2_NONE
                                        : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .srcAccessMask =
                        texture->layout == VK_IMAGE_LAYOUT_UNDEFINED
                            ? VK_ACCESS_2_NONE
                            : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .oldLayout = texture->layout,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = texture->image,
                    .subresourceRange = {
                        VK_IMAGE_ASPECT_COLOR_BIT, 0U, texture->mip_levels, 0U, 1U}};
                ++image_count;
            }
            image_stages[index] |= record->stages;
            image_access[index] |= record->access;
        }
    }
    VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .bufferMemoryBarrierCount = buffer_count,
                                   .pBufferMemoryBarriers = buffers,
                                   .imageMemoryBarrierCount = image_count,
                                   .pImageMemoryBarriers = images};
    vkCmdPipelineBarrier2(command->command, &dependency);
    ++batch->device->stats.upload_barrier_calls;
    uint64_t bytes = 0U;
    for (uint32_t i = 0U; i < batch->count; ++i) {
        Graphics_Upload_Record_t *record = &batch->records[i];
        bool hazard = false;
        for (uint32_t j = previous[i]; j != 0U && !hazard; j = previous[j - 1U])
            hazard = overlaps(record, &batch->records[j - 1U]);
        if (hazard) {
            VkMemoryBarrier2 barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT};
            VkDependencyInfo ordered = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                        .memoryBarrierCount = 1U,
                                        .pMemoryBarriers = &barrier};
            vkCmdPipelineBarrier2(command->command, &ordered);
            ++batch->device->stats.upload_barrier_calls;
        }
        if (record->buffer != NULL) {
            VkBufferCopy copy = {record->staging_offset, record->offset, record->size};
            vkCmdCopyBuffer(
                command->command, batch->staging.buffer, record->buffer->buffer, 1U, &copy);
        } else {
            VkBufferImageCopy copy = {
                .bufferOffset = record->staging_offset,
                .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, record->region.mip_level, 0U, 1U},
                .imageOffset = {record->region.offset.x, record->region.offset.y, 0},
                .imageExtent = {record->region.extent.width, record->region.extent.height, 1U}};
            vkCmdCopyBufferToImage(command->command,
                                   batch->staging.buffer,
                                   record->texture->image,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1U,
                                   &copy);
        }
        bytes += record->size;
    }
    uint32_t buffer_index = 0U;
    for (uint32_t i = 0U; i < batch->count; ++i) {
        Graphics_Upload_Record_t *record = &batch->records[i];
        if (record->buffer != NULL) {
            buffers[buffer_index].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            buffers[buffer_index].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            buffers[buffer_index].dstStageMask = record->stages;
            buffers[buffer_index].dstAccessMask = record->access;
            ++buffer_index;
        }
    }
    for (uint32_t i = 0U; i < image_count; ++i) {
        images[i].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        images[i].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        images[i].dstStageMask = image_stages[i];
        images[i].dstAccessMask = image_access[i];
        images[i].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        images[i].newLayout = final_layouts[i];
        result = graphics_track_image(command, textures[i], final_layouts[i]);
        if (result != Graphics_NONE)
            break;
    }
    if (result == Graphics_NONE) {
        vkCmdPipelineBarrier2(command->command, &dependency);
        ++batch->device->stats.upload_barrier_calls;
#ifdef GRAPHICS_BENCHMARK
        if (batch->profile_queries != VK_NULL_HANDLE)
            vkCmdWriteTimestamp2(
                command->command, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, batch->profile_queries, 1U);
#endif
        result = graphics_submit_command(command);
    }
    batch->recording = false;
    if (result == Graphics_NONE) {
        ++batch->device->stats.upload_submissions;
        batch->device->stats.uploaded_bytes += bytes;
    }
    return result;
}

Graphics_Result graphics_cancel_uploads(Gpu_Upload_Batch_t *batch) {
    if (batch == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    Graphics_Result result = graphics_cancel_command(batch->command);
    if (result == Graphics_NONE) {
        batch->recording = false;
        batch->used = 0U;
        batch->count = 0U;
    }
    return result;
}

Graphics_Result graphics_destroy_upload_batch(Gpu_Upload_Batch_t **batch) {
    if (batch == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (*batch == NULL)
        return Graphics_NONE;
    Graphics_Result result = graphics_destroy_command(&(*batch)->command);
    if (result == Graphics_NONE || result == Graphics_DEVICE_LOST) {
        (void)graphics_destroy_buffer((*batch)->device, &(*batch)->staging);
        --(*batch)->device->live_objects;
        free(*batch);
        *batch = NULL;
    }
    return result;
}

Graphics_Result graphics_upload_texture_wait(Graphics_Device_t *device,
                                             Gpu_Texture_t *texture,
                                             const void *data,
                                             size_t size) {
    Gpu_Upload_Batch_t *batch = NULL;
    Graphics_Result result = graphics_create_upload_batch(device, size, &batch);
    if (result == Graphics_NONE)
        result = graphics_begin_uploads(batch);
    if (result == Graphics_NONE)
        result = graphics_upload_texture(batch, texture, NULL, data, size);
    if (result == Graphics_NONE)
        result = graphics_submit_uploads(batch);
    Graphics_Result cleanup = graphics_destroy_upload_batch(&batch);
    return result != Graphics_NONE ? result : cleanup;
}
