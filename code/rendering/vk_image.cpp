#include "vk_image.hpp"


Image::Image(const Image_Create_Info& info) :
    m_device(info.device)
    , m_allocator(info.allocator)
    , m_gpu_allocator(info.gpu_allocator)
    , m_usage(info.usage_category)
    , m_flags(info.flags)
    , m_format(info.format)
    , m_extent(info.extent)
    , m_mip_levels(info.mip_levels)
    , m_tiling(info.tiling)
    , m_mode(info.mode)
    , m_queue_family_indices(info.queue_family_indices)
    , m_image(VK_NULL_HANDLE)
{
    if( info.concurrent_sharing ) {
        m_mode = VK_SHARING_MODE_CONCURRENT;
    }

    VkImageCreateInfo image_ci {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = m_flags,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = m_format,
        .extent = m_extent,
        .mipLevels = m_mip_levels > 0 ? m_mip_levels : 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = (m_tiling == Image_Tiling::Optimal) ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR,
        .usage = Derive_Vulkan_Usage(m_usage) | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = m_mode,
        .queueFamilyIndexCount = m_queue_family_indices.Length(),
        .pQueueFamilyIndices = m_queue_family_indices.Memory(),
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo alloc_ci = {};
	alloc_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	alloc_ci.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	#ifndef NDEBUG
    	// Validate all parameters
        printf("[Vulkan Debug] Image creation parameters:\n");
        printf("  extent: %ux%ux%u (must all be > 0)\n",
               m_extent.width, m_extent.height, m_extent.depth);
        printf("  format: %d\n", m_format);
        printf("  mipLevels: %u, arrayLayers: %u (must be > 0)\n",
               image_ci.mipLevels, image_ci.arrayLayers);
        printf("  samples: 0x%x (must be non-zero power of 2)\n", image_ci.samples);
        printf("  tiling: %d, usage: 0x%x\n", image_ci.tiling, image_ci.usage);
        printf("  sharingMode: %d\n", image_ci.sharingMode);

        // Check if format is supported
        VkImageFormatProperties format_props;
        VkResult check = vkGetPhysicalDeviceImageFormatProperties(
            m_device->Get_Physical_Device(),  // You need this method
            m_format,
            VK_IMAGE_TYPE_2D,
            image_ci.tiling,
            image_ci.usage,
            m_flags,
            &format_props
        );

        if (check == VK_ERROR_FORMAT_NOT_SUPPORTED) {
            printf("[Vulkan Error] Format %d with tiling %d and usage 0x%x is NOT supported!\n",
                   m_format, image_ci.tiling, image_ci.usage);
            // Try alternative format or tiling
        } else if (check != VK_SUCCESS) {
            printf("[Vulkan Warning] vkGetPhysicalDeviceImageFormatProperties failed: %d\n", check);
        }
	#endif

    VkResult result =  vmaCreateImage(m_gpu_allocator, &image_ci, &alloc_ci, &m_image, &m_gpu_allocation, nullptr);
	if( result != VK_SUCCESS ) {
        printf("[Vulkan Error] vmaCreateImage failed: %s\n", VkResult_To_String(result));
        exit(1);
	}

    // if the format is a depth format, we will need to have it use the correct
	// aspect flag
	VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
	if (m_format == VK_FORMAT_D32_SFLOAT) {
		aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build a image-view for the image
	VkImageViewCreateInfo view_info {
	   .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
	   .pNext = nullptr,
	   .flags = {},
	   .image = m_image,
	   .viewType = VK_IMAGE_VIEW_TYPE_2D,
	   .format = m_format,
	   .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = image_ci.mipLevels,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
	};

    VkImageView image_view = VK_NULL_HANDLE;
	if( vkCreateImageView(m_device->Get_Handle(), &view_info, nullptr, &image_view) != VK_SUCCESS ) {
        printf("[Vulkan Error] Could not create image view");
        exit(1);
	}

	m_image_view_handle = Image_View_Handle(image_view, m_device->Get_Handle());
}


void Image::Upload_Data_To_Image(void* data, size_t width, size_t height, size_t channels) {
    if( width * height * channels <= 0 ) {
        printf("[Vulkan Error] Trying to upload data with total size of 0, w:%d h:%d c:%d\n",
            width, height, channels);
        return;
    }

    VkDeviceSize size = static_cast<VkDeviceSize>(width * height * channels);

    Buffer_Create_Info buffer_ci {
        .size = size,
        .usage_category = Buffer_Usage::Staging,
        .additional_vulkan_usage = 0,  // For specialized cases
        .mapped_permanently = true,  // Keep CPU pointer valid
        .initial_data = data  // Optional immediate upload
    };

    Buffer staging_buffer(*m_device, buffer_ci);

	m_device->Submit_Immediate_Commands([&](VkCommandBuffer cmd) {
        // Barrier: UNDEFINED → TRANSFER_DST_OPTIMAL
        VkImageMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = m_image,  // Your stored VkImage
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        // Copy buffer to image
        VkBufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {static_cast<uint32_t>(width),
                           static_cast<uint32_t>(height), 1}
        };

        vkCmdCopyBufferToImage(cmd,
                               staging_buffer.Get_Handle(),
                               m_image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &region);

        // Barrier: TRANSFER_DST → SHADER_READ_ONLY_OPTIMAL
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    });
}


VkImageUsageFlagBits Image::Derive_Vulkan_Usage(Image_Usage usage) const noexcept {
    switch( usage ) {
        case Image_Usage::Color_Attachment : { return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;} break;
        case Image_Usage::Depth_Attachment : { return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;} break;
        case Image_Usage::Sampled          : { return VK_IMAGE_USAGE_SAMPLED_BIT;} break;
        case Image_Usage::Storage          : { return VK_IMAGE_USAGE_STORAGE_BIT;} break;
        case Image_Usage::Transfer_Src     : { return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;} break;
        case Image_Usage::Transfer_Dst     : { return VK_IMAGE_USAGE_TRANSFER_DST_BIT;} break;
        case Image_Usage::Present          :
        default               : return {};
    };
}