#include "vk_swapchain.hpp"

// Frame_Sync_Objects implementation
void Frame_Sync_Objects::Create(VkDevice device) {
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphore image_available_raw;
    VkSemaphore render_finished_raw;
    VkFence in_flight_raw;

    if (vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_raw) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_raw) != VK_SUCCESS ||
        vkCreateFence(device, &fence_info, nullptr, &in_flight_raw) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create synchronization objects");
    }

    // Transfer ownership to Vulkan_Handle
    image_available = Vulkan_Handle<VkSemaphore, Semaphore_Deleter>(image_available_raw, device);
    render_finished = Vulkan_Handle<VkSemaphore, Semaphore_Deleter>(render_finished_raw, device);
    in_flight = Vulkan_Handle<VkFence, Fence_Deleter>(in_flight_raw, device);
}

Swapchain::Swapchain(const Device& device, const Swapchain_Create_Info& info) {
    m_device = &device;
    m_surface = info.surface;
    m_vk_device = device.Get_Handle();

    Create_Swapchain(info);
    Create_Image_Views();
    Create_Sync_Objects();
}

Swapchain::Swapchain(Swapchain&& other) noexcept = default;
Swapchain& Swapchain::operator=(Swapchain&& other) noexcept = default;

Swapchain::~Swapchain() = default;  // RAII handles do all cleanup

