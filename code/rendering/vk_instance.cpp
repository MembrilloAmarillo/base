#include "vk_instance.hpp"

Instance::Instance(const Create_Info& info) {
    Create_Instance(info);

    if (info.enable_validation && info.enable_debug_messenger) {
        Setup_Debug_Messenger();
    }
}

Instance::Instance(Instance&& other) noexcept
    : m_instance(other.m_instance)
    , m_debug_messenger(other.m_debug_messenger)
    , m_validation_enabled(other.m_validation_enabled)
    , m_debug_messenger_enabled(other.m_debug_messenger_enabled)
    , m_mem_allocator(other.m_mem_allocator) {

    other.m_instance = VK_NULL_HANDLE;
    other.m_debug_messenger = VK_NULL_HANDLE;
    other.m_validation_enabled = false;
    other.m_debug_messenger_enabled = false;
    other.m_mem_allocator = nullptr;
}

Instance& Instance::operator=(Instance&& other) noexcept {
    if (this != &other) {
        // Clean up existing
        Destroy_Debug_Messenger();
        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
        }

        // Move from other
        m_instance = other.m_instance;
        m_debug_messenger = other.m_debug_messenger;
        m_validation_enabled = other.m_validation_enabled;
        m_debug_messenger_enabled = other.m_debug_messenger_enabled;
        m_mem_allocator = other.m_mem_allocator;

        // Invalidate other
        other.m_instance = VK_NULL_HANDLE;
        other.m_debug_messenger = VK_NULL_HANDLE;
        other.m_validation_enabled = false;
        other.m_debug_messenger_enabled = false;
        other.m_mem_allocator = nullptr;
    }
    return *this;
}

Instance::~Instance() {
    Destroy_Debug_Messenger();
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }
}

void Instance::Create_Instance(const Create_Info& info) {
    // Application info
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = info.app_name;
    app_info.applicationVersion = info.app_version;
    app_info.pEngineName = "Custom Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = info.api_version;

    m_mem_allocator = info.mem_allocator;

    // Instance create info
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    // Extensions (surface + debug utilities)
    std::vector<const char*> extensions = info.extensions;
    if (info.enable_validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    // Validation layers
    std::vector<const char*> layers;
    if (info.enable_validation) {
        if (!Check_Validation_Layer_Support()) {
            throw std::runtime_error("Validation layers requested but not available");
        }
        layers.push_back("VK_LAYER_KHRONOS_validation");
        m_validation_enabled = true;
    }

    create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.data();

    // Debug messenger create info (must be in pNext for instance creation)
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    if (info.enable_validation && info.enable_debug_messenger) {
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = Debug_Callback;
        debug_create_info.pUserData = nullptr;

        create_info.pNext = &debug_create_info;
    }

    // Create instance
    VkResult result = vkCreateInstance(&create_info, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void Instance::Setup_Debug_Messenger() {
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = Debug_Callback;
    create_info.pUserData = nullptr;

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

    if (func && func(m_instance, &create_info, nullptr, &m_debug_messenger) == VK_SUCCESS) {
        m_debug_messenger_enabled = true;
    }
}

void Instance::Destroy_Debug_Messenger() {
    if (m_debug_messenger != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) {
            func(m_instance, m_debug_messenger, nullptr);
        }
        m_debug_messenger = VK_NULL_HANDLE;
        m_debug_messenger_enabled = false;
    }
}

std::vector<VkPhysicalDevice> Instance::Enumerate_Physical_Devices() const {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);

    if (device_count == 0) {
        return {};
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

    return devices;
}


// Static helpers
std::vector<const char*> Instance::Get_Required_Extensions(bool enable_validation) {
    std::vector<const char*> extensions;

    // Platform-specific surface extensions would be added here
    #ifdef VK_USE_PLATFORM_WIN32_KHR
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #endif
    #ifdef VK_USE_PLATFORM_XLIB_KHR
    extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
    #endif
    #ifdef VK_USE_PLATFORM_WAYLAND_KHR
    extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
    #endif

    if (enable_validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool Instance::Check_Validation_Layer_Support() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char* layer_name : {"VK_LAYER_KHRONOS_validation"}) {
        bool layer_found = false;
        for (const auto& layer_properties : available_layers) {
            if (strcmp(layer_name, layer_properties.layerName) == 0) {
                layer_found = true;
                break;
            }
        }
        if (!layer_found) {
            return false;
        }
    }
    return true;
}

bool Instance::Check_Extension_Support(const char* extension_name) {
    uint32_t extension_count;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data());

    for (const auto& extension : available_extensions) {
        if (strcmp(extension_name, extension.extensionName) == 0) {
            return true;
        }
    }
    return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL Instance::Debug_Callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {

    // Filter: only print warnings and errors to reduce noise
    if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        fprintf(stderr, "[Vulkan %s] %s\n",
                message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "ERROR" :
                message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ? "WARN" : "INFO",
                callback_data->pMessage);
    }

    return VK_FALSE;  // Don't abort
}

