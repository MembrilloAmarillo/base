#include "vk_render.h"
#include <algorithm>
#include <cstring>
#include <vector>

// Debug utils callback implementation
VKAPI_ATTR VkBool32 VKAPI_CALL VkDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
											   VkDebugUtilsMessageTypeFlagsEXT messageType,
											   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
											   void* pUserData) {
	(void)messageSeverity; (void)messageType; (void)pUserData;
	fprintf(stderr, "[VULKAN DEBUG] %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

static bool Has_Stencil_Component(VkFormat format) {
	return format == VK_FORMAT_D16_UNORM_S8_UINT ||
	       format == VK_FORMAT_D24_UNORM_S8_UINT ||
	       format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

static bool Has_Instance_Layer(const char* requested_layer) {
	U32 layer_count = 0;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
	std::vector<VkLayerProperties> layers(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, layers.data());
	for (const VkLayerProperties& layer : layers) {
		if (std::strcmp(layer.layerName, requested_layer) == 0) {
			return true;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////
//             Class Vk_Texture: Load_From_Ktx                     //
/////////////////////////////////////////////////////////////////////

VkDescriptorImageInfo Vk_Texture::Load_From_Ktx( Vk_Render *render, const char *path ) {
    Destroy(render);

    VkDevice                   device = render->Get_Device();
    Device*           device_object = render->Get_Device_Object();
    //Vk_Bindless_Table  bindless_table = render->Get_Bindless_Table();

	ktxTexture* ktx_texture = nullptr;
	ktxResult result = ktxTexture_CreateFromNamedFile(path, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
	if (result != KTX_SUCCESS) {
		printf("ERROR: Failed to load KTX texture from '%s': %s\n", path, ktxErrorString(result));
		return {};
	}
	Image_Create_Info image_ci{
		.device = device_object,
		.allocator = allocator,
		.gpu_allocator = render->Get_Vma_Allocator(),
		.usage_category = Image_Usage::Sampled,
		.flags = 0,
		.format = ktxTexture_GetVkFormat(ktx_texture),
		.extent = {.width = ktx_texture->baseWidth, .height = ktx_texture->baseHeight, .depth = 1},
		.mip_levels = ktx_texture->numLevels,
		.tiling = Image_Tiling::Optimal,
		.mode = VK_SHARING_MODE_EXCLUSIVE,
		.concurrent_sharing = false
	};
	image_resource = std::make_unique<Image>(image_ci);

	dyn_vector<VkBufferImageCopy> copy_regions =
	   dyn_vector<VkBufferImageCopy>::Init(allocator, ktx_texture->numLevels);

    // Validate offsets and build copy regions. ktxTexture provides image sizes
    // and offsets; ensure they are inside the loaded pData buffer.
    for (U32 j = 0; j < ktx_texture->numLevels; ++j) {
        ktx_size_t mipOffset{0};
        KTX_error_code ret = ktxTexture_GetImageOffset(ktx_texture, j, 0, 0, &mipOffset);
        if (ret != KTX_SUCCESS) {
            fprintf(stderr, "ktxTexture_GetImageOffset failed for level %d\n", j);
            abort();
        }
        ktx_size_t imageSize = ktxTexture_GetImageSize(ktx_texture, j);
        if (mipOffset + imageSize > (ktx_size_t)ktx_texture->dataSize) {
            fprintf(stderr, "ktx image data overflow: level %d offset %zu size %zu dataSize %zu\n",
                    j, (size_t)mipOffset, (size_t)imageSize, (size_t)ktx_texture->dataSize);
            abort();
        }

        uint32_t mipWidth = std::max<uint32_t>(1u, static_cast<uint32_t>(ktx_texture->baseWidth >> j));
        uint32_t mipHeight = std::max<uint32_t>(1u, static_cast<uint32_t>(ktx_texture->baseHeight >> j));
        VkBufferImageCopy region{};
        region.bufferOffset = mipOffset;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = j;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = mipWidth;
        region.imageExtent.height = mipHeight;
        region.imageExtent.depth = 1;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        copy_regions.AppendByCopy(region);
    }

    image_resource->Upload_Data_To_Image(
        ktx_texture->pData,
        static_cast<VkDeviceSize>(ktx_texture->dataSize),
        copy_regions.Memory(),
        static_cast<U32>(copy_regions.Length()),
        VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
    );

    copy_regions.Destroy();
	// Sampler
	Create_Sampler(device, static_cast<f32>(ktx_texture->numLevels));
	ktxTexture_Destroy(ktx_texture);

	layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	return {
		.sampler = sampler,
		.imageView = image_resource->Get_View_Handle(),
		.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
	};
}

VkDescriptorImageInfo Vk_Texture::Load_From_R8_Data(Vk_Render *render, const void* pixels, U32 width, U32 height) {
    if (render == nullptr || pixels == nullptr || width == 0 || height == 0) {
        fprintf(stderr, "ERROR: Load_From_R8_Data received invalid input\n");
        return {};
    }

    Destroy(render);

    Device* device_object = render->Get_Device_Object();
    Image_Create_Info image_ci{
        .device = device_object,
        .allocator = allocator,
        .gpu_allocator = render->Get_Vma_Allocator(),
        .usage_category = Image_Usage::Sampled,
        .flags = 0,
        .format = VK_FORMAT_R8_UNORM,
        .extent = {width, height, 1},
        .mip_levels = 1,
        .tiling = Image_Tiling::Optimal,
        .mode = VK_SHARING_MODE_EXCLUSIVE,
        .concurrent_sharing = false
    };
    image_resource = std::make_unique<Image>(image_ci);
    image_resource->Upload_Data_To_Image(const_cast<void*>(pixels), width, height, 1);

    Create_Sampler(render->Get_Device(), 0.0f);
    layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return {
        .sampler = sampler,
        .imageView = image_resource->Get_View_Handle(),
        .imageLayout = layout
    };
}

void Vk_Texture::Destroy(Vk_Render *render) {
    if (image_resource) {
        image_resource.reset();
    }
    if (render == nullptr) {
        layout = VK_IMAGE_LAYOUT_UNDEFINED;
        return;
    }

    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(render->Get_Device(), sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }
    layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

U32 Vk_Texture::Load_From_Ktx_Bindless(Vk_Render *render, const char *path) {
	return Load_From_Ktx_Bindless_With_Sampler(render, path).first;
}

std::pair<U32, U32> Vk_Texture::Load_From_Ktx_Bindless_With_Sampler(Vk_Render *render, const char *path) {
	// First, load using the existing method
	VkDescriptorImageInfo desc_info = Load_From_Ktx(render, path);

	Vk_Bindless_Table *bindless_table = &render->Get_Bindless_Table();

	if (!bindless_table) {
		fprintf(stderr, "ERROR: Bindless table is null in Load_From_Ktx_Bindless\n");
		return {UINT32_MAX, UINT32_MAX};
	}

	// Register the texture and sampler with the bindless table
	U32 texture_handle = bindless_table->Register_Texture(desc_info.imageView, desc_info.imageLayout);
	U32 sampler_handle = bindless_table->Register_Sampler(desc_info.sampler);

	// Return both handles
	return {texture_handle, sampler_handle};
}

/////////////////////////////////////////////////////////////////////
//         Class Vk_Bindless_Table: Implementation                 //
/////////////////////////////////////////////////////////////////////

void Vk_Bindless_Table::Init(Allocator* alloc, VkDevice dev) {
	allocator = alloc;
	device = dev;

	handle_entries = dyn_vector<Handle_Entry>::Init(alloc, 1024);
	free_handles = dyn_vector<U32>::Init(alloc, 1024);
	next_generation = 1;

	// Create descriptor set layout with variable descriptor count on last binding
	VkDescriptorSetLayoutBinding bindings[3];

	// Binding 0: Sampled images
	bindings[0] = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = MAX_TEXTURES,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
	};

	// Binding 1: Samplers
	bindings[1] = {
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
		.descriptorCount = MAX_SAMPLERS,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
	};

	// Binding 2: Storage buffers
	bindings[2] = {
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = MAX_STORAGE_BUFFERS,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
	};

	// Set descriptor binding flags for all bindings
	VkDescriptorBindingFlags binding_flags[3];
	for (int i = 0; i < 3; ++i) {
		binding_flags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
		                 | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
	}

	// Variable descriptor count on the storage buffer binding (last one)
	binding_flags[2] |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_ci{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.bindingCount = 3,
		.pBindingFlags = binding_flags
	};

	VkDescriptorSetLayoutCreateInfo layout_ci{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &binding_flags_ci,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		.bindingCount = 3,
		.pBindings = bindings
	};

	Vk::Check(vkCreateDescriptorSetLayout(device, &layout_ci, nullptr, &descriptor_set_layout));

	// Create descriptor pool with UPDATE_AFTER_BIND flag
	VkDescriptorPoolSize pool_sizes[3];
	pool_sizes[0] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURES };
	pool_sizes[1] = { VK_DESCRIPTOR_TYPE_SAMPLER, MAX_SAMPLERS };
	pool_sizes[2] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_STORAGE_BUFFERS };

	VkDescriptorPoolCreateInfo pool_ci{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets = 1,
		.poolSizeCount = 3,
		.pPoolSizes = pool_sizes
	};

	Vk::Check(vkCreateDescriptorPool(device, &pool_ci, nullptr, &descriptor_pool));

	// Allocate descriptor set with variable descriptor count (start with 0)
	VkDescriptorSetVariableDescriptorCountAllocateInfo count_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
		.descriptorSetCount = 1,
		.pDescriptorCounts = &current_buffer_count  // Start with 0 buffers
	};

	VkDescriptorSetAllocateInfo alloc_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = &count_info,
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &descriptor_set_layout
	};

	Vk::Check(vkAllocateDescriptorSets(device, &alloc_info, &descriptor_set));
}

void Vk_Bindless_Table::Shutdown() {
	if (device != VK_NULL_HANDLE) {
		if (descriptor_set_layout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
		}
		if (descriptor_pool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
		}
	}
	if (allocator) {
		handle_entries.Destroy();
		free_handles.Destroy();
	}
	allocator = nullptr;
	device = VK_NULL_HANDLE;
	descriptor_pool = VK_NULL_HANDLE;
	descriptor_set = VK_NULL_HANDLE;
	descriptor_set_layout = VK_NULL_HANDLE;
	current_texture_count = 0;
	current_sampler_count = 0;
	current_buffer_count = 0;
	next_generation = 1;
}

U32 Vk_Bindless_Table::Register_Texture(VkImageView image_view, VkImageLayout layout) {
	// Allocate a handle
	U32 handle;
	if (free_handles.Length() > 0) {
		handle = free_handles.At(free_handles.Length() - 1);
		free_handles.Len--;
	} else {
		handle = (U32)handle_entries.Length();
		Handle_Entry entry{ next_generation, true };
		handle_entries.AppendByCopy(entry);
	}

	// Update the descriptor set
	VkDescriptorImageInfo image_info{
		.sampler = VK_NULL_HANDLE,  // Will be set by sampler in shader
		.imageView = image_view,
		.imageLayout = layout
	};

	VkWriteDescriptorSet write{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descriptor_set,
		.dstBinding = 0,  // Binding 0 is textures
		.dstArrayElement = handle,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.pImageInfo = &image_info
	};

	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

	current_texture_count = std::max(current_texture_count, handle + 1);

	return handle;
}

U32 Vk_Bindless_Table::Register_Sampler(VkSampler sampler) {
	// Allocate a handle
	U32 handle;
	if (free_handles.Length() > 0) {
		handle = free_handles.At(free_handles.Length() - 1);
		free_handles.Len--;
	} else {
		handle = (U32)handle_entries.Length();
		Handle_Entry entry{ next_generation, true };
		handle_entries.AppendByCopy(entry);
	}

	// Update the descriptor set
	VkDescriptorImageInfo image_info{
		.sampler = sampler,
		.imageView = VK_NULL_HANDLE,
		.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VkWriteDescriptorSet write{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descriptor_set,
		.dstBinding = 1,  // Binding 1 is samplers
		.dstArrayElement = handle,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
		.pImageInfo = &image_info
	};

	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

	current_sampler_count = std::max(current_sampler_count, handle + 1);

	return handle;
}

U32 Vk_Bindless_Table::Register_Storage_Buffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size) {
	// Allocate a handle
	U32 handle;
	if (free_handles.Length() > 0) {
		handle = free_handles.At(free_handles.Length() - 1);
		free_handles.Len--;
	} else {
		handle = (U32)handle_entries.Length();
		Handle_Entry entry{ next_generation, true };
		handle_entries.AppendByCopy(entry);
	}

	// Update the descriptor set
	VkDescriptorBufferInfo buffer_info{
		.buffer = buffer,
		.offset = offset,
		.range = size
	};

	VkWriteDescriptorSet write{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descriptor_set,
		.dstBinding = 2,  // Binding 2 is storage buffers
		.dstArrayElement = handle,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pBufferInfo = &buffer_info
	};

	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

	current_buffer_count = std::max(current_buffer_count, handle + 1);

	return handle;
}

void Vk_Bindless_Table::Unregister(U32 handle) {
	if (handle >= (U32)handle_entries.Length()) {
		fprintf(stderr, "Warning: Unregistering invalid handle %u\n", handle);
		return;
	}

	// Mark as invalid
	handle_entries[handle].is_valid = false;
	handle_entries[handle].generation++;

	// Add to free list
	free_handles.AppendByCopy(handle);
}

/////////////////////////////////////////////////////////////////////
//                 Class Vk_Render: Init                           //
/////////////////////////////////////////////////////////////////////

void Vk_Render::Init(F64 w, F64 h, Allocator* Alloc) {
    this->Alloc = Alloc;
    window = SurfaceCreateWindow(w, h);
    //volkInitialize();

	std::vector<const char*> extensions;
	extensions.reserve(8);
#ifdef SDL_USAGE
	U32 instance_extension_count{ 0 };
	char const* const* sdl_exts = SDL_Vulkan_GetInstanceExtensions(&instance_extension_count);
	extensions.reserve(instance_extension_count + 2);
	for (U32 i = 0; i < instance_extension_count; ++i) {
		extensions.push_back(sdl_exts[i]);
	}
#else
	extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_XLIB_KHR)
	extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
	extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
#endif

	const char* validation_layer = "VK_LAYER_KHRONOS_validation";
	const bool validation_available = Has_Instance_Layer(validation_layer);
	if (validation_available) {
		fprintf(stderr, "[Vulkan] Validation layer enabled: %s\n", validation_layer);
	} else {
		fprintf(stderr, "[Vulkan] Validation layer not found: %s\n", validation_layer);
	}

	Instance::Create_Info instance_ci{};
	instance_ci.app_name = "Vulkan Instance";
	instance_ci.api_version = VK_API_VERSION_1_3;
	instance_ci.extensions = extensions;
	instance_ci.enable_validation = validation_available;
	instance_ci.enable_debug_messenger = validation_available;
	instance_ci.mem_allocator = Alloc;

	instance_owner = std::make_unique<Instance>(instance_ci);
	instance = instance_owner->Get_Handle();
	debug_messenger = VK_NULL_HANDLE;

	// Swap chain
	//
	Check(SurfaceCreateVkSurface(&window, instance));
	Device_Create_Info device_ci{};
	device_ci.surface = window.Surface;
	device_ci.enable_vma = true;
	device_ci.allocator = Alloc;
	device_owner = std::make_unique<Device>(*instance_owner, device_ci);
	physical_device = device_owner->Get_Physical_Device();
	device = device_owner->Get_Handle();
	queue = device_owner->Get_Graphics_Queue();
	command_pool = device_owner->Get_Graphics_Command_Pool();
	allocator = device_owner->Get_Vma_Allocator();

	vec2 drawable_size = SurfaceGetWindowSize(&window);
	window.Width = Max(1.0f, drawable_size.x);
	window.Height = Max(1.0f, drawable_size.y);
	VkSurfaceCapabilitiesKHR surface_caps{};
	Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, window.Surface, &surface_caps));

	const VkFormat image_format { VK_FORMAT_B8G8R8A8_SRGB };
	auto pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	if( surface_caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ) {
		pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	} else {
		pre_transform = surface_caps.currentTransform;
	}
	surface_caps.currentExtent.width  = window.Width;
	surface_caps.currentExtent.height = window.Height;
	U32 swapchain_image_count = surface_caps.minImageCount + 1;
	if(surface_caps.maxImageCount > 0 && swapchain_image_count > surface_caps.maxImageCount) {
		swapchain_image_count = surface_caps.maxImageCount;
	}
	VkSwapchainCreateInfoKHR swapchain_ci{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = window.Surface,
		.minImageCount = swapchain_image_count,
		.imageFormat = image_format ,
		.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
		.imageExtent{.width = surface_caps.currentExtent.width, .height = surface_caps.currentExtent.height },
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = pre_transform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR
	};
	Check(vkCreateSwapchainKHR(device, &swapchain_ci, nullptr, &swapchain));
	U32 image_count{ 0 };
	Check(vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr));
	swapchain_images = dyn_vector<VkImage>::Init(Alloc, image_count);
	Check(vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.Memory()));
	swapchain_images.Len = image_count;
	swapchain_image_views = dyn_vector<VkImageView>::Init(Alloc, image_count);
	for (U32 i = 0; i < image_count; ++i) {
		VkImageViewCreateInfo view_ci{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = swapchain_images.At(i), .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = image_format, .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 } };
		Check(vkCreateImageView(device, &view_ci, nullptr, swapchain_image_views.Memory() + i));
	}
	swapchain_image_views.Len = image_count;
	swapchain_image_layouts = dyn_vector<VkImageLayout>::Init(Alloc, image_count);
	for (U32 i = 0; i < image_count; ++i) {
		swapchain_image_layouts.AppendByCopy(VK_IMAGE_LAYOUT_UNDEFINED);
	}
	printf("Created swapchain\n");
	// Depth attachment
    VkFormat depth_format_list[] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	for (U64 i = 0; i < ArrayCount(depth_format_list); ++i) {
        VkFormat format = depth_format_list[i];
        VkFormatProperties2 format_properties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
        vkGetPhysicalDeviceFormatProperties2(physical_device, format, &format_properties);
        if (format_properties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            depth_format = format;
            break;
        }
	}
	assert(depth_format != VK_FORMAT_UNDEFINED);
	VkImageCreateInfo depth_image_ci{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = depth_format,
		.extent{
		  .width = static_cast<U32>(window.Width),
		  .height = static_cast<U32>(window.Height),
		  .depth = 1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VmaAllocationCreateInfo alloc_ci{
	   .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
	   .usage = VMA_MEMORY_USAGE_AUTO
	};
	Check(vmaCreateImage(
	       allocator,
	       &depth_image_ci,
	       &alloc_ci,
	       &depth_image,
	       &depth_image_allocation,
	       nullptr
        )
    );
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (Has_Stencil_Component(depth_format)) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
    VkImageViewCreateInfo depth_view_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depth_format,
        .subresourceRange{
            .aspectMask = depth_aspect,
            .levelCount = 1,
            .layerCount = 1
        }
    };
    Check(vkCreateImageView(device, &depth_view_ci, nullptr, &depth_image_view));

    // Sync objects
    //
    VkSemaphoreCreateInfo semaphore_ci{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};
	VkFenceCreateInfo fence_ci {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};
	for (U32 i = 0; i < max_frames_in_flight; ++i) {
		Check(vkCreateFence(device, &fence_ci, nullptr, &fences[i]));
		Check(vkCreateSemaphore(device, &semaphore_ci, nullptr, &present_semaphores[i]));
	}
	render_semaphores = dyn_vector<VkSemaphore>::Init(Alloc, swapchain_images.Capacity());
	for (U64 i = 0; i < render_semaphores.Capacity(); ++i) {
        auto semaphore = (render_semaphores.Memory() + i);
        Check(vkCreateSemaphore(device, &semaphore_ci, nullptr, semaphore));
	}
	render_semaphores.Len = render_semaphores.Capacity();

	// Command buffers
	//
	VkCommandBufferAllocateInfo cbAllocCI{
	   .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	   .commandPool = command_pool,
	   .commandBufferCount = max_frames_in_flight
    };

	Check(vkAllocateCommandBuffers(
	   device,
	   &cbAllocCI,
	   command_buffers
   ));

   // Initialize Slang shader compiler
   //
	slang::createGlobalSession(slang_global_session.writeRef());
	slang_target = { slang::TargetDesc{} };
	slang_target[0].format = SLANG_SPIRV;
	slang_target[0].profile = slang_global_session->findProfile("spirv_1_4");
	slang_options = std::to_array<slang::CompilerOptionEntry>({
		{
			slang::CompilerOptionName::EmitSpirvDirectly,
			{slang::CompilerOptionValueKind::Int, 1}
		}
	});

	slang_session_description = slang::SessionDesc{};
	slang_session_description.targets = slang_target.data();
	slang_session_description.targetCount = SlangInt(slang_target.size());
	slang_session_description.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
	slang_session_description.compilerOptionEntries = slang_options.data();
	slang_session_description.compilerOptionEntryCount = U32(slang_options.size());

	// Initialize Bindless Descriptor Table
	//
	bindless_table.Init(Alloc, device);
	TRACE("[Init] Bindless descriptor table initialized");
	initialized = true;
}

void Vk_Render::Shutdown() {
	if (!initialized) {
		return;
	}

	if (device != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(device);
	}

	bindless_table.Shutdown();

	if (device != VK_NULL_HANDLE) {
		for (U32 i = 0; i < max_frames_in_flight; ++i) {
			if (fences[i] != VK_NULL_HANDLE) {
				vkDestroyFence(device, fences[i], nullptr);
				fences[i] = VK_NULL_HANDLE;
			}
			if (present_semaphores[i] != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, present_semaphores[i], nullptr);
				present_semaphores[i] = VK_NULL_HANDLE;
			}
		}

		for (U64 i = 0; i < render_semaphores.Length(); ++i) {
			if (render_semaphores[i] != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, render_semaphores[i], nullptr);
			}
		}
		render_semaphores.Destroy();

		for (U64 i = 0; i < swapchain_image_views.Length(); ++i) {
			if (swapchain_image_views[i] != VK_NULL_HANDLE) {
				vkDestroyImageView(device, swapchain_image_views[i], nullptr);
			}
		}
		swapchain_image_views.Destroy();
		swapchain_images.Destroy();
		swapchain_image_layouts.Destroy();

		if (depth_image_view != VK_NULL_HANDLE) {
			vkDestroyImageView(device, depth_image_view, nullptr);
			depth_image_view = VK_NULL_HANDLE;
		}
		if (depth_image != VK_NULL_HANDLE) {
			vmaDestroyImage(allocator, depth_image, depth_image_allocation);
			depth_image = VK_NULL_HANDLE;
			depth_image_allocation = VK_NULL_HANDLE;
		}
		if (swapchain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(device, swapchain, nullptr);
			swapchain = VK_NULL_HANDLE;
		}
	}

	if (window.Surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
		SurfaceDestroyVkSurface(&window, instance);
		window.Surface = VK_NULL_HANDLE;
	}
	device_owner.reset();
	instance_owner.reset();
	device = VK_NULL_HANDLE;
	physical_device = VK_NULL_HANDLE;
	queue = VK_NULL_HANDLE;
	command_pool = VK_NULL_HANDLE;
	allocator = VK_NULL_HANDLE;
	debug_messenger = VK_NULL_HANDLE;
	instance = VK_NULL_HANDLE;

	initialized = false;
}

void Vk_Render::Recreate_Swapchain() {
	if (!initialized || device == VK_NULL_HANDLE || window.Surface == VK_NULL_HANDLE) {
		return;
	}

	vkDeviceWaitIdle(device);

	vec2 drawable_size = SurfaceGetWindowSize(&window);
	window.Width = Max(1.0f, drawable_size.x);
	window.Height = Max(1.0f, drawable_size.y);

	for (U64 i = 0; i < render_semaphores.Length(); ++i) {
		if (render_semaphores[i] != VK_NULL_HANDLE) {
			vkDestroySemaphore(device, render_semaphores[i], nullptr);
		}
	}
	render_semaphores.Destroy();

	for (U64 i = 0; i < swapchain_image_views.Length(); ++i) {
		if (swapchain_image_views[i] != VK_NULL_HANDLE) {
			vkDestroyImageView(device, swapchain_image_views[i], nullptr);
		}
	}
	swapchain_image_views.Destroy();
	swapchain_images.Destroy();
	swapchain_image_layouts.Destroy();

	if (depth_image_view != VK_NULL_HANDLE) {
		vkDestroyImageView(device, depth_image_view, nullptr);
		depth_image_view = VK_NULL_HANDLE;
	}
	if (depth_image != VK_NULL_HANDLE) {
		vmaDestroyImage(allocator, depth_image, depth_image_allocation);
		depth_image = VK_NULL_HANDLE;
		depth_image_allocation = VK_NULL_HANDLE;
	}
	if (swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}

	VkSurfaceCapabilitiesKHR surface_caps{};
	Vk::Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, window.Surface, &surface_caps));

	VkExtent2D extent{};
	if (surface_caps.currentExtent.width != UINT32_MAX) {
		extent = surface_caps.currentExtent;
	} else {
		extent.width = static_cast<U32>(window.Width);
		extent.height = static_cast<U32>(window.Height);
		extent.width = std::clamp(extent.width, surface_caps.minImageExtent.width, surface_caps.maxImageExtent.width);
		extent.height = std::clamp(extent.height, surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height);
	}
	window.Width = static_cast<F64>(extent.width);
	window.Height = static_cast<F64>(extent.height);

	const VkFormat image_format { VK_FORMAT_B8G8R8A8_SRGB };
	auto pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	if (surface_caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
		pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	} else {
		pre_transform = surface_caps.currentTransform;
	}

	U32 swapchain_image_count = surface_caps.minImageCount + 1;
	if(surface_caps.maxImageCount > 0 && swapchain_image_count > surface_caps.maxImageCount) {
		swapchain_image_count = surface_caps.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchain_ci{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = window.Surface,
		.minImageCount = swapchain_image_count,
		.imageFormat = image_format,
		.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = pre_transform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR
	};
	Vk::Check(vkCreateSwapchainKHR(device, &swapchain_ci, nullptr, &swapchain));

	U32 image_count{0};
	Vk::Check(vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr));
	swapchain_images = dyn_vector<VkImage>::Init(Alloc, image_count);
	Vk::Check(vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.Memory()));
	swapchain_images.Len = image_count;

	swapchain_image_views = dyn_vector<VkImageView>::Init(Alloc, image_count);
	for (U32 i = 0; i < image_count; ++i) {
		VkImageViewCreateInfo view_ci{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_images.At(i),
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = image_format,
			.subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
		};
		Vk::Check(vkCreateImageView(device, &view_ci, nullptr, swapchain_image_views.Memory() + i));
	}
	swapchain_image_views.Len = image_count;

	swapchain_image_layouts = dyn_vector<VkImageLayout>::Init(Alloc, image_count);
	for (U32 i = 0; i < image_count; ++i) {
		swapchain_image_layouts.AppendByCopy(VK_IMAGE_LAYOUT_UNDEFINED);
	}

	VkImageCreateInfo depth_image_ci{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = depth_format,
		.extent = {.width = extent.width, .height = extent.height, .depth = 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VmaAllocationCreateInfo depth_alloc_ci{
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	Vk::Check(vmaCreateImage(allocator, &depth_image_ci, &depth_alloc_ci, &depth_image, &depth_image_allocation, nullptr));

	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (Has_Stencil_Component(depth_format)) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	VkImageViewCreateInfo depth_view_ci{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = depth_image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = depth_format,
		.subresourceRange = {.aspectMask = depth_aspect, .levelCount = 1, .layerCount = 1}
	};
	Vk::Check(vkCreateImageView(device, &depth_view_ci, nullptr, &depth_image_view));
	depth_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkSemaphoreCreateInfo semaphore_ci{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	render_semaphores = dyn_vector<VkSemaphore>::Init(Alloc, image_count);
	for (U32 i = 0; i < image_count; ++i) {
		Vk::Check(vkCreateSemaphore(device, &semaphore_ci, nullptr, render_semaphores.Memory() + i));
	}
	render_semaphores.Len = image_count;
	update_swapchain = false;
	fprintf(stderr, "[Vulkan] Recreated swapchain: %ux%u, %u images\n", extent.width, extent.height, image_count);
}

void Vk_Render::Begin_Command_Buffer() {
	auto cmd = command_buffers[frame_index];

	Vk::Check( vkResetCommandBuffer(cmd, 0) );

	VkCommandBufferBeginInfo cmd_bi {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	Vk::Check( vkBeginCommandBuffer( cmd, &cmd_bi ) );
}

void Vk_Render::End_Command_Buffer() {
	auto cmd = command_buffers[frame_index];
    Vk::Check(vkEndCommandBuffer(cmd));
}

// ============================================================================
// Vk_Buffer Constructor
// ============================================================================
template <typename T>
Vk_Buffer<T>::Vk_Buffer(Allocator* alloc, Vk_Render* Render, const Buffer_Description& desc)
{
    if (desc.size == 0) {
        TRACE("[Vk_Buffer] Warning: Creating buffer with size 0");
    }

    // Initialize core members
    //
    allocator = alloc;
    description = desc;
    vma_allocator_copy = Render->Get_Vma_Allocator();

    cpu_mapped_ptr = nullptr;
    is_persistently_mapped = false;
    staging_buffer = VK_NULL_HANDLE;
    staging_buffer_allocation = nullptr;
    staging_cpu_ptr = nullptr;
    owns_staging_buffer = false;

    // Initialize frame states
    frame_states = dyn_vector<Frame_State>::Init(alloc, desc.max_frames_in_flight);
    for (U32 i = 0; i < desc.max_frames_in_flight; ++i) {
        Frame_State state{};
        state.frame_number = i;
        state.gpu_work_pending = false;
        state.last_write_time_ns = 0;
        frame_states.AppendByCopy(state);
    }

    // Initialize statistics
    stats.total_writes = 0;
    stats.total_gpu_transfers = 0;
    stats.last_access_frame = 0;

    // Vulkan buffer creation
    //
    VkDevice device = Render->Get_Device();
    VmaAllocator vma_allocator = vma_allocator_copy;

    VkBufferCreateInfo buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = desc.size,
        .usage = desc.Usage_Convert_To_Vulkan() | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    // Determine allocation strategy
    const bool is_upload_buffer = desc.usage & Buffer_Usage::Upload;
    VmaAllocationCreateInfo alloc_ci{};

    if (is_upload_buffer) {
        // Host-visible buffer for CPU writes
        alloc_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        alloc_ci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_ci.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    } else {
        // Device-local buffer
        alloc_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        if (desc.initial_data) {
            alloc_ci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
        }
    }

    // Check for UMA (Unified Memory Architecture)
    bool is_uma = false;
    VkPhysicalDeviceMemoryProperties2 mem_props = Render->Get_Device_Memory_Properties();
    for (uint32_t heap_idx = 0; heap_idx < mem_props.memoryProperties.memoryHeapCount; ++heap_idx) {
        if ((mem_props.memoryProperties.memoryHeaps[heap_idx].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0)
            continue;
        for (uint32_t type_idx = 0; type_idx < mem_props.memoryProperties.memoryTypeCount; ++type_idx) {
            const VkMemoryType& mtype = mem_props.memoryProperties.memoryTypes[type_idx];
            if (mtype.heapIndex != heap_idx) continue;
            if (mtype.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                is_uma = true;
                break;
            }
        }
        if (is_uma) break;
    }

    if (is_uma) {
        alloc_ci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_ci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    // Create buffer
    Vk::Check(vmaCreateBuffer(vma_allocator, &buffer_ci, &alloc_ci, &buffer, &buffer_allocation, nullptr));

    // Query memory properties
    //
    vmaGetAllocationMemoryProperties(vma_allocator, buffer_allocation, &memory_properties);
    is_host_coherent = (memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
    is_host_visible = (memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    is_device_local = (memory_properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;

    // Query buffer device address (for shader access)
    //
    VkBufferDeviceAddressInfo addr_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };
    buffer_address = vkGetBufferDeviceAddress(device, &addr_info);

    // Persistent CPU mapping (if host-visible)
    //
    if (is_host_visible && (is_upload_buffer || is_uma)) {
        Vk::Check(vmaMapMemory(vma_allocator, buffer_allocation, &cpu_mapped_ptr));
        is_persistently_mapped = true;
        TRACE("Vk_Buffer: Persistently mapped host-visible buffer at %p", cpu_mapped_ptr);
    }

    // Handle initial data
    //
    if (desc.initial_data && desc.size > 0) {
        if (is_host_visible && (is_upload_buffer || is_uma)) {
            // Direct copy to persistently mapped buffer
            memcpy(cpu_mapped_ptr, desc.initial_data, desc.size);

            if (!is_host_coherent) {
                vmaFlushAllocation(vma_allocator, buffer_allocation, 0, VK_WHOLE_SIZE);
            }
            TRACE("[Vk_Buffer] Initialized host-visible buffer with %zu bytes", desc.size);
        } else if (is_device_local) {
            // Device-local: create staging buffer for initial data transfer
            Create_Staging_Buffer(Render, desc.size);

            // Copy data to staging
            memcpy(staging_cpu_ptr, desc.initial_data, desc.size);
            if (!is_host_coherent) {
                vmaFlushAllocation(vma_allocator, staging_buffer_allocation, 0, VK_WHOLE_SIZE);
            }

            // Transfer from staging to device-local using command guard
            {
                Command_Buffer_Guard cmd_guard(
                    Render->Get_Device(),
                    Render->Get_Command_Pool(),
                    Render->Get_Queue()
                );

                VkBufferCopy copy_region{
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size = desc.size
                };
                vkCmdCopyBuffer(cmd_guard.Get(), staging_buffer, buffer, 1, &copy_region);
            }

            TRACE("[Vk_Buffer] Initialized device-local buffer with %zu bytes via staging", desc.size);
        }
    }

    TRACE("[Vk_Buffer] Buffer created: %s (%zu bytes)", desc.debug_name.data, desc.size);
}

// Vk_Buffer Staging Buffer Helper
//
template <typename T>
void Vk_Buffer<T>::Create_Staging_Buffer(Vk_Render *render, U64 size) {
    if (staging_buffer != VK_NULL_HANDLE) {
        return; // Already created
    }

    VmaAllocator vma_allocator = vma_allocator_copy;

    VkBufferCreateInfo buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo alloc_ci{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        .preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    VmaAllocationInfo alloc_info{};
    Vk::Check(vmaCreateBuffer(
        vma_allocator,
        &buffer_ci,
        &alloc_ci,
        &staging_buffer,
        &staging_buffer_allocation,
        &alloc_info
    ));

    staging_cpu_ptr = alloc_info.pMappedData;
    owns_staging_buffer = true;

    TRACE("[Vk_Buffer] Created staging buffer of size %llu bytes", size);
}

// Vk_Buffer Data upload
//
template <typename T>
void Vk_Buffer<T>::Upload_Data_To_Buffer(Vk_Render *render, dyn_vector<T>& data) {
    if (data.Size() == 0) {
        TRACE("[Vk_Buffer] Warning: Upload called with empty data");
        return;
    }

    U64 data_size = data.Size() * sizeof(T);
    if (data_size > description.size) {
        TRACE("[Vk_Buffer] ERROR: Data size (%llu bytes) exceeds buffer size (%llu bytes)",
              data_size, description.size);
        return;
    }

    // Host-visible, persistently mapped (integrated GPU / upload buffers)
    //
    if (is_persistently_mapped && cpu_mapped_ptr != nullptr) {
        TRACE("[Vk_Buffer] Direct memcpy to persistently mapped buffer");
        memcpy(cpu_mapped_ptr, data.Memory(), data_size);

        // Only flush if not coherent
        if (!is_host_coherent) {
            vmaFlushAllocation(vma_allocator_copy, buffer_allocation, 0, data_size);
        }

        stats.total_writes++;
        return;
    }

    // Case 2: Device-local buffer (discrete GPU) - use staging buffer
    //
    if (is_device_local) {
        TRACE("[Vk_Buffer] Copying to device-local buffer via staging");

        // Ensure staging buffer exists
        if (staging_buffer == VK_NULL_HANDLE) {
            Create_Staging_Buffer(render, data_size);
        }

        // Copy data to staging buffer
        memcpy(staging_cpu_ptr, data.Memory(), data_size);

        // Flush staging buffer if needed
        if (!is_host_coherent) {
            vmaFlushAllocation(vma_allocator_copy, staging_buffer_allocation, 0, data_size);
        }

        // Transfer from staging to device-local buffer using command buffer
        {
            Command_Buffer_Guard cmd_guard(
                render->Get_Device(),
                render->Get_Command_Pool(),
                render->Get_Queue()
            );

            VkBufferCopy region{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = data_size
            };
            vkCmdCopyBuffer(cmd_guard.Get(), staging_buffer, buffer, 1, &region);

            // Implicit submit and wait in destructor
        }

        stats.total_writes++;
        stats.total_gpu_transfers++;
        return;
    }

    // Case 3: Fallback for host-visible non-persistent (shouldn't normally happen)
    //
    TRACE("[Vk_Buffer] Warning: Using slow host-visible non-persistent path");
    void* mapped_ptr = nullptr;
    vmaMapMemory(vma_allocator_copy, buffer_allocation, &mapped_ptr);
    {
        memcpy(mapped_ptr, data.Memory(), data_size);

        if (!is_host_coherent) {
            vmaFlushAllocation(vma_allocator_copy, buffer_allocation, 0, data_size);
        }
    }
    vmaUnmapMemory(vma_allocator_copy, buffer_allocation);

    stats.total_writes++;
}

// Vk_Buffer Destructor
//
template <typename T>
Vk_Buffer<T>::~Vk_Buffer ()
{
    if (vma_allocator_copy == VK_NULL_HANDLE) return;

    // Unmap CPU memory if it exists
    if (is_persistently_mapped && cpu_mapped_ptr != nullptr) {
        vmaUnmapMemory(vma_allocator_copy, buffer_allocation);
        cpu_mapped_ptr = nullptr;
        is_persistently_mapped = false;
    }

    // Destroy staging buffer if owned
    if (owns_staging_buffer && staging_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(vma_allocator_copy, staging_buffer, staging_buffer_allocation);
        staging_buffer = VK_NULL_HANDLE;
        staging_buffer_allocation = nullptr;
        staging_cpu_ptr = nullptr;
    }

    // Destroy main buffer
    if (buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(vma_allocator_copy, buffer, buffer_allocation);
        buffer = VK_NULL_HANDLE;
        buffer_allocation = nullptr;
    }

    // Cleanup vectors
    frame_states.Destroy();

    TRACE("Vk_Buffer: Destroyed buffer");
}

//  Frame Loop Implementation
//

void Vk_Render::Begin_Frame() {
    U32 current_frame = frame_index;

    // SYNCHRONIZATION PHASE 1: Wait for GPU to finish with this frame's data
    // Without this wait, we'd be overwriting a command buffer the GPU is still using!
    //
    TRACE("[Frame %u] Waiting for GPU to finish previous work...", current_frame);
    Vk::Check(vkWaitForFences(
        device,
        1,
        &fences[current_frame],
        VK_TRUE,
        UINT64_MAX  // Wait indefinitely (safe for games)
    ));
    TRACE("[Frame %u] GPU finished, command buffer is safe to reuse", current_frame);

	if (update_swapchain) {
		Recreate_Swapchain();
	}

    // ACQUISITION PHASE: Get the next swapchain image to render into
    //
    VkResult acquire_result = vkAcquireNextImageKHR(
        device,
        swapchain,
        UINT64_MAX,
        present_semaphores[current_frame],  // Signaled when image is ready for rendering
        VK_NULL_HANDLE,                      // No fence needed
        &image_index
    );

    // Handle swapchain out-of-date
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        TRACE("[Frame %u] Swapchain out of date, recreating now", current_frame);
        Recreate_Swapchain();
        acquire_result = vkAcquireNextImageKHR(
            device,
            swapchain,
            UINT64_MAX,
            present_semaphores[current_frame],
            VK_NULL_HANDLE,
            &image_index
        );
    }
	if (acquire_result == VK_SUBOPTIMAL_KHR) {
		update_swapchain = true;
	} else {
		Check_Swapchain(acquire_result);
	}

    TRACE("[Frame %u] Acquired swapchain image %u", current_frame, image_index);

    // RECORDING PHASE: Begin command buffer for this frame
    //
    Begin_Command_Buffer();
}

void Vk_Render::End_Frame() {
    U32 current_frame = frame_index;

    // END RECORDING: Finish command buffer
    //
    End_Command_Buffer();

    // SYNCHRONIZATION PHASE 2: Reset fence before submission
    // Reset fence so it's in unsignaled state before submission
    //
    TRACE("[Frame %u] Resetting fence for next submission", current_frame);
    Vk::Check(vkResetFences(device, 1, &fences[current_frame]));

    // SUBMISSION PHASE: Submit commands to GPU
    //
    // The GPU will signal the fence when it's done with this frame's work
    // We place an explicit image barrier before rendering starts. Waiting at
    // ALL_COMMANDS guarantees that barrier cannot execute before acquire.
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &present_semaphores[current_frame],  // Wait for acquire
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffers[current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &render_semaphores[image_index]
    };

    TRACE("[Frame %u] Submitting command buffer to GPU", current_frame);
    Vk::Check(vkQueueSubmit(
        queue,
        1,
        &submit_info,
        fences[current_frame]  // GPU signals this fence when done
    ));

    // PRESENTATION PHASE: Present to screen
    //
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_semaphores[image_index],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &image_index
    };

    VkResult present_result = vkQueuePresentKHR(queue, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR ||
        present_result == VK_SUBOPTIMAL_KHR) {
        TRACE("[Frame %u] Swapchain needs recreation after present", current_frame);
        update_swapchain = true;
    }
    Check_Swapchain(present_result);

    TRACE("[Frame %u] Frame presented to screen", current_frame);

    // FRAME ADVANCE: Move to next frame slot
    //
    Update_Frame_Idx();
    TRACE("[Frame] Advanced to frame %u", frame_index);
}

void Vk_Render::Record_Frame(std::function<void()> record_callback) {
    Begin_Frame();
    record_callback();  // User's draw calls go here
    End_Frame();
}

void Vk_Render::Render_Loop() {
    TRACE("[Render] Render_Loop is a stub - use Begin_Frame/End_Frame or Record_Frame instead");

    // User should call Begin_Frame, record draw commands, and End_Frame in a loop
    // Example:
    // while (app_running) {
    //     renderer.Begin_Frame();
    //     renderer.Begin_Rendering();
    //     // ... draw calls ...
    //     renderer.End_Rendering();
    //     renderer.End_Frame();
    // }
}

// Rendering Commands Implementation
//

void Vk_Render::Begin_Rendering(VkClearColorValue clear_color) {
    VkCommandBuffer cmd = Get_Current_Cmd_Buffer();

    TRACE("[Rendering] Begin rendering with clear color [%.2f, %.2f, %.2f, %.2f]",
          clear_color.float32[0], clear_color.float32[1],
          clear_color.float32[2], clear_color.float32[3]);

    VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (Has_Stencil_Component(depth_format)) {
        depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VkPipelineStageFlags2 depth_src_stage =
        (depth_image_layout == VK_IMAGE_LAYOUT_UNDEFINED)
            ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT
            : (VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);

    // Transition attachments from their tracked layouts into renderable layouts.
    VkImageMemoryBarrier2 swapchain_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        .oldLayout = swapchain_image_layouts[image_index],
        .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = Get_Current_Swapchain_Image(),
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    VkImageMemoryBarrier2 depth_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = depth_src_stage,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        .oldLayout = depth_image_layout,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = depth_image,
        .subresourceRange = {depth_aspect, 0, 1, 0, 1}
    };

    VkImageMemoryBarrier2 barriers[] = {swapchain_barrier, depth_barrier};
    VkDependencyInfo dependency_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = barriers
    };

    vkCmdPipelineBarrier2(cmd, &dependency_info);
    swapchain_image_layouts[image_index] = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Setup color attachment
    VkRenderingAttachmentInfo color_attachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = Get_Current_Swapchain_Image_View(),
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = clear_color}
    };

    // Setup depth attachment
    VkRenderingAttachmentInfo depth_attachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.depthStencil = {1.0f, 0}}
    };

    // Begin rendering
    VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {.offset = {0, 0}, .extent = {(u32)window.Width, (u32)window.Height}},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
        .pDepthAttachment = &depth_attachment
    };

    vkCmdBeginRendering(cmd, &rendering_info);
}

void Vk_Render::End_Rendering() {
    VkCommandBuffer cmd = Get_Current_Cmd_Buffer();

    TRACE("[Rendering] End rendering");

    vkCmdEndRendering(cmd);

    // Transition swapchain image back to PRESENT_SRC for presentation
    VkImageMemoryBarrier2 present_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .dstAccessMask = 0,
        .oldLayout = swapchain_image_layouts[image_index],
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = Get_Current_Swapchain_Image(),
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    VkDependencyInfo dependency_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &present_barrier
    };

    vkCmdPipelineBarrier2(cmd, &dependency_info);
    swapchain_image_layouts[image_index] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

void Vk_Render::Set_Viewport(F32 x, F32 y, F32 width, F32 height, F32 min_depth, F32 max_depth) {
    VkCommandBuffer cmd = Get_Current_Cmd_Buffer();

    // Use window dimensions if not specified
    if (width == 0) width = static_cast<F32>(window.Width);
    if (height == 0) height = static_cast<F32>(window.Height);

    VkViewport viewport{
        .x = x,
        .y = y,
        .width = width,
        .height = height,
        .minDepth = min_depth,
        .maxDepth = max_depth
    };

    TRACE("[Rendering] Set viewport [%.1f, %.1f] size [%.1f x %.1f]", x, y, width, height);

    vkCmdSetViewport(cmd, 0, 1, &viewport);
}

void Vk_Render::Set_Scissor(i32 x, i32 y, u32 width, u32 height) {
    VkCommandBuffer cmd = Get_Current_Cmd_Buffer();

    // Use window dimensions if not specified
    if (width == 0) width = (u32)window.Width;
    if (height == 0) height = (u32)window.Height;

    VkRect2D scissor{
        .offset = {x, y},
        .extent = {width, height}
    };

    TRACE("[Rendering] Set scissor [%d, %d] size [%u x %u]", x, y, width, height);

    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void Vk_Render::Bind_Pipeline(VkPipeline pipeline, VkPipelineLayout pipeline_layout) {
    VkCommandBuffer cmd = Get_Current_Cmd_Buffer();

    current_pipeline = pipeline;
    current_pipeline_layout = pipeline_layout;

    TRACE("[Rendering] Bind pipeline");

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void Vk_Render::Bind_Descriptor_Sets(VkPipelineLayout pipeline_layout, u32 set_index,
                                     u32 descriptor_set_count, const VkDescriptorSet* descriptor_sets) {
    VkCommandBuffer cmd = Get_Current_Cmd_Buffer();

    TRACE("[Rendering] Bind descriptor sets (count: %u, set index: %u)", descriptor_set_count, set_index);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                           set_index, descriptor_set_count, descriptor_sets, 0, nullptr);
}

void Vk_Render::Bind_Vertex_Buffer(VkBuffer buffer, VkDeviceSize offset) {
    VkCommandBuffer cmd = Get_Current_Cmd_Buffer();

    TRACE("[Rendering] Bind vertex buffer at offset %lu", offset);

    vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);
}

void Vk_Render::Bind_Index_Buffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType index_type) {
    VkCommandBuffer cmd = Get_Current_Cmd_Buffer();

    TRACE("[Rendering] Bind index buffer at offset %lu (type: %u)", offset, index_type);

    vkCmdBindIndexBuffer(cmd, buffer, offset, index_type);
}

void Vk_Render::Draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance) {
    VkCommandBuffer cmd = Get_Current_Cmd_Buffer();

    TRACE("[Rendering] Draw %u vertices (%u instances)", vertex_count, instance_count);

    vkCmdDraw(cmd, vertex_count, instance_count, first_vertex, first_instance);
}

void Vk_Render::Draw_Indexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance) {
    VkCommandBuffer cmd = Get_Current_Cmd_Buffer();

    TRACE("[Rendering] Draw indexed %u indices (%u instances)", index_count, instance_count);

    vkCmdDrawIndexed(cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void Vk_Render::Push_Constants(VkPipelineLayout pipeline_layout, VkShaderStageFlags stage_flags,
                              u32 offset, u32 size, const void* data) {
    VkCommandBuffer cmd = Get_Current_Cmd_Buffer();

    TRACE("[Rendering] Push constants (size: %u, offset: %u)", size, offset);

    vkCmdPushConstants(cmd, pipeline_layout, stage_flags, offset, size, data);
}
