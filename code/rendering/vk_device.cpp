#include "vk_device.hpp"

Device::Device(const Instance& instance, const Device_Create_Info& info) {
    m_instance = &instance;
    m_mem_allocator = info.allocator;

    Create_Logical_Device(info);
    Retrieve_Queues();
    Create_Graphics_Command_Pool();

    if (info.enable_vma) {
        Initialize_Vma(instance);
    }
}

Device::Device(Device&& other) noexcept
    : m_device(other.m_device)
    , m_physical_device(other.m_physical_device)
    , m_graphics_queue(other.m_graphics_queue)
    , m_present_queue(other.m_present_queue)
    , m_compute_queue(other.m_compute_queue)
    , m_transfer_queue(other.m_transfer_queue)
    , m_graphics_queue_family(other.m_graphics_queue_family)
    , m_present_queue_family(other.m_present_queue_family)
    , m_compute_queue_family(other.m_compute_queue_family)
    , m_transfer_queue_family(other.m_transfer_queue_family)
    , m_graphics_command_pool(other.m_graphics_command_pool)
    , m_vma_allocator(other.m_vma_allocator)
    , m_instance(other.m_instance)
    , m_mem_allocator(other.m_mem_allocator) {

    other.m_device = VK_NULL_HANDLE;
    other.m_physical_device = VK_NULL_HANDLE;
    other.m_graphics_queue = VK_NULL_HANDLE;
    other.m_present_queue = VK_NULL_HANDLE;
    other.m_compute_queue = VK_NULL_HANDLE;
    other.m_transfer_queue = VK_NULL_HANDLE;
    other.m_graphics_command_pool = VK_NULL_HANDLE;
    other.m_vma_allocator = nullptr;
    other.m_instance = nullptr;
}

Device& Device::operator=(Device&& other) noexcept {
    if (this != &other) {
        // Clean up existing
        if (m_graphics_command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_graphics_command_pool, nullptr);
        }
        Shutdown_Vma();
        if (m_device != VK_NULL_HANDLE) {
            vkDestroyDevice(m_device, nullptr);
        }

        // Move from other
        m_device = other.m_device;
        m_physical_device = other.m_physical_device;
        m_graphics_queue = other.m_graphics_queue;
        m_present_queue = other.m_present_queue;
        m_compute_queue = other.m_compute_queue;
        m_transfer_queue = other.m_transfer_queue;
        m_graphics_queue_family = other.m_graphics_queue_family;
        m_present_queue_family = other.m_present_queue_family;
        m_compute_queue_family = other.m_compute_queue_family;
        m_transfer_queue_family = other.m_transfer_queue_family;
        m_graphics_command_pool = other.m_graphics_command_pool;
        m_vma_allocator = other.m_vma_allocator;
        m_instance = other.m_instance;
        m_mem_allocator = other.m_mem_allocator;

        // Invalidate other
        other.m_device = VK_NULL_HANDLE;
        other.m_physical_device = VK_NULL_HANDLE;
        other.m_graphics_queue = VK_NULL_HANDLE;
        other.m_present_queue = VK_NULL_HANDLE;
        other.m_compute_queue = VK_NULL_HANDLE;
        other.m_transfer_queue = VK_NULL_HANDLE;
        other.m_graphics_command_pool = VK_NULL_HANDLE;
        other.m_vma_allocator = nullptr;
        other.m_instance = nullptr;
        other.m_mem_allocator = nullptr;
    }
    return *this;
}

Device::~Device() {
    // Wait for device to finish all work
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }

    if (m_graphics_command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_graphics_command_pool, nullptr);
    }

    Shutdown_Vma();

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }
}

