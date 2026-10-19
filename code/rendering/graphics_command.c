#include "graphics_internal.h"
#include <math.h>

static Graphics_Result validate_viewport(const Graphics_Device_t *device,
                                         const VkViewport *viewport,
                                         const VkRect2D *scissor);

bool graphics_command_recording(const Graphics_Command_t *command) {
    return command != NULL && (command->phase == GRAPHICS_COMMAND_RECORDING ||
                               command->phase == GRAPHICS_COMMAND_RENDERING);
}

Graphics_Result graphics_create_command(Graphics_Device_t *device, Graphics_Command_t **out) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (out == NULL || *out != NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    Graphics_Command_t *command = (Graphics_Command_t *)calloc(1U, sizeof(*command));
    if (command == NULL)
        return Graphics_NO_MEMORY;
    command->device = device;
    VkCommandPoolCreateInfo pool = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                    .queueFamilyIndex = device->queue_family};
    Graphics_Result result =
        graphics_vk_result(device,
                           vkCreateCommandPool(device->device, &pool, NULL, &command->pool),
                           Graphics_NO_COMMAND_BUFFER);
    if (result == Graphics_NONE) {
        VkCommandBufferAllocateInfo info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                            .commandPool = command->pool,
                                            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                            .commandBufferCount = 1U};
        result =
            graphics_vk_result(device,
                               vkAllocateCommandBuffers(device->device, &info, &command->command),
                               Graphics_NO_COMMAND_BUFFER);
    }
    if (result == Graphics_NONE) {
        VkFenceCreateInfo info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        result = graphics_vk_result(device,
                                    vkCreateFence(device->device, &info, NULL, &command->fence),
                                    Graphics_ERROR_FENCE);
    }
    if (result == Graphics_NONE) {
        *out = command;
        ++device->live_objects;
    } else {
        vkDestroyCommandPool(device->device, command->pool, NULL);
        free(command);
    }
    return result;
}

Graphics_Result graphics_poll_command(Graphics_Command_t *command) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (command->device->lost)
        return Graphics_DEVICE_LOST;
    if (command->phase != GRAPHICS_COMMAND_PENDING)
        return Graphics_NONE;
    VkResult result = vkGetFenceStatus(command->device->device, command->fence);
    if (result == VK_NOT_READY)
        return Graphics_BUSY;
    if (result == VK_SUCCESS)
        command->phase = GRAPHICS_COMMAND_IDLE;
    return graphics_vk_result(command->device, result, Graphics_ERROR_FENCE);
}

Graphics_Result graphics_wait_command(Graphics_Command_t *command) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (command->device->lost)
        return Graphics_DEVICE_LOST;
    if (command->phase != GRAPHICS_COMMAND_PENDING)
        return Graphics_NONE;
    Graphics_Result result = graphics_vk_result(
        command->device,
        vkWaitForFences(command->device->device, 1U, &command->fence, VK_TRUE, UINT64_MAX),
        Graphics_ERROR_FENCE);
    if (result == Graphics_NONE)
        command->phase = GRAPHICS_COMMAND_IDLE;
    return result;
}

void graphics_invalidate_state(Graphics_Command_t *command) {
    if (command != NULL) {
        command->state_valid = false;
        command->pipeline = NULL;
        memset(command->sets, 0, sizeof(command->sets));
        command->index_bytes = 0U;
        command->index_stride = 0U;
    }
}

Graphics_Result graphics_command_reset(Graphics_Command_t *command) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (command->phase == GRAPHICS_COMMAND_PENDING)
        return Graphics_BUSY;
    Graphics_Result result =
        graphics_vk_result(command->device,
                           vkResetCommandPool(command->device->device, command->pool, 0U),
                           Graphics_NO_COMMAND_BUFFER);
    command->phase = result == Graphics_NONE ? GRAPHICS_COMMAND_IDLE : GRAPHICS_COMMAND_FAILED;
    command->image_count = 0U;
    graphics_invalidate_state(command);
    return result;
}

