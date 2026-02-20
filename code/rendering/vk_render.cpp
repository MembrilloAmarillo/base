#include "vk_render.h"
#include <algorithm>
#include <vector>

/////////////////////////////////////////////////////////////////////
//        Vk_Descriptor::Builder::Build() Implementation           //
/////////////////////////////////////////////////////////////////////

Vk_Descriptor Vk_Descriptor::Builder::Build() {
    if (bindings.Length() == 0) {
        fprintf(stderr, "Error: Descriptor builder has no bindings\n");
        exit(EXIT_FAILURE);
    }
    return Vk_Descriptor(allocator, device, bindings);
}

// Debug utils callback implementation
VKAPI_ATTR VkBool32 VKAPI_CALL VkDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
											   VkDebugUtilsMessageTypeFlagsEXT messageType,
											   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
											   void* pUserData) {
	(void)messageSeverity; (void)messageType; (void)pUserData;
	fprintf(stderr, "[VULKAN DEBUG] %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

/////////////////////////////////////////////////////////////////////
//             Class Vk_Texture: Load_From_Ktx                     //
/////////////////////////////////////////////////////////////////////

VkDescriptorImageInfo Vk_Texture::Load_From_Ktx( Vk_Render *render, const char *path ) {

    VkDevice                   device = render->Get_Device();
    VkQueue                     queue = render->Get_Queue();
    VkCommandPool        command_pool = render->Get_Command_Pool();
    VmaAllocator        vma_allocator = render->Get_Vma_Allocator();
    //Vk_Bindless_Table  bindless_table = render->Get_Bindless_Table();

	ktxTexture* ktx_texture = nullptr;
	ktxResult result = ktxTexture_CreateFromNamedFile(path, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
	if (result != KTX_SUCCESS) {
		printf("ERROR: Failed to load KTX texture from '%s': %s\n", path, ktxErrorString(result));
		return {};
	}
	VkImageCreateInfo tex_image_ci{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = ktxTexture_GetVkFormat(ktx_texture),
		.extent = {.width = ktx_texture->baseWidth, .height = ktx_texture->baseHeight, .depth = 1 },
		.mipLevels = ktx_texture->numLevels,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL, .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};


	VmaAllocationCreateInfo texImageAllocCI{ .usage = VMA_MEMORY_USAGE_AUTO };
	Vk::Check(vmaCreateImage(vma_allocator, &tex_image_ci, &texImageAllocCI, &image, &allocation, nullptr));
	VkImageViewCreateInfo texVewCI{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = tex_image_ci.format,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = ktx_texture->numLevels,
			.layerCount = 1
		}
	};


	Vk::Check(vkCreateImageView(device, &texVewCI, nullptr, &image_view));


	VkBuffer img_src_buffer {};
	VmaAllocation img_src_allocation {};

	VkBufferCreateInfo img_src_buff_ci {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = static_cast<U32>(ktx_texture->dataSize),
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	};

	VmaAllocationCreateInfo img_src_alloc_ci {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	Vk::Check(vmaCreateBuffer(vma_allocator, &img_src_buff_ci, &img_src_alloc_ci, &img_src_buffer, &img_src_allocation, nullptr));


    void* img_src_buffer_ptr { nullptr };
    Vk::Check(vmaMapMemory(vma_allocator, img_src_allocation, &img_src_buffer_ptr));


    /* copy into the mapped pointer, not the VkBuffer handle */
    memcpy(img_src_buffer_ptr, ktx_texture->pData, ktx_texture->dataSize);
    /* If the allocation is not host-coherent we must flush the mapped memory so the device
       sees the written data before we submit the command buffer. VMA provides a helper. */
    vmaFlushAllocation(vma_allocator, img_src_allocation, 0, VK_WHOLE_SIZE);
	{
	   Command_Buffer_Guard command_buffer_guard(device, command_pool, queue);

	   auto cb_one_time = command_buffer_guard.Get();

    	VkImageMemoryBarrier2 barrier_tex_image {
    		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
    		.srcAccessMask = VK_ACCESS_2_NONE,
    		.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    		.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    		.image = image,
    		.subresourceRange = {
    			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    			.levelCount = ktx_texture->numLevels, .layerCount = 1
    		}
    	};
    	VkDependencyInfo barrier_tex_info {
    		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    		.imageMemoryBarrierCount = 1,
    		.pImageMemoryBarriers = &barrier_tex_image
    	};
    	vkCmdPipelineBarrier2(cb_one_time, &barrier_tex_info);
    	dyn_vector<VkBufferImageCopy> copy_regions =
    	   dyn_vector<VkBufferImageCopy>::Init(allocator, ktx_texture->numLevels);

        // Validate offsets and build copy regions. ktxTexture provides image sizes
        // and offsets; ensure they are inside the loaded pData buffer.
        for (auto j = 0; j < ktx_texture->numLevels; j++) {
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
            region.imageSubresource.mipLevel = (uint32_t)j;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = mipWidth;
            region.imageExtent.height = mipHeight;
            region.imageExtent.depth = 1;
            /* tightly packed buffer */
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            copy_regions.AppendByCopy(region);
        }

    	vkCmdCopyBufferToImage(
    		cb_one_time,
    		img_src_buffer,
    		image,
    		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    		static_cast<uint32_t>(copy_regions.Length()),
    		copy_regions.Memory()
    	);

    	VkImageMemoryBarrier2 barrier_tex_read {
    		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    		.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
    		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    		.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    		.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
    		.image = image,
    		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = ktx_texture->numLevels, .layerCount = 1 }
    	};

    	barrier_tex_info.pImageMemoryBarriers = &barrier_tex_read;
    	vkCmdPipelineBarrier2(cb_one_time, &barrier_tex_info);
	}
	vmaUnmapMemory(vma_allocator, img_src_allocation);
	vmaDestroyBuffer(vma_allocator, img_src_buffer, img_src_allocation);
	// Sampler
	Create_Sampler(device, static_cast<f32>(ktx_texture->numLevels));
	ktxTexture_Destroy(ktx_texture);

	layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	return {
		.sampler = sampler,
		.imageView = image_view,
		.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
	};
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
    window = SurfaceCreateWindow(w, h);
    //volkInitialize();

	// Instance
	VkApplicationInfo app_info{
	   .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
	   .pApplicationName = "Vulkan Instance",
	   .apiVersion = VK_API_VERSION_1_3
    };
	U32 instance_extension_count{ 0 };
	char const* const* sdl_exts = SDL_Vulkan_GetInstanceExtensions(&instance_extension_count);
	std::vector<const char*> extensions;
	extensions.reserve(instance_extension_count + 1);
	for (U32 i = 0; i < instance_extension_count; ++i) extensions.push_back(sdl_exts[i]);
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	const char* validation_layer = "VK_LAYER_KHRONOS_validation";

	VkInstanceCreateInfo instanceCI{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = 1,
		.ppEnabledLayerNames = &validation_layer,
		.enabledExtensionCount = static_cast<U32>(extensions.size()),
		.ppEnabledExtensionNames = extensions.data(),
	};

	Check(vkCreateInstance(&instanceCI, nullptr, &instance));
	//volkLoadInstance(instance);
	auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	// Create debug utils messenger
	VkDebugUtilsMessengerCreateInfoEXT debug_ci {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = VkDebugCallback,
		.pUserData = nullptr
	};

	// Use extension function (populated by volkLoadInstance)
	if (vkCreateDebugUtilsMessengerEXT) {
		Vk::Check(vkCreateDebugUtilsMessengerEXT(instance, &debug_ci, nullptr, &debug_messenger));
	}

	// Physical device
	//
	U32 device_count{ 0 };
	Check(vkEnumeratePhysicalDevices(instance, &device_count, nullptr));
	dyn_vector<VkPhysicalDevice> devices = dyn_vector<VkPhysicalDevice>::Init(Alloc, device_count);
	// devices.Destroy();
	Check(vkEnumeratePhysicalDevices(instance, &device_count, devices.Memory()));
	U32 device_index{ 0 };
	//if (argc > 1) {
	//	device_index = std::stoi(argv[1]);
	//	assert(device_index < device_count);
	//}
	// Find a queue family for graphics
	uint32_t queue_family_count{ 0 };
	vkGetPhysicalDeviceQueueFamilyProperties(devices.At(device_index), &queue_family_count, nullptr);
	dyn_vector<VkQueueFamilyProperties> queue_families = dyn_vector<VkQueueFamilyProperties>::Init(Alloc, queue_family_count);
	// queue_families.Destroy();
	vkGetPhysicalDeviceQueueFamilyProperties(devices.At(device_index), &queue_family_count, queue_families.Memory());
	uint32_t queue_family{ 0 };
	for (size_t i = 0; i < queue_families.Capacity(); i++) {
		if (queue_families.At(i).queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			queue_family = i;
			break;
		}
	}

	// Logical device
    const float qfpriorities{ 1.0f };
    VkDeviceQueueCreateInfo queue_ci{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family,
        .queueCount = 1,
        .pQueuePriorities = &qfpriorities
    };
	VkPhysicalDeviceVulkan12Features enabled_vk12_features {
	   .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
	   .pNext = nullptr,
	   .descriptorIndexing = true,
	   .shaderSampledImageArrayNonUniformIndexing = true,
	   .descriptorBindingUniformBufferUpdateAfterBind = true,
	   .descriptorBindingSampledImageUpdateAfterBind = true,
	   .descriptorBindingStorageImageUpdateAfterBind = true,
	   .descriptorBindingStorageBufferUpdateAfterBind = true,
	   .descriptorBindingPartiallyBound = true,
	   .descriptorBindingVariableDescriptorCount = true,
	   .runtimeDescriptorArray = true,
	   .bufferDeviceAddress = true
	};
	VkPhysicalDeviceVulkan13Features enabled_vk13_features{
	   .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
	   .pNext = &enabled_vk12_features,
	   .synchronization2 = true,
	   .dynamicRendering = true
	};
	const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	const VkPhysicalDeviceFeatures enabled_vk10_features{ .samplerAnisotropy = VK_TRUE };
	VkDeviceCreateInfo device_ci{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &enabled_vk13_features,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queue_ci,
		.enabledExtensionCount = static_cast<U32>(ArrayCount(device_extensions)),
		.ppEnabledExtensionNames = device_extensions,
		.pEnabledFeatures = &enabled_vk10_features
	};

	physical_device = devices[device_index];
    Check(vkCreateDevice(devices.At(device_index), &device_ci, nullptr, &device));
    // Make sure volk resolves device-level function pointers for this VkDevice.
    // Without this, calls that depend on device dispatch may crash with an
    // invalid dispatch pointer inside the loader trampoline.
    //volkLoadDevice(device);
    vkGetDeviceQueue(device, queue_family, 0, &queue);

	// VMA
	//
	VmaVulkanFunctions vk_functions{
	   .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
	   .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
	   .vkCreateImage = vkCreateImage
	};
	VmaAllocatorCreateInfo allocator_ci{
	   .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
	   .physicalDevice = devices[device_index],
	   .device = device,
	   .pVulkanFunctions = &vk_functions,
	   .instance = instance
	};
	Check(vmaCreateAllocator(&allocator_ci, &allocator));

	// Swap chain
	//
	Check(SDL_Vulkan_CreateSurface(window.Win, instance, nullptr, &window.Surface));
	VkSurfaceCapabilitiesKHR surface_caps{};
	Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices.At(device_index), window.Surface, &surface_caps));

	const VkFormat image_format { VK_FORMAT_B8G8R8A8_SRGB };
	auto pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	if( surface_caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ) {
		pre_transform = surface_caps.currentTransform;
	}
	surface_caps.currentExtent.width  = window.Width;
	surface_caps.currentExtent.height = window.Height;
	VkSwapchainCreateInfoKHR swapchain_ci{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = window.Surface,
		.minImageCount = surface_caps.minImageCount + 1,
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
	swapchain_image_views = dyn_vector<VkImageView>::Init(Alloc, image_count);
	for (auto i = 0; i < image_count; i++) {
		VkImageViewCreateInfo view_ci{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = swapchain_images.At(i), .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = image_format, .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 } };
		Check(vkCreateImageView(device, &view_ci, nullptr, swapchain_image_views.Memory() + i));
	}
	printf("Created swapchain\n");
	// Depth attachment
    VkFormat depth_format_list[] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	for ( int i = 0; i < ArrayCount(depth_format_list); i++ ) {
        VkFormat format = depth_format_list[i];
        VkFormatProperties2 format_properties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
        vkGetPhysicalDeviceFormatProperties2(devices[device_index], format, &format_properties);
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
    VkImageViewCreateInfo depth_view_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depth_format,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
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
	for (auto i = 0; i < max_frames_in_flight; i++) {
		Check(vkCreateFence(device, &fence_ci, nullptr, &fences[i]));
		Check(vkCreateSemaphore(device, &semaphore_ci, nullptr, &present_semaphores[i]));
	}
	render_semaphores = dyn_vector<VkSemaphore>::Init(Alloc, swapchain_images.Capacity());
	for (auto i = 0; i < render_semaphores.Capacity(); i++) {
        auto semaphore = (render_semaphores.Memory() + i);
        Check(vkCreateSemaphore(device, &semaphore_ci, nullptr, semaphore));
	}

	// Command pool
	//
	VkCommandPoolCreateInfo command_pool_ci{
	   .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
	   .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
	   .queueFamilyIndex = queue_family
	};
	Check(vkCreateCommandPool(
	   device,
	   &command_pool_ci,
	   nullptr,
	   &command_pool
    ));
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
	slang_target = std::to_array<slang::TargetDesc>({
			{
				.format = SLANG_SPIRV,
				.profile = slang_global_session->findProfile("spirv_1_4")
			}
		});
	slang_options = std::to_array<slang::CompilerOptionEntry>({
		{
			slang::CompilerOptionName::EmitSpirvDirectly,
			{slang::CompilerOptionValueKind::Int, 1}
		}
	});

	slang_session_description = {
		.targets = slang_target.data(),
		.targetCount = SlangInt(slang_target.size()),
		.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
		.compilerOptionEntries = slang_options.data(),
		.compilerOptionEntryCount = U32(slang_options.size())
	};

	// Initialize Bindless Descriptor Table
	//
	bindless_table.Init(Alloc, device);
	TRACE("[Init] Bindless descriptor table initialized");

	// ========================================================================
	// Initialize Image Layouts (ONE-TIME, at startup)
	// ========================================================================
	// Swapchain images must be transitioned from UNDEFINED to ATTACHMENT_OPTIMAL
	// Depth image must be transitioned from UNDEFINED to DEPTH_ATTACHMENT_OPTIMAL
	// This is done once during initialization using a temporary command buffer
	{
		TRACE("[Init] Transitioning image layouts...");

		// Use Command_Buffer_Guard for safe one-time command buffer
		Command_Buffer_Guard cmd_guard(device, command_pool, queue);

		// Transition ALL swapchain images to ATTACHMENT_OPTIMAL
		for (U32 i = 0; i < swapchain_images.Length(); ++i) {
			VkImageMemoryBarrier2 image_barrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
				.srcAccessMask = 0,
				.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = swapchain_images[i],
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			VkDependencyInfo dependency_info{
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &image_barrier
			};

			vkCmdPipelineBarrier2(cmd_guard.Get(), &dependency_info);
		}

		// Transition depth image to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		// Must use DEPTH_STENCIL layout when both DEPTH and STENCIL aspects are present
		VkImageMemoryBarrier2 depth_barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = depth_image,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		VkDependencyInfo depth_dependency_info{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &depth_barrier
		};

		vkCmdPipelineBarrier2(cmd_guard.Get(), &depth_dependency_info);

		TRACE("[Init] Image layouts transitioned successfully");
		// Command_Buffer_Guard destructor submits and waits automatically
	}
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

    VkDevice device = render->Get_Device();
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
            VkMappedMemoryRange range{
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = buffer_allocation->GetMemory(),
                .offset = 0,
                .size = data_size
            };
            vkFlushMappedMemoryRanges(render->Get_Device(), 1, &range);
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
            VkMappedMemoryRange range{
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = staging_buffer_allocation->GetMemory(),
                .offset = 0,
                .size = data_size
            };
            vkFlushMappedMemoryRanges(render->Get_Device(), 1, &range);
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
            VkMappedMemoryRange range{
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = buffer_allocation->GetMemory(),
                .offset = 0,
                .size = data_size
            };
            vkFlushMappedMemoryRanges(render->Get_Device(), 1, &range);
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
        TRACE("[Frame %u] Swapchain out of date, will recreate on next frame", current_frame);
        update_swapchain = true;
        // For now, just retry acquiring
        vkAcquireNextImageKHR(
            device,
            swapchain,
            UINT64_MAX,
            present_semaphores[current_frame],
            VK_NULL_HANDLE,
            &image_index
        );
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
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &present_semaphores[current_frame],  // Wait for acquire
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffers[current_frame],
        .signalSemaphoreCount = 0,  // Can add render_semaphores if needed
        .pSignalSemaphores = nullptr
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
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
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

    // Transition swapchain image to ATTACHMENT_OPTIMAL if needed
    // (After present, driver may reset it to UNDEFINED)
    VkImageMemoryBarrier2 swapchain_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = Get_Current_Swapchain_Image(),
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    // Transition depth image to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    // Must use DEPTH_STENCIL layout when both DEPTH and STENCIL aspects are present
    VkImageMemoryBarrier2 depth_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = depth_image,
        .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1}
    };

    VkImageMemoryBarrier2 barriers[] = {swapchain_barrier, depth_barrier};
    VkDependencyInfo dependency_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = barriers
    };

    vkCmdPipelineBarrier2(cmd, &dependency_info);

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
        .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
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