void Device::Create_Logical_Device(const Device_Create_Info& info) {

    // Physical device
	//
	U32 device_count{ 0 };
	Check(vkEnumeratePhysicalDevices(m_instance->Get_Handle(), &device_count, nullptr));
	dyn_vector<VkPhysicalDevice> devices = dyn_vector<VkPhysicalDevice>::Init(m_mem_allocator, device_count);
	// devices.Destroy();
	Check(vkEnumeratePhysicalDevices(m_instance->Get_Handle(), &device_count, devices.Memory()));
	U32 device_index{ 0 };
	int max_score = 0;
	for( U32 i = 0 ; i < devices.Capacity(); i++ ) {
	   int score = Score_Physical_Device(devices[i], info.surface, info.device_extensions);
	   if( max_score < score ) {
            max_score = score;
            device_index = i;
	   }
	}

	m_physical_device = devices[device_index];

    Queue_Family_Indices indices = Find_Queue_Families(m_physical_device, info.surface);

    if (!indices.Is_Complete()) {
        std::runtime_error("Selected physical device does not support required graphics queue families");
    }

    // Store indices for later
    m_graphics_queue_family = indices.graphics_family.value();
    m_present_queue_family = indices.present_family.value();
    if (indices.compute_family.has_value()) {
        m_compute_queue_family = indices.compute_family.value();
    }
    if (indices.transfer_family.has_value()) {
        m_transfer_queue_family = indices.transfer_family.value();
    }

    // Create unique queue create infos (graphics and present might be same family)
    std::set<uint32_t> unique_queue_families = {
        m_graphics_queue_family,
        m_present_queue_family
    };
    if (m_compute_queue_family != UINT32_MAX) {
        unique_queue_families.insert(m_compute_queue_family);
    }
    if (m_transfer_queue_family != UINT32_MAX) {
        unique_queue_families.insert(m_transfer_queue_family);
    }

    float queue_priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

    for (uint32_t queue_family : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    // Device features

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

    VkPhysicalDeviceFeatures device_features{};
    vkGetPhysicalDeviceFeatures(m_physical_device, &device_features);
    device_features.samplerAnisotropy = VK_TRUE;

    // Device create info
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = static_cast<uint32_t>(info.device_extensions.size());
    create_info.ppEnabledExtensionNames = info.device_extensions.data();
    create_info.pNext = &enabled_vk13_features;

    // Validation layers (deprecated for device, but included for compatibility)
    if (m_instance && m_instance->Validation_Enabled()) {
        const char* validation_layer = "VK_LAYER_KHRONOS_validation";
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = &validation_layer;
    }

    // pNext chain for modern features
    //create_info.pNext = info.p_next_chain;

    if (vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    } else {
        printf("[Vulkan Info] Created logical device\n");
    }

    VkPhysicalDeviceMemoryProperties mem_props = Get_Memory_Properties();

    // UMA detection heuristic:
    // In UMA (Unified Memory Architecture), there's typically:
    // 1. Only ONE heap that is DEVICE_LOCAL | HOST_VISIBLE
    // 2. OR multiple heaps but all are HOST_VISIBLE

    uint32_t device_local_host_visible_count = 0;
    uint32_t device_local_only_count = 0;

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        VkMemoryPropertyFlags flags = mem_props.memoryTypes[i].propertyFlags;

        if (mem_props.memoryHeapCount == 1) {
            if ((mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
                is_uma = true;
            }
        }

        bool device_local = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
        bool host_visible = (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;

        if (device_local && host_visible) {
            device_local_host_visible_count++;
        } else if (device_local && !host_visible) {
            device_local_only_count++;
        }
    }

    // True UMA: device-local memory is also host-visible (no separate VRAM)
    // Or: no device-local-only memory types exist
    is_uma = is_uma | (device_local_only_count == 0) && (device_local_host_visible_count > 0);
}

void Device::Retrieve_Queues() {
    vkGetDeviceQueue(m_device, m_graphics_queue_family, 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, m_present_queue_family, 0, &m_present_queue);

    if (m_compute_queue_family != UINT32_MAX) {
        vkGetDeviceQueue(m_device, m_compute_queue_family, 0, &m_compute_queue);
    }
    if (m_transfer_queue_family != UINT32_MAX) {
        vkGetDeviceQueue(m_device, m_transfer_queue_family, 0, &m_transfer_queue);
    }
}

void Device::Create_Graphics_Command_Pool() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = m_graphics_queue_family;

    if (vkCreateCommandPool(m_device, &pool_info, nullptr, &m_graphics_command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics command pool");
    }
}

VkCommandPool Device::Create_Command_Pool(uint32_t queue_family_index, VkCommandPoolCreateFlags flags) {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = flags;
    pool_info.queueFamilyIndex = queue_family_index;

    VkCommandPool pool;
    if (vkCreateCommandPool(m_device, &pool_info, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
    return pool;
}

void Device::Destroy_Command_Pool(VkCommandPool pool) {
    vkDestroyCommandPool(m_device, pool, nullptr);
}

VkCommandBuffer Device::Allocate_Command_Buffer(VkCommandPool pool, VkCommandBufferLevel level) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pool;
    alloc_info.level = level;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer buffer;
    if (vkAllocateCommandBuffers(m_device, &alloc_info, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    }
    return buffer;
}

void Device::Free_Command_Buffer(VkCommandBuffer buffer, VkCommandPool pool) {
    vkFreeCommandBuffers(m_device, pool, 1, &buffer);
}

void Device::Submit_Immediate_Commands(std::function<void(VkCommandBuffer)>&& record_func) {
    // Allocate temporary command buffer
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = m_graphics_command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(m_device, &alloc_info, &command_buffer);

    // Begin recording
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    // Execute user recording function
    record_func(command_buffer);

    // End and submit
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(m_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphics_queue);

    // Cleanup
    vkFreeCommandBuffers(m_device, m_graphics_command_pool, 1, &command_buffer);
}

void Device::Initialize_Vma(const Instance& instance) {
    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.physicalDevice = m_physical_device;
    allocator_info.device = m_device;
    allocator_info.instance = instance.Get_Handle();
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;

    // Optional: custom memory callbacks
    // allocator_info.pAllocationCallbacks = nullptr;
    // allocator_info.pDeviceMemoryCallbacks = nullptr;

    if (vmaCreateAllocator(&allocator_info, reinterpret_cast<VmaAllocator*>(&m_vma_allocator)) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA allocator");
    }
}

void Device::Shutdown_Vma() {
    if (m_vma_allocator) {
        vmaDestroyAllocator(reinterpret_cast<VmaAllocator>(m_vma_allocator));
        m_vma_allocator = nullptr;
    }
}

VkPhysicalDeviceProperties Device::Get_Device_Properties() const {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m_physical_device, &properties);
    return properties;
}

VkPhysicalDeviceMemoryProperties Device::Get_Memory_Properties() const {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_properties);
    return mem_properties;
}

uint32_t Device::Find_Memory_Type(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties mem_properties = Get_Memory_Properties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}


// Static helpers

Queue_Family_Indices Device::Find_Queue_Families(VkPhysicalDevice device, VkSurfaceKHR surface) {
    Queue_Family_Indices indices;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    int i = 0;
    for (const auto& queue_family : queue_families) {
        // Graphics support
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
        }

        // Compute support (dedicated preferred)
        if (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            if (!indices.compute_family.has_value() ||
                !(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.compute_family = i;
            }
        }

        // Transfer support (dedicated preferred)
        if (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            if (!indices.transfer_family.has_value() ||
                !(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.transfer_family = i;
            }
        }

        // Present support
        if (surface != VK_NULL_HANDLE) {
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
            if (present_support) {
                indices.present_family = i;
            }
        }

        if (indices.Is_Complete()) {
            break;
        }

        i++;
    }

    return indices;
}

bool Device::Check_Device_Extension_Support(VkPhysicalDevice device, const std::vector<const char*>& extensions) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions(extensions.begin(), extensions.end());

    for (const auto& extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }

    return required_extensions.empty();
}

int Device::Score_Physical_Device(VkPhysicalDevice device, VkSurfaceKHR surface,
                                   const std::vector<const char*>& extensions) {
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    vkGetPhysicalDeviceFeatures(device, &device_features);

    int score = 0;

    // Discrete GPU bonus
    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Maximum texture size
    score += device_properties.limits.maxImageDimension2D;

    // Check required features and extensions
    if (!device_features.geometryShader) {
        return 0;
    }

    if (!Check_Device_Extension_Support(device, extensions)) {
        return 0;
    }

    Queue_Family_Indices indices = Find_Queue_Families(device, surface);
    if (!indices.Is_Complete()) {
        return 0;
    }

    // Dedicated compute/transfer bonus
    if (indices.compute_family != indices.graphics_family) {
        score += 100;
    }
    if (indices.transfer_family != indices.graphics_family) {
        score += 100;
    }

    return score;
}