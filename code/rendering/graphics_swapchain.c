#include "graphics_internal.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

static void destroy_images(Graphics_Device_t *device, Graphics_Swapchain_Images_t *images) {
    /* Iterate initialized slots, never an unchecked driver-reported count. */
    for (uint32_t i = 0U; i < GRAPHICS_MAX_SWAPCHAIN_IMAGES; ++i) {
        vkDestroySemaphore(device->device, images->complete[i], NULL);
        vkDestroyImageView(device->device, images->images[i].view, NULL);
    }
    vkDestroySwapchainKHR(device->device, images->handle, NULL);
    *images = (Graphics_Swapchain_Images_t){0};
}

static Graphics_Result
build_images(Graphics_Swapchain_t *swapchain, Graphics_Swapchain_Images_t *out, bool *retired) {
    Graphics_Device_t *device = swapchain->device;
    int width = 0, height = 0;
    if (!SDL_GetWindowSizeInPixels(swapchain->window, &width, &height))
        return Graphics_NO_WINDOW;
    if (width <= 0 || height <= 0 ||
        (SDL_GetWindowFlags(swapchain->window) & SDL_WINDOW_MINIMIZED) != 0U)
        return Graphics_SWAPCHAIN_OUT_OF_DATE;
    VkSurfaceCapabilitiesKHR capabilities;
    Graphics_Result result =
        graphics_vk_result(device,
                           vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                               device->physical_device, swapchain->surface, &capabilities),
                           Graphics_NO_SWAPCHAIN);
    if (result != Graphics_NONE)
        return result;
    if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0U)
        return Graphics_UNSUPPORTED;
    VkSurfaceFormatKHR formats[GRAPHICS_MAX_ENUMERATED];
    uint32_t count = GRAPHICS_MAX_ENUMERATED;
    VkResult queried = vkGetPhysicalDeviceSurfaceFormatsKHR(
        device->physical_device, swapchain->surface, &count, formats);
    if (queried != VK_SUCCESS && queried != VK_INCOMPLETE)
        return graphics_vk_result(device, queried, Graphics_NO_SWAPCHAIN);
    if (count == 0U)
        return Graphics_UNSUPPORTED;
    VkSurfaceFormatKHR selected = formats[0];
    if (selected.format == VK_FORMAT_UNDEFINED)
        selected.format = VK_FORMAT_B8G8R8A8_SRGB;
    for (uint32_t i = 0U; i < count; ++i) {
        if ((formats[i].format == VK_FORMAT_B8G8R8A8_SRGB ||
             formats[i].format == VK_FORMAT_R8G8B8A8_SRGB) &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            selected = formats[i];
            break;
        }
    }
    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent = (VkExtent2D){(uint32_t)width, (uint32_t)height};
        if (extent.width < capabilities.minImageExtent.width)
            extent.width = capabilities.minImageExtent.width;
        if (extent.height < capabilities.minImageExtent.height)
            extent.height = capabilities.minImageExtent.height;
        if (extent.width > capabilities.maxImageExtent.width)
            extent.width = capabilities.maxImageExtent.width;
        if (extent.height > capabilities.maxImageExtent.height)
            extent.height = capabilities.maxImageExtent.height;
    }
    if (extent.width == 0U || extent.height == 0U)
        return Graphics_SWAPCHAIN_OUT_OF_DATE;
    uint32_t image_count = capabilities.minImageCount + 1U;
    if (capabilities.maxImageCount != 0U && image_count > capabilities.maxImageCount)
        image_count = capabilities.maxImageCount;
    if (image_count > GRAPHICS_MAX_SWAPCHAIN_IMAGES)
        return Graphics_CAPACITY;
    const VkCompositeAlphaFlagBitsKHR choices[] = {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                                   VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                                   VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                                                   VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    for (uint32_t i = 0U; i < 4U; ++i)
        if ((capabilities.supportedCompositeAlpha & choices[i]) != 0U) {
            alpha = choices[i];
            break;
        }
    VkSwapchainCreateInfoKHR info = {.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                     .surface = swapchain->surface,
                                     .minImageCount = image_count,
                                     .imageFormat = selected.format,
                                     .imageColorSpace = selected.colorSpace,
                                     .imageExtent = extent,
                                     .imageArrayLayers = 1U,
                                     .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                     .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                     .preTransform = capabilities.currentTransform,
                                     .compositeAlpha = alpha,
                                     .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                                     .clipped = VK_TRUE,
                                     .oldSwapchain = swapchain->images.handle};
    *retired = info.oldSwapchain != VK_NULL_HANDLE;
    result = graphics_vk_result(device,
                                vkCreateSwapchainKHR(device->device, &info, NULL, &out->handle),
                                Graphics_NO_SWAPCHAIN);
#ifdef GRAPHICS_TESTING
    if (result == Graphics_NONE && graphics_test_failure(GRAPHICS_FAIL_SWAPCHAIN))
        result = Graphics_NO_MEMORY;
#endif
    VkImage images[GRAPHICS_MAX_SWAPCHAIN_IMAGES] = {0};
    count = 0U;
    if (result == Graphics_NONE)
        result =
            graphics_vk_result(device,
                               vkGetSwapchainImagesKHR(device->device, out->handle, &count, NULL),
                               Graphics_NO_SWAPCHAIN);
    if (result == Graphics_NONE && (count == 0U || count > GRAPHICS_MAX_SWAPCHAIN_IMAGES))
        result = Graphics_CAPACITY;
    if (result == Graphics_NONE)
        result =
            graphics_vk_result(device,
                               vkGetSwapchainImagesKHR(device->device, out->handle, &count, images),
                               Graphics_NO_SWAPCHAIN);
    if (result == Graphics_NONE) {
        out->count = count;
        out->format = selected.format;
        out->extent = extent;
        for (uint32_t i = 0U; i < count && result == Graphics_NONE; ++i) {
            out->images[i] = (Gpu_Texture_t){.image = images[i],
                                             .format = selected.format,
                                             .extent = {extent.width, extent.height, 1U},
                                             .aspects = VK_IMAGE_ASPECT_COLOR_BIT,
                                             .mip_levels = 1U,
                                             .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                             .owner = device->device};
            VkImageViewCreateInfo view = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = selected.format,
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, 0U, 1U}};
            result = graphics_vk_result(
                device,
                vkCreateImageView(device->device, &view, NULL, &out->images[i].view),
                Graphics_NO_SWAPCHAIN);
#ifdef GRAPHICS_TESTING
            if (result == Graphics_NONE && graphics_test_failure(GRAPHICS_FAIL_VIEW))
                result = Graphics_NO_MEMORY;
#endif
            if (result == Graphics_NONE) {
                VkSemaphoreCreateInfo semaphore = {.sType =
                                                       VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
                result = graphics_vk_result(
                    device,
                    vkCreateSemaphore(device->device, &semaphore, NULL, &out->complete[i]),
                    Graphics_NO_SWAPCHAIN);
            }
        }
    }
    return result;
}

