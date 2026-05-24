#pragma once

#include "../third-party/vk_mem_alloc.h"
#include "../vector/DynamicVector.h"

#include "vk_device.hpp"
#include "vk_buffer.h"

enum class Image_Usage {
    Color_Attachment,
    Depth_Attachment,
    Sampled,
    Storage,
    Transfer_Src,
    Transfer_Dst,
    Present
};

enum class Image_Tiling {
    Optimal,   // VK_IMAGE_TILING_OPTIMAL
    Linear     // VK_IMAGE_TILING_LINEAR
};


struct Image_Create_Info {
    Device             *device;

    Allocator          *allocator;
    VmaAllocator       gpu_allocator;

    Image_Usage          usage_category = Image_Usage::Sampled;
    VkImageCreateFlags   flags         = 0;
    VkFormat             format        = VK_FORMAT_R8G8B8A8_UNORM;
    VkExtent3D           extent        = {1, 1, 1};
    U32                  mip_levels    = 0;
    Image_Tiling         tiling        = Image_Tiling::Optimal;
    VkSharingMode        mode          = VK_SHARING_MODE_EXCLUSIVE;
    dyn_vector<U32>      queue_family_indices;

    bool concurrent_sharing = false;
};

class Image {
public:

    Image() = delete;

    Image(const Image_Create_Info& info);

    Image(Image&&);
    Image& operator=(Image&&);

    Image(Image&)            = delete;
    Image& operator=(Image&) = delete;
    ~Image() {
        vmaDestroyImage(m_gpu_allocator, m_image, m_gpu_allocation);
    }

    // Upload functions
    //
    void Upload_Data_To_Image(void* data, size_t width, size_t height, size_t channels);
    void Upload_Data_To_Image(void* data, VkDeviceSize size, const VkBufferImageCopy* regions, U32 region_count, VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    Buffer& Create_Buffer_From_Image();
    void Copy_From_Image();

    // Get functions
    //
    VkImageUsageFlagBits Derive_Vulkan_Usage(Image_Usage usage) const noexcept;

    VmaAllocator       Get_Gpu_Allocator()         noexcept { return m_gpu_allocator; }
    VmaAllocation      Get_Gpu_Allocation()        noexcept { return m_gpu_allocation; }
    VkImage            Get_Handle()          const noexcept { return m_image; }
    VkImageView        Get_View_Handle()     const noexcept { return m_image_view_handle.Get(); }
    VkImageCreateFlags Get_Flags()           const noexcept { return m_flags; }
    VkFormat           Get_Format()          const noexcept { return m_format; }
    VkExtent3D         Get_Extent()          const noexcept { return m_extent; }
    U32                Get_Mip_Levels()      const noexcept { return m_mip_levels; }
    Image_Tiling       Get_Tiling()          const noexcept { return m_tiling; }
    Image_Usage        Get_Usage()           const noexcept { return m_usage; }
    VkSharingMode      Get_Mode()            const noexcept { return m_mode; }

private:
    // Allocator
    //
    Allocator* m_allocator;

    // GPU-Side Allocator
    //
    VmaAllocator m_gpu_allocator;
    VmaAllocation m_gpu_allocation;

    // Pointers from necessary data
    //
    Device* m_device;

    // Data
    //
    VkImageCreateFlags m_flags;
    VkFormat           m_format;
    VkExtent3D         m_extent;
    U32                m_mip_levels;
    Image_Tiling       m_tiling;
    Image_Usage        m_usage;
    VkSharingMode      m_mode;
    dyn_vector<U32>    m_queue_family_indices;

    // handles
    //
    Image_View_Handle m_image_view_handle;
    VkImage           m_image;
};