Graphics_Result graphics_cancel_command(Graphics_Command_t *command) {
    if (command == NULL || command->frame_owned)
        return Graphics_ERROR_INVALID_ARGUMENT;
    return graphics_command_reset(command);
}

Graphics_Result graphics_command_begin(Graphics_Command_t *command) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (command->device->lost)
        return Graphics_DEVICE_LOST;
    Graphics_Result result = graphics_poll_command(command);
    if (result != Graphics_NONE)
        return result;
    if (command->phase != GRAPHICS_COMMAND_IDLE)
        return Graphics_ERROR_STATE;
    result = graphics_command_reset(command);
    if (result == Graphics_NONE) {
        VkCommandBufferBeginInfo info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                         .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        result = graphics_vk_result(command->device,
                                    vkBeginCommandBuffer(command->command, &info),
                                    Graphics_NO_COMMAND_BUFFER);
#ifdef GRAPHICS_TESTING
        if (result == Graphics_NONE && graphics_test_failure(GRAPHICS_FAIL_BEGIN))
            result = Graphics_NO_COMMAND_BUFFER;
#endif
        command->phase =
            result == Graphics_NONE ? GRAPHICS_COMMAND_RECORDING : GRAPHICS_COMMAND_FAILED;
    }
    return result;
}

Graphics_Result graphics_begin_command(Graphics_Command_t *command) {
    if (command == NULL || command->frame_owned)
        return Graphics_ERROR_INVALID_ARGUMENT;
    return graphics_command_begin(command);
}

Graphics_Result
graphics_command_submit(Graphics_Command_t *command, VkSemaphore wait, VkSemaphore signal) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (command->phase != GRAPHICS_COMMAND_RECORDING)
        return Graphics_ERROR_STATE;
    Graphics_Device_t *device = command->device;
    Graphics_Result result = graphics_vk_result(
        device, vkEndCommandBuffer(command->command), Graphics_NO_COMMAND_BUFFER);
    if (result == Graphics_NONE)
        result = graphics_vk_result(
            device, vkResetFences(device->device, 1U, &command->fence), Graphics_ERROR_FENCE);
    if (result == Graphics_NONE) {
        VkSemaphoreSubmitInfo wait_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                           .semaphore = wait,
                                           .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
        VkSemaphoreSubmitInfo signal_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                             .semaphore = signal,
                                             .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
        VkCommandBufferSubmitInfo buffer = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                            .commandBuffer = command->command};
        VkSubmitInfo2 info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                              .commandBufferInfoCount = 1U,
                              .pCommandBufferInfos = &buffer,
                              .waitSemaphoreInfoCount = wait != VK_NULL_HANDLE ? 1U : 0U,
                              .pWaitSemaphoreInfos = &wait_info,
                              .signalSemaphoreInfoCount = signal != VK_NULL_HANDLE ? 1U : 0U,
                              .pSignalSemaphoreInfos = &signal_info};
        VkResult submitted;
#ifdef GRAPHICS_TESTING
        submitted = graphics_test_failure(GRAPHICS_FAIL_SUBMIT)
                        ? VK_ERROR_UNKNOWN
                        : vkQueueSubmit2(device->queue, 1U, &info, command->fence);
#else
        submitted = vkQueueSubmit2(device->queue, 1U, &info, command->fence);
#endif
        result = graphics_vk_result(device, submitted, Graphics_ERROR_QUEUE_SUBMIT);
    }
    command->phase = result == Graphics_NONE ? GRAPHICS_COMMAND_PENDING : GRAPHICS_COMMAND_FAILED;
    if (result == Graphics_NONE) {
        for (uint32_t i = 0U; i < command->image_count; ++i)
            command->images[i].texture->layout = command->images[i].layout;
    }
    return result;
}

Graphics_Result graphics_submit_command(Graphics_Command_t *command) {
    if (command == NULL || command->frame_owned)
        return Graphics_ERROR_INVALID_ARGUMENT;
    return graphics_command_submit(command, VK_NULL_HANDLE, VK_NULL_HANDLE);
}

Graphics_Result graphics_destroy_command(Graphics_Command_t **command) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (*command == NULL)
        return Graphics_NONE;
    if ((*command)->frame_owned)
        return Graphics_ERROR_STATE;
    Graphics_Result result = graphics_wait_command(*command);
    if (result == Graphics_NONE || result == Graphics_DEVICE_LOST) {
        Graphics_Device_t *device = (*command)->device;
        vkDestroyCommandPool(device->device, (*command)->pool, NULL);
        vkDestroyFence(device->device, (*command)->fence, NULL);
        --device->live_objects;
        free(*command);
        *command = NULL;
    }
    return result;
}

VkCommandBuffer graphics_native_command(const Graphics_Command_t *command) {
    return graphics_command_recording(command) ? command->command : VK_NULL_HANDLE;
}

VkImageLayout graphics_command_image_layout(const Graphics_Command_t *command,
                                            const Gpu_Texture_t *texture) {
    for (uint32_t i = 0U; i < command->image_count; ++i)
        if (command->images[i].texture == texture)
            return command->images[i].layout;
    return texture->layout;
}

Graphics_Result
graphics_track_image(Graphics_Command_t *command, Gpu_Texture_t *texture, VkImageLayout layout) {
    uint32_t index = 0U;
    while (index < command->image_count && command->images[index].texture != texture)
        ++index;
    if (index == GRAPHICS_MAX_TRACKED_IMAGES)
        return Graphics_CAPACITY;
    command->images[index] = (Graphics_Image_State_t){texture, layout};
    if (index == command->image_count)
        ++command->image_count;
    return Graphics_NONE;
}

void graphics_image_barrier(VkCommandBuffer command,
                            Gpu_Texture_t *texture,
                            VkImageLayout old_layout,
                            VkImageLayout new_layout,
                            VkPipelineStageFlags2 stages,
                            VkAccessFlags2 access) {
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = old_layout == VK_IMAGE_LAYOUT_UNDEFINED
                            ? VK_PIPELINE_STAGE_2_NONE
                            : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = old_layout == VK_IMAGE_LAYOUT_UNDEFINED
                             ? VK_ACCESS_2_NONE
                             : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = stages,
        .dstAccessMask = access,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture->image,
        .subresourceRange = {texture->aspects, 0U, texture->mip_levels, 0U, 1U}};
    VkDependencyInfo info = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                             .imageMemoryBarrierCount = 1U,
                             .pImageMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(command, &info);
}

Graphics_Result graphics_transition_texture(Graphics_Command_t *command,
                                            Gpu_Texture_t *texture,
                                            VkImageLayout layout,
                                            VkPipelineStageFlags2 stages,
                                            VkAccessFlags2 access) {
    if (command == NULL || texture == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (command->phase != GRAPHICS_COMMAND_RECORDING)
        return Graphics_ERROR_STATE;
    if (texture->owner != command->device->device || texture->image == VK_NULL_HANDLE)
        return Graphics_ERROR_INVALID_ARGUMENT;
    VkImageUsageFlags required = 0U;
    switch (layout) {
    case VK_IMAGE_LAYOUT_GENERAL:
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        required = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        required = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        required = VK_IMAGE_USAGE_SAMPLED_BIT;
        break;
    case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
        required = texture->aspects == VK_IMAGE_ASPECT_COLOR_BIT
                       ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                       : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        if (texture->allocation != 0U)
            return Graphics_ERROR_INVALID_ARGUMENT;
        break;
    default:
        return Graphics_UNSUPPORTED;
    }
    if ((texture->usage & required) != required)
        return Graphics_ERROR_INVALID_ARGUMENT;
    VkImageLayout old_layout = graphics_command_image_layout(command, texture);
    Graphics_Result result = graphics_track_image(command, texture, layout);
    if (result == Graphics_NONE)
        graphics_image_barrier(command->command, texture, old_layout, layout, stages, access);
    return result;
}

Graphics_Result graphics_copy_buffer(Graphics_Command_t *command,
                                     const Gpu_Buffer_t *src,
                                     VkDeviceSize src_offset,
                                     const Gpu_Buffer_t *dst,
                                     VkDeviceSize dst_offset,
                                     VkDeviceSize size) {
    if (command == NULL || src == NULL || dst == NULL || size == 0U)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (command->phase != GRAPHICS_COMMAND_RECORDING)
        return Graphics_ERROR_STATE;
    if (src->owner != command->device->device || dst->owner != command->device->device ||
        (src->usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0U ||
        (dst->usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0U)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (src_offset > src->size || size > src->size - src_offset || dst_offset > dst->size ||
        size > dst->size - dst_offset)
        return Graphics_OUT_OF_RANGE;
    if (src->buffer == dst->buffer && src_offset < dst_offset + size &&
        dst_offset < src_offset + size)
        return Graphics_ERROR_INVALID_ARGUMENT;
    VkBufferMemoryBarrier2 barriers[2] = {
        {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
         .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
         .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
         .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
         .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .buffer = src->buffer,
         .offset = src_offset,
         .size = size},
        {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
         .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
         .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
         .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
         .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .buffer = dst->buffer,
         .offset = dst_offset,
         .size = size}};
    VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .bufferMemoryBarrierCount = 2U,
                                   .pBufferMemoryBarriers = barriers};
    vkCmdPipelineBarrier2(command->command, &dependency);
    VkBufferCopy region = {src_offset, dst_offset, size};
    vkCmdCopyBuffer(command->command, src->buffer, dst->buffer, 1U, &region);
    barriers[1].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barriers[1].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barriers[1].dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT;
    barriers[1].dstAccessMask =
        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_READ_BIT;
    dependency.bufferMemoryBarrierCount = 1U;
    dependency.pBufferMemoryBarriers = &barriers[1];
    vkCmdPipelineBarrier2(command->command, &dependency);
    return Graphics_NONE;
}

static bool attachment_valid(Graphics_Command_t *command,
                             const Graphics_Attachment_t *attachment,
                             VkRect2D area,
                             VkImageAspectFlags aspect) {
    const Gpu_Texture_t *texture = attachment->texture;
    VkImageUsageFlags usage = aspect == VK_IMAGE_ASPECT_COLOR_BIT
                                  ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                  : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    return texture != NULL && texture->owner == command->device->device &&
           texture->view != VK_NULL_HANDLE && texture->mip_levels == 1U &&
           (texture->aspects & aspect) != 0U && (texture->usage & usage) != 0U &&
           (uint32_t)area.offset.x <= texture->extent.width &&
           area.extent.width <= texture->extent.width - (uint32_t)area.offset.x &&
           (uint32_t)area.offset.y <= texture->extent.height &&
           area.extent.height <= texture->extent.height - (uint32_t)area.offset.y &&
           (attachment->load == VK_ATTACHMENT_LOAD_OP_CLEAR ||
            attachment->load == VK_ATTACHMENT_LOAD_OP_DONT_CARE ||
            (attachment->load == VK_ATTACHMENT_LOAD_OP_LOAD &&
             graphics_command_image_layout(command, texture) != VK_IMAGE_LAYOUT_UNDEFINED)) &&
           (attachment->store == VK_ATTACHMENT_STORE_OP_STORE ||
            attachment->store == VK_ATTACHMENT_STORE_OP_DONT_CARE);
}

Graphics_Result graphics_begin_render(Graphics_Command_t *command,
                                      const Graphics_Render_Desc_t *desc) {
    if (command == NULL || desc == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (command->phase != GRAPHICS_COMMAND_RECORDING)
        return Graphics_ERROR_STATE;
    if (desc->color_count > GRAPHICS_MAX_COLOR_ATTACHMENTS ||
        desc->color_count > command->device->properties.limits.maxColorAttachments ||
        (desc->color_count != 0U && desc->colors == NULL) || desc->area.offset.x < 0 ||
        desc->area.offset.y < 0 || desc->area.extent.width == 0U ||
        desc->area.extent.height == 0U ||
        (desc->color_count == 0U && desc->depth == NULL && desc->stencil == NULL))
        return Graphics_ERROR_INVALID_ARGUMENT;
    const Graphics_Attachment_t *attachments[GRAPHICS_MAX_COLOR_ATTACHMENTS + 2U];
    uint32_t count = 0U;
    for (uint32_t i = 0U; i < desc->color_count; ++i) {
        if (!attachment_valid(command, &desc->colors[i], desc->area, VK_IMAGE_ASPECT_COLOR_BIT))
            return Graphics_ERROR_INVALID_ARGUMENT;
        for (uint32_t j = 0U; j < i; ++j)
            if (desc->colors[i].texture == desc->colors[j].texture)
                return Graphics_ERROR_INVALID_ARGUMENT;
        attachments[count++] = &desc->colors[i];
    }
    if (desc->depth != NULL) {
        if (!attachment_valid(command, desc->depth, desc->area, VK_IMAGE_ASPECT_DEPTH_BIT))
            return Graphics_ERROR_INVALID_ARGUMENT;
        attachments[count++] = desc->depth;
    }
    if (desc->stencil != NULL) {
        if (!attachment_valid(command, desc->stencil, desc->area, VK_IMAGE_ASPECT_STENCIL_BIT) ||
            (desc->depth != NULL && desc->depth->texture != desc->stencil->texture))
            return Graphics_ERROR_INVALID_ARGUMENT;
        attachments[count++] = desc->stencil;
    }
    uint32_t new_images = 0U;
    for (uint32_t i = 0U; i < count; ++i) {
        bool known = false;
        for (uint32_t j = 0U; j < command->image_count; ++j)
            known = known || command->images[j].texture == attachments[i]->texture;
        for (uint32_t j = 0U; j < i; ++j)
            known = known || attachments[j]->texture == attachments[i]->texture;
        if (!known)
            ++new_images;
    }
    if (new_images > GRAPHICS_MAX_TRACKED_IMAGES - command->image_count)
        return Graphics_CAPACITY;
    VkViewport viewport = {(float)desc->area.offset.x,
                           (float)desc->area.offset.y,
                           (float)desc->area.extent.width,
                           (float)desc->area.extent.height,
                           0.0f,
                           1.0f};
    Graphics_Result valid = validate_viewport(command->device, &viewport, &desc->area);
    if (valid != Graphics_NONE)
        return valid;
    if ((uint32_t)desc->area.offset.x > command->device->properties.limits.maxFramebufferWidth ||
        desc->area.extent.width > command->device->properties.limits.maxFramebufferWidth -
                                      (uint32_t)desc->area.offset.x ||
        (uint32_t)desc->area.offset.y > command->device->properties.limits.maxFramebufferHeight ||
        desc->area.extent.height >
            command->device->properties.limits.maxFramebufferHeight - (uint32_t)desc->area.offset.y)
        return Graphics_OUT_OF_RANGE;
    VkRenderingAttachmentInfo infos[GRAPHICS_MAX_COLOR_ATTACHMENTS + 2U] = {0};
    for (uint32_t i = 0U; i < count; ++i) {
        Gpu_Texture_t *texture = attachments[i]->texture;
        bool color = i < desc->color_count;
        VkPipelineStageFlags2 stages = color ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
                                             : VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                                   VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        VkAccessFlags2 access =
            color ? VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
                  : VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if (i == 0U || attachments[i - 1U]->texture != texture) {
            Graphics_Result result = graphics_transition_texture(
                command, texture, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, stages, access);
            if (result != Graphics_NONE)
                return result;
        }
        infos[i] = (VkRenderingAttachmentInfo){.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                               .imageView = texture->view,
                                               .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                               .loadOp = attachments[i]->load,
                                               .storeOp = attachments[i]->store,
                                               .clearValue = attachments[i]->clear};
    }
    VkRenderingInfo rendering = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = desc->area,
        .layerCount = 1U,
        .colorAttachmentCount = desc->color_count,
        .pColorAttachments = infos,
        .pDepthAttachment = desc->depth != NULL ? &infos[desc->color_count] : NULL,
        .pStencilAttachment = desc->stencil != NULL ? &infos[count - 1U] : NULL};
    vkCmdBeginRendering(command->command, &rendering);
    command->phase = GRAPHICS_COMMAND_RENDERING;
    command->color_count = desc->color_count;
    for (uint32_t i = 0U; i < desc->color_count; ++i)
        command->colors[i] = desc->colors[i].texture->format;
    command->depth_format =
        desc->depth != NULL ? desc->depth->texture->format : VK_FORMAT_UNDEFINED;
    command->stencil_format =
        desc->stencil != NULL ? desc->stencil->texture->format : VK_FORMAT_UNDEFINED;
    return graphics_set_viewport(command, &viewport, &desc->area);
}

Graphics_Result graphics_end_render(Graphics_Command_t *command) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (command->phase != GRAPHICS_COMMAND_RENDERING)
        return Graphics_ERROR_STATE;
    vkCmdEndRendering(command->command);
    command->phase = GRAPHICS_COMMAND_RECORDING;
    return Graphics_NONE;
}

static Graphics_Result validate_viewport(const Graphics_Device_t *device,
                                         const VkViewport *viewport,
                                         const VkRect2D *scissor) {
    const VkPhysicalDeviceLimits *limits = &device->properties.limits;
    if (!isfinite(viewport->x) || !isfinite(viewport->y) || !isfinite(viewport->width) ||
        !isfinite(viewport->height) || !isfinite(viewport->minDepth) ||
        !isfinite(viewport->maxDepth) || viewport->width <= 0.0f || viewport->height <= 0.0f ||
        viewport->width > (float)limits->maxViewportDimensions[0] ||
        viewport->height > (float)limits->maxViewportDimensions[1] ||
        viewport->x < limits->viewportBoundsRange[0] ||
        viewport->y < limits->viewportBoundsRange[0] ||
        viewport->x + viewport->width > limits->viewportBoundsRange[1] ||
        viewport->y + viewport->height > limits->viewportBoundsRange[1] ||
        viewport->minDepth < 0.0f || viewport->minDepth > 1.0f || viewport->maxDepth < 0.0f ||
        viewport->maxDepth > 1.0f || scissor->offset.x < 0 || scissor->offset.y < 0 ||
        scissor->extent.width > INT32_MAX - (uint32_t)scissor->offset.x ||
        scissor->extent.height > INT32_MAX - (uint32_t)scissor->offset.y)
        return Graphics_OUT_OF_RANGE;
    return Graphics_NONE;
}

Graphics_Result graphics_set_viewport(Graphics_Command_t *command,
                                      const VkViewport *viewport,
                                      const VkRect2D *scissor) {
    if (command == NULL || viewport == NULL || scissor == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!graphics_command_recording(command))
        return Graphics_ERROR_STATE;
    Graphics_Result result = validate_viewport(command->device, viewport, scissor);
    if (result != Graphics_NONE)
        return result;
    vkCmdSetViewportWithCount(command->command, 1U, viewport);
    vkCmdSetScissorWithCount(command->command, 1U, scissor);
    return Graphics_NONE;
}