static void cleanup_swapchain(Graphics_Swapchain_t *swapchain) {
    Graphics_Device_t *device = swapchain->device;
    destroy_images(device, &swapchain->images);
    for (uint32_t i = 0U; i < GRAPHICS_FRAMES; ++i) {
        vkDestroySemaphore(device->device, swapchain->acquired[i], NULL);
        if (swapchain->frames[i] != NULL) {
            swapchain->frames[i]->frame_owned = false;
            (void)graphics_destroy_command(&swapchain->frames[i]);
        }
    }
    vkDestroySurfaceKHR(device->instance, swapchain->surface, NULL);
    free(swapchain);
}

/* A failed submission has not consumed the WSI acquire signal. Device idle
 * alone does not wait for an outstanding WSI acquisition; explicitly consume
 * it before destroying or recycling the semaphore and retired swapchain. */
static Graphics_Result drain_acquisitions(Graphics_Swapchain_t *swapchain) {
    Graphics_Device_t *device = swapchain->device;
    for (uint32_t i = 0U; i < GRAPHICS_FRAMES; ++i) {
        if (swapchain->acquire_pending[i]) {
            VkSemaphoreSubmitInfo wait = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                          .semaphore = swapchain->acquired[i],
                                          .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
            VkSubmitInfo2 submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                                    .waitSemaphoreInfoCount = 1U,
                                    .pWaitSemaphoreInfos = &wait};
            Graphics_Result result =
                graphics_vk_result(device,
                                   vkQueueSubmit2(device->queue, 1U, &submit, VK_NULL_HANDLE),
                                   Graphics_ERROR_QUEUE_SUBMIT);
            if (result != Graphics_NONE)
                return result;
            swapchain->acquire_pending[i] = false;
        }
    }
    return graphics_wait_idle(device);
}

Graphics_Result graphics_create_swapchain(Graphics_Device_t *device,
                                          SDL_Window *window,
                                          Graphics_Swapchain_t **out) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (out == NULL || *out != NULL || window == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!device->presentation)
        return Graphics_UNSUPPORTED;
    Graphics_Swapchain_t *swapchain = (Graphics_Swapchain_t *)calloc(1U, sizeof(*swapchain));
    if (swapchain == NULL)
        return Graphics_NO_MEMORY;
    swapchain->device = device;
    swapchain->window = window;
    Graphics_Result result =
        SDL_Vulkan_CreateSurface(window, device->instance, NULL, &swapchain->surface)
            ? Graphics_NONE
            : Graphics_NO_SURFACE;
    if (result == Graphics_NONE) {
        VkBool32 supported = VK_FALSE;
        result = graphics_vk_result(
            device,
            vkGetPhysicalDeviceSurfaceSupportKHR(
                device->physical_device, device->queue_family, swapchain->surface, &supported),
            Graphics_NO_SURFACE);
        if (result == Graphics_NONE && !supported)
            result = Graphics_UNSUPPORTED;
    }
    if (result == Graphics_NONE) {
        bool retired = false;
        result = build_images(swapchain, &swapchain->images, &retired);
    }
    for (uint32_t i = 0U; i < GRAPHICS_FRAMES && result == Graphics_NONE; ++i) {
        result = graphics_create_command(device, &swapchain->frames[i]);
        if (result == Graphics_NONE) {
            swapchain->frames[i]->frame_owned = true;
            VkSemaphoreCreateInfo info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            result = graphics_vk_result(
                device,
                vkCreateSemaphore(device->device, &info, NULL, &swapchain->acquired[i]),
                Graphics_NO_SWAPCHAIN);
        }
    }
    if (result == Graphics_NONE) {
        *out = swapchain;
        ++device->live_objects;
    } else
        cleanup_swapchain(swapchain);
    return result;
}

Graphics_Result graphics_destroy_swapchain(Graphics_Swapchain_t **swapchain) {
    if (swapchain == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (*swapchain == NULL)
        return Graphics_NONE;
    if ((*swapchain)->active && !(*swapchain)->device->lost)
        return Graphics_ERROR_STATE;
    Graphics_Device_t *device = (*swapchain)->device;
    Graphics_Result result = device->lost ? Graphics_DEVICE_LOST : drain_acquisitions(*swapchain);
    if (result == Graphics_NONE || result == Graphics_DEVICE_LOST) {
        cleanup_swapchain(*swapchain);
        --device->live_objects;
        *swapchain = NULL;
    }
    return result;
}

Graphics_Result graphics_update_swapchain(Graphics_Swapchain_t *swapchain) {
    if (swapchain == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (swapchain->active)
        return Graphics_ERROR_STATE;
    Graphics_Device_t *device = swapchain->device;
    Graphics_Result result = drain_acquisitions(swapchain);
    if (result != Graphics_NONE)
        return result;
    Graphics_Swapchain_Images_t replacement = {0};
    bool retired = false;
    result = build_images(swapchain, &replacement, &retired);
    if (retired || result == Graphics_NONE) {
        destroy_images(device, &swapchain->images);
        swapchain->needs_rebuild = true;
        /* Aborted submissions can leave acquire semaphores signaled. Replace
         * them after idle instead of reusing them or waiting on an unsignaled fence. */
        for (uint32_t i = 0U; i < GRAPHICS_FRAMES; ++i) {
            Graphics_Result reset = graphics_wait_command(swapchain->frames[i]);
            if (reset == Graphics_NONE)
                reset = graphics_command_reset(swapchain->frames[i]);
            vkDestroySemaphore(device->device, swapchain->acquired[i], NULL);
            swapchain->acquired[i] = VK_NULL_HANDLE;
            VkSemaphoreCreateInfo info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            if (reset == Graphics_NONE)
                reset = graphics_vk_result(
                    device,
                    vkCreateSemaphore(device->device, &info, NULL, &swapchain->acquired[i]),
                    Graphics_NO_SWAPCHAIN);
            if (result == Graphics_NONE)
                result = reset;
        }
    }
    if (result == Graphics_NONE) {
        swapchain->images = replacement;
        swapchain->needs_rebuild = false;
        swapchain->frame = 0U;
    } else
        destroy_images(device, &replacement);
    return result;
}

VkFormat graphics_swapchain_format(const Graphics_Swapchain_t *swapchain) {
    return swapchain != NULL ? swapchain->images.format : VK_FORMAT_UNDEFINED;
}
VkExtent2D graphics_swapchain_extent(const Graphics_Swapchain_t *swapchain) {
    return swapchain != NULL ? swapchain->images.extent : (VkExtent2D){0};
}
Gpu_Texture_t *graphics_swapchain_target(Graphics_Swapchain_t *swapchain) {
    return swapchain != NULL && swapchain->active ? &swapchain->images.images[swapchain->image]
                                                  : NULL;
}

Graphics_Result graphics_start_frame(Graphics_Swapchain_t *swapchain, Graphics_Command_t **out) {
    if (swapchain == NULL || out == NULL || *out != NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (swapchain->active)
        return Graphics_ERROR_STATE;
    if (swapchain->needs_rebuild || swapchain->images.handle == VK_NULL_HANDLE)
        return Graphics_SWAPCHAIN_OUT_OF_DATE;
    Graphics_Command_t *command = swapchain->frames[swapchain->frame];
    Graphics_Result result = graphics_wait_command(command);
    if (result == Graphics_NONE)
        result = graphics_command_begin(command);
    if (result != Graphics_NONE) {
        swapchain->needs_rebuild = true;
        return result;
    }
    VkResult acquired = vkAcquireNextImageKHR(swapchain->device->device,
                                              swapchain->images.handle,
                                              UINT64_MAX,
                                              swapchain->acquired[swapchain->frame],
                                              VK_NULL_HANDLE,
                                              &swapchain->image);
    if (acquired == VK_SUCCESS || acquired == VK_SUBOPTIMAL_KHR) {
        swapchain->active = true;
        swapchain->acquire_pending[swapchain->frame] = true;
        swapchain->needs_rebuild = acquired == VK_SUBOPTIMAL_KHR;
        *out = command;
        return Graphics_NONE;
    }
    (void)graphics_command_reset(command);
    swapchain->needs_rebuild = true;
    return acquired == VK_ERROR_OUT_OF_DATE_KHR
               ? Graphics_SWAPCHAIN_OUT_OF_DATE
               : graphics_vk_result(swapchain->device, acquired, Graphics_NO_SWAPCHAIN);
}

Graphics_Result graphics_end_frame(Graphics_Swapchain_t *swapchain) {
    if (swapchain == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!swapchain->active)
        return Graphics_ERROR_STATE;
    Graphics_Command_t *command = swapchain->frames[swapchain->frame];
    if (command->phase != GRAPHICS_COMMAND_RECORDING)
        return Graphics_ERROR_STATE;
    Graphics_Result result =
        graphics_transition_texture(command,
                                    &swapchain->images.images[swapchain->image],
                                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                    VK_PIPELINE_STAGE_2_NONE,
                                    VK_ACCESS_2_NONE);
    if (result != Graphics_NONE)
        return result;
    result = graphics_command_submit(command,
                                     swapchain->acquired[swapchain->frame],
                                     swapchain->images.complete[swapchain->image]);
    swapchain->active = false;
    if (result != Graphics_NONE) {
        swapchain->needs_rebuild = true;
        return result;
    }
    swapchain->acquire_pending[swapchain->frame] = false;
    VkPresentInfoKHR present = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                .waitSemaphoreCount = 1U,
                                .pWaitSemaphores = &swapchain->images.complete[swapchain->image],
                                .swapchainCount = 1U,
                                .pSwapchains = &swapchain->images.handle,
                                .pImageIndices = &swapchain->image};
    VkResult presented = vkQueuePresentKHR(swapchain->device->queue, &present);
    swapchain->frame = (swapchain->frame + 1U) % GRAPHICS_FRAMES;
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR)
        swapchain->needs_rebuild = true;
    else if (presented != VK_SUCCESS) {
        swapchain->needs_rebuild = true;
        return graphics_vk_result(swapchain->device, presented, Graphics_ERROR_PRESENT);
    }
    return swapchain->needs_rebuild ? Graphics_SWAPCHAIN_OUT_OF_DATE : Graphics_NONE;
}

Graphics_Result graphics_cancel_frame(Graphics_Swapchain_t *swapchain) {
    if (swapchain == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!swapchain->active)
        return Graphics_ERROR_STATE;
    Graphics_Command_t *command = swapchain->frames[swapchain->frame];
    Graphics_Result result = graphics_command_reset(command);
    if (result == Graphics_NONE)
        result = graphics_command_begin(command);
    if (result == Graphics_NONE)
        result = graphics_end_frame(swapchain);
    if (result != Graphics_NONE) {
        swapchain->active = false;
        swapchain->needs_rebuild = true;
    }
    return result;
}