void Swapchain::Create_Swapchain(const Swapchain_Create_Info& info) {
    VkSurfaceCapabilitiesKHR capabilities = Get_Surface_Capabilities();

    auto available_formats = Get_Available_Surface_Formats();
    auto available_present_modes = Get_Available_Present_Modes();

    m_format = Choose_Swap_Surface_Format(available_formats).format;
    m_color_space = Choose_Swap_Surface_Format(available_formats).colorSpace;
    m_present_mode = Choose_Swap_Present_Mode(available_present_modes);
    m_extent = Choose_Swap_Extent(capabilities, info.width, info.height);

    uint32_t image_count = info.min_image_count;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = m_surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = m_format;
    create_info.imageColorSpace = m_color_space;
    create_info.imageExtent = m_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t graphics_family = m_device->Get_Graphics_Queue_Family();
    uint32_t present_family = m_device->Get_Present_Queue_Family();
    uint32_t queue_family_indices[] = {graphics_family, present_family};

    if (graphics_family != present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = m_present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;  // No old swapchain on first creation

    VkSwapchainKHR raw_swapchain;
    if (vkCreateSwapchainKHR(m_vk_device, &create_info, nullptr, &raw_swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }

    // Transfer ownership to RAII handle
    m_swapchain = Vulkan_Handle<VkSwapchainKHR, Swapchain_Deleter>(raw_swapchain, m_vk_device);

    // Retrieve images (non-owning, managed by swapchain)
    vkGetSwapchainImagesKHR(m_vk_device, m_swapchain.Get_Handle(), &image_count, nullptr);
    m_images.resize(image_count);
    vkGetSwapchainImagesKHR(m_vk_device, m_swapchain.Get_Handle(), &image_count, m_images.data());
}

void Swapchain::Create_Image_Views() {
    m_image_views.clear();
    m_image_views.reserve(m_images.size());

    for (size_t i = 0; i < m_images.size(); i++) {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = m_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = m_format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        VkImageView raw_view;
        if (vkCreateImageView(m_vk_device, &create_info, nullptr, &raw_view) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }

        // Emplace RAII handle directly into vector
        m_image_views.emplace_back(raw_view, m_vk_device);
    }
}

void Swapchain::Create_Sync_Objects() {
    m_frame_sync.clear();
    m_frame_sync.resize(MAX_FRAMES_IN_FLIGHT);

    for (auto& sync : m_frame_sync) {
        sync.Create(m_vk_device);
    }
}

uint32_t Swapchain::Acquire_Next_Image(uint64_t timeout) {
    // Wait for current frame's fence
    vkWaitForFences(m_vk_device, 1, &m_frame_sync[m_current_frame].in_flight.Get_Handle(), VK_TRUE, UINT64_MAX);

    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(
        m_vk_device,
        m_swapchain.Get_Handle(),
        timeout,
        m_frame_sync[m_current_frame].image_available.Get_Handle(),
        VK_NULL_HANDLE,
        &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        m_needs_recreate = true;
        return 0;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    return image_index;
}

void Swapchain::Present_Image(uint32_t image_index, const std::vector<VkSemaphore>& wait_semaphores) {
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
    present_info.pWaitSemaphores = wait_semaphores.data();
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swapchain.Get_Handle();  // Get raw handle
    present_info.pImageIndices = &image_index;

    VkQueue present_queue = m_device->Get_Present_Queue();
    VkResult result = vkQueuePresentKHR(present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_needs_recreate = true;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

const Frame_Sync_Objects& Swapchain::Get_Frame_Sync(uint32_t frame_index) const {
    return m_frame_sync.at(frame_index);
}

void Swapchain::Recreate(uint32_t new_width, uint32_t new_height) {
    vkDeviceWaitIdle(m_vk_device);

    // Clear current resources - RAII handles clean up automatically
    m_image_views.clear();
    m_images.clear();
    m_frame_sync.clear();

    // Create new swapchain
    Swapchain_Create_Info info{};
    info.surface = m_surface;
    info.width = new_width;
    info.height = new_height;
    info.min_image_count = 2;
    info.present_mode = m_present_mode;
    info.preferred_format = m_format;
    info.preferred_color_space = m_color_space;

    Create_Swapchain(info);
    Create_Image_Views();
    Create_Sync_Objects();

    m_needs_recreate = false;
}

void Swapchain::Populate_VK_Render(VK_Render& render) const {
    render.swapchain = m_swapchain.Get_Handle();
    render.swapchain_images = m_images;

    // Extract raw handles from Vulkan_Handle vector
    render.swapchain_image_views.clear();
    render.swapchain_image_views.reserve(m_image_views.size());
    for (const auto& view : m_image_views) {
        render.swapchain_image_views.push_back(view.Get_Handle());
    }

    render.swapchain_format = m_format;
    render.swapchain_extent = m_extent;
}

// Utility implementations (unchanged logic, using m_vk_device)
VkSurfaceCapabilitiesKHR Swapchain::Get_Surface_Capabilities() const {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        m_device->Get_Physical_Device(), m_surface, &capabilities);
    return capabilities;
}

std::vector<VkPresentModeKHR> Swapchain::Get_Available_Present_Modes() const {
    uint32_t count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_device->Get_Physical_Device(), m_surface, &count, nullptr);

    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_device->Get_Physical_Device(), m_surface, &count, modes.data());
    return modes;
}

std::vector<VkSurfaceFormatKHR> Swapchain::Get_Available_Surface_Formats() const {
    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        m_device->Get_Physical_Device(), m_surface, &count, nullptr);

    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        m_device->Get_Physical_Device(), m_surface, &count, formats.data());
    return formats;
}

VkSurfaceFormatKHR Swapchain::Choose_Swap_Surface_Format(
    const std::vector<VkSurfaceFormatKHR>& available_formats) const {

    for (const auto& format : available_formats) {
        if (format.format == m_format && format.colorSpace == m_color_space) {
            return format;
        }
    }
    return available_formats[0];
}

VkPresentModeKHR Swapchain::Choose_Swap_Present_Mode(
    const std::vector<VkPresentModeKHR>& available_modes) const {

    for (auto mode : available_modes) {
        if (mode == m_present_mode) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::Choose_Swap_Extent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    uint32_t width, uint32_t height) const {

    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D extent = {width, height};
        extent.width = std::max(capabilities.minImageExtent.width,
                               std::min(capabilities.maxImageExtent.width, extent.width));
        extent.height = std::max(capabilities.minImageExtent.height,
                                std::min(capabilities.maxImageExtent.height, extent.height));
        return extent;
    }
}