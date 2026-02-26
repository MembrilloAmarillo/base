#ifndef _VK_SWAPCHAIN_HPP_
#define _VK_SWAPCHAIN_HPP_

// Swapchain configuration
struct Swapchain_Create_Info {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    uint32_t width = 800;
    uint32_t height = 600;
    uint32_t min_image_count = 2;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    VkFormat preferred_format = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR preferred_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
};

// Per-frame synchronization resources using Vulkan_Handle
struct Frame_Sync_Objects {
    Vulkan_Handle<VkSemaphore, Semaphore_Deleter> image_available;
    Vulkan_Handle<VkSemaphore, Semaphore_Deleter> render_finished;
    Vulkan_Handle<VkFence, Fence_Deleter> in_flight;

    // Construction helper
    void Create(VkDevice device);
};

class Swapchain {
public:
    // Constructors
    Swapchain() = default;
    Swapchain(const Device& device, const Swapchain_Create_Info& info);

    // Move semantics
    Swapchain(Swapchain&& other) noexcept;
    Swapchain& operator=(Swapchain&& other) noexcept;

    // Non-copyable
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // Destructor
    ~Swapchain() = default;  // RAII members handle cleanup

    // Accessors
    VkSwapchainKHR Get_Handle() const noexcept { return m_swapchain.Get_Handle(); }
    bool Is_Valid() const noexcept { return static_cast<bool>(m_swapchain); }

    // Image management
    uint32_t Get_Image_Count() const noexcept { return static_cast<uint32_t>(m_images.size()); }
    VkImage Get_Image(uint32_t index) const { return m_images.at(index); }
    VkImageView Get_Image_View(uint32_t index) const { return m_image_views.at(index).Get_Handle(); }
    VkFormat Get_Format() const noexcept { return m_format; }
    VkExtent2D Get_Extent() const noexcept { return m_extent; }

    // Frame synchronization
    uint32_t Acquire_Next_Image(uint64_t timeout = UINT64_MAX);
    void Present_Image(uint32_t image_index, const std::vector<VkSemaphore>& wait_semaphores);
    const Frame_Sync_Objects& Get_Frame_Sync(uint32_t frame_index) const;

    // Resize handling
    void Recreate(uint32_t new_width, uint32_t new_height);
    bool Needs_Recreate() const noexcept { return m_needs_recreate; }
    void Reset_Recreate_Flag() { m_needs_recreate = false; }

    // Populate VK_Render for compatibility
    void Populate_VK_Render(VK_Render& render) const;

private:
    // Core handle using Vulkan_Handle RAII
    Vulkan_Handle<VkSwapchainKHR, Swapchain_Deleter> m_swapchain;

    // Images (non-owning, managed by swapchain)
    std::vector<VkImage> m_images;

    // Image views (owning, RAII)
    std::vector<Vulkan_Handle<VkImageView, Image_View_Deleter>> m_image_views;

    // Properties
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR m_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D m_extent = {0, 0};
    VkPresentModeKHR m_present_mode = VK_PRESENT_MODE_FIFO_KHR;

    // Per-frame sync objects
    std::vector<Frame_Sync_Objects> m_frame_sync;
    uint32_t m_current_frame = 0;
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    // State
    bool m_needs_recreate = false;

    // Device reference (non-owning)
    const Device* m_device = nullptr;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkDevice m_vk_device = VK_NULL_HANDLE;  // Cached for convenience

    // Internal helpers
    void Create_Swapchain(const Swapchain_Create_Info& info);
    void Create_Image_Views();
    void Create_Sync_Objects();
    void Destroy_Resources();

    // Utility functions
    VkSurfaceFormatKHR Choose_Swap_Surface_Format(
        const std::vector<VkSurfaceFormatKHR>& available_formats) const;
    VkPresentModeKHR Choose_Swap_Present_Mode(
        const std::vector<VkPresentModeKHR>& available_modes) const;
    VkExtent2D Choose_Swap_Extent(
        const VkSurfaceCapabilitiesKHR& capabilities,
        uint32_t width, uint32_t height) const;
};

#endif // _VK_SWAPCHAIN_HPP_
