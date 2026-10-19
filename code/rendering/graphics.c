#include "graphics_internal.h"
#include <SDL3/SDL_vulkan.h>
#include <stdio.h>

#ifdef GRAPHICS_TESTING
static Graphics_Test_Fault test_fault;
void graphics_test_fault(Graphics_Test_Fault fault) {
    test_fault = fault;
}
bool graphics_test_failure(Graphics_Test_Fault fault) {
    bool hit = test_fault == fault;
    if (hit)
        test_fault = GRAPHICS_FAIL_NONE;
    return hit;
}
#endif

Graphics_Result
graphics_vk_result(Graphics_Device_t *device, VkResult result, Graphics_Result fallback) {
    if (result == VK_SUCCESS) {
        return Graphics_NONE;
    }
    if (device != NULL) {
        device->last_vk_result = result;
        device->lost = device->lost || result == VK_ERROR_DEVICE_LOST;
    }
    if (result == VK_ERROR_DEVICE_LOST)
        return Graphics_DEVICE_LOST;
    if (result == VK_ERROR_OUT_OF_HOST_MEMORY)
        return Graphics_NO_MEMORY;
    if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY)
        return Graphics_NO_DEVICE_MEM;
    return fallback;
}

const char *graphics_result_string(Graphics_Result result) {
    static const char *const names[] = {"success",
                                        "invalid argument",
                                        "invalid recording state",
                                        "resource error",
                                        "no suitable device",
                                        "host allocation failed",
                                        "device allocation failed",
                                        "buffer is not mapped",
                                        "out of range",
                                        "bounded capacity exhausted",
                                        "unsupported",
                                        "validation layer unavailable",
                                        "extension unavailable",
                                        "window unavailable",
                                        "surface unavailable",
                                        "swapchain unavailable",
                                        "swapchain out of date",
                                        "command buffer error",
                                        "queue submission failed",
                                        "presentation failed",
                                        "fence error",
                                        "device lost",
                                        "busy"};
    return (unsigned)result < sizeof(names) / sizeof(names[0]) ? names[(unsigned)result]
                                                               : "unknown";
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
              VkDebugUtilsMessageTypeFlagsEXT types,
              const VkDebugUtilsMessengerCallbackDataEXT *data,
              void *user) {
    Graphics_Device_t *device = (Graphics_Device_t *)user;
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
        ++device->stats.validation_errors;
    }
    if (device->enabled.debug_callback != NULL) {
        return device->enabled.debug_callback(severity, types, data, device->enabled.debug_user);
    }
    fprintf(stderr, "[Vulkan] %s\n", data->pMessage != NULL ? data->pMessage : "");
    return VK_FALSE;
}

static bool has_layer(const char *name) {
    uint32_t count = 0U;
    if (vkEnumerateInstanceLayerProperties(&count, NULL) != VK_SUCCESS || count == 0U ||
        count > GRAPHICS_MAX_DISCOVERY_PROPERTIES)
        return false;
    VkLayerProperties *layers = (VkLayerProperties *)malloc(count * sizeof(*layers));
    if (layers == NULL)
        return false;
    VkResult result = vkEnumerateInstanceLayerProperties(&count, layers);
    bool found = false;
    if (result == VK_SUCCESS || result == VK_INCOMPLETE) {
        for (uint32_t i = 0U; i < count; ++i)
            found = found || strcmp(layers[i].layerName, name) == 0;
    }
    free(layers);
    return found;
}

static bool has_instance_extension(const char *layer, const char *name) {
    uint32_t count = 0U;
    if (vkEnumerateInstanceExtensionProperties(layer, &count, NULL) != VK_SUCCESS || count == 0U ||
        count > GRAPHICS_MAX_DISCOVERY_PROPERTIES)
        return false;
    VkExtensionProperties *extensions =
        (VkExtensionProperties *)malloc(count * sizeof(*extensions));
    if (extensions == NULL)
        return false;
    VkResult result = vkEnumerateInstanceExtensionProperties(layer, &count, extensions);
    bool found = false;
    if (result == VK_SUCCESS || result == VK_INCOMPLETE) {
        for (uint32_t i = 0U; i < count; ++i)
            found = found || strcmp(extensions[i].extensionName, name) == 0;
    }
    free(extensions);
    return found;
}

static bool has_device_extension(VkPhysicalDevice physical, const char *name) {
    uint32_t count = 0U;
    if (vkEnumerateDeviceExtensionProperties(physical, NULL, &count, NULL) != VK_SUCCESS ||
        count == 0U || count > GRAPHICS_MAX_DISCOVERY_PROPERTIES)
        return false;
    VkExtensionProperties *extensions =
        (VkExtensionProperties *)malloc(count * sizeof(*extensions));
    if (extensions == NULL)
        return false;
    VkResult result = vkEnumerateDeviceExtensionProperties(physical, NULL, &count, extensions);
    bool found = false;
    if (result == VK_SUCCESS || result == VK_INCOMPLETE) {
        for (uint32_t i = 0U; i < count; ++i)
            found = found || strcmp(extensions[i].extensionName, name) == 0;
    }
    free(extensions);
    return found;
}

static Graphics_Result create_instance(Graphics_Device_t *device) {
    const char *layer = "VK_LAYER_KHRONOS_validation";
    const char *extensions[32];
    uint32_t count = 0U;
    uint32_t version = 0U;
    if (vkEnumerateInstanceVersion(&version) != VK_SUCCESS || version < VK_API_VERSION_1_3)
        return Graphics_UNSUPPORTED;
    if (device->enabled.validation && !has_layer(layer))
        return Graphics_LAYER_NON_AVAILABLE;
    if (device->presentation) {
        uint32_t sdl_count = 0U;
        const char *const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_count);
        if (sdl_extensions == NULL)
            return Graphics_EXTENSION_NON_AVAILABLE;
        if (sdl_count > 30U)
            return Graphics_CAPACITY;
        for (uint32_t i = 0U; i < sdl_count; ++i)
            extensions[count++] = sdl_extensions[i];
    }
    if (device->enabled.validation)
        extensions[count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    if (device->enabled.synchronization_validation)
        extensions[count++] = VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME;
    for (uint32_t i = 0U; i < count; ++i) {
        if (!has_instance_extension(NULL, extensions[i]) &&
            !(device->enabled.validation && has_instance_extension(layer, extensions[i]))) {
            return Graphics_EXTENSION_NON_AVAILABLE;
        }
    }
    VkDebugUtilsMessengerCreateInfoEXT debug = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_message,
        .pUserData = device,
    };
    VkValidationFeatureEnableEXT sync = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validation = {
        .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
        .pNext = &debug,
        .enabledValidationFeatureCount = 1U,
        .pEnabledValidationFeatures = &sync,
    };
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                             .pApplicationName = "base graphics",
                             .apiVersion = VK_API_VERSION_1_3};
    VkInstanceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = device->enabled.synchronization_validation
                     ? (const void *)&validation
                     : (device->enabled.validation ? (const void *)&debug : NULL),
        .pApplicationInfo = &app,
        .enabledExtensionCount = count,
        .ppEnabledExtensionNames = extensions,
        .enabledLayerCount = device->enabled.validation ? 1U : 0U,
        .ppEnabledLayerNames = device->enabled.validation ? &layer : NULL,
    };
    Graphics_Result result = graphics_vk_result(
        device, vkCreateInstance(&info, NULL, &device->instance), Graphics_NO_DEVICE);
    if (result == Graphics_NONE && device->enabled.validation) {
        PFN_vkCreateDebugUtilsMessengerEXT create =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                device->instance, "vkCreateDebugUtilsMessengerEXT");
        result = create != NULL
                     ? graphics_vk_result(
                           device,
                           create(device->instance, &debug, NULL, &device->debug_messenger),
                           Graphics_ERROR_RESOURCE)
                     : Graphics_EXTENSION_NON_AVAILABLE;
    }
    return result;
}

static bool device_suitable(Graphics_Device_t *device,
                            VkPhysicalDevice physical,
                            VkSurfaceKHR surface,
                            uint32_t *family) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical, &properties);
    if (properties.apiVersion < VK_API_VERSION_1_3)
        return false;
    VkPhysicalDeviceVulkan13Features f13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceVulkan12Features f12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &f13};
    VkPhysicalDeviceFeatures2 f = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                   .pNext = &f12};
    vkGetPhysicalDeviceFeatures2(physical, &f);
    if (!f13.dynamicRendering || !f13.synchronization2 ||
        (device->enabled.buffer_device_address && !f12.bufferDeviceAddress) ||
        (device->enabled.scalar_block_layout && !f12.scalarBlockLayout) ||
        (device->enabled.sampler_anisotropy && !f.features.samplerAnisotropy))
        return false;
    if (device->presentation && !has_device_extension(physical, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        return false;
    VkQueueFamilyProperties queues[GRAPHICS_MAX_ENUMERATED];
    uint32_t count = GRAPHICS_MAX_ENUMERATED;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, queues);
    for (uint32_t i = 0U; i < count; ++i) {
        VkBool32 present = VK_TRUE;
        if (surface != VK_NULL_HANDLE &&
            vkGetPhysicalDeviceSurfaceSupportKHR(physical, i, surface, &present) != VK_SUCCESS)
            present = VK_FALSE;
        if (queues[i].queueCount != 0U && (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U &&
            present) {
            *family = i;
            return true;
        }
    }
    return false;
}

static Graphics_Result create_device(Graphics_Device_t *device, VkSurfaceKHR surface) {
    VkPhysicalDevice physical[GRAPHICS_MAX_ENUMERATED];
    uint32_t count = GRAPHICS_MAX_ENUMERATED;
    VkResult enumerated = vkEnumeratePhysicalDevices(device->instance, &count, physical);
    if (enumerated != VK_SUCCESS && enumerated != VK_INCOMPLETE)
        return graphics_vk_result(device, enumerated, Graphics_NO_DEVICE);
    int best_score = -1;
    for (uint32_t i = 0U; i < count; ++i) {
        uint32_t family = 0U;
        if (device_suitable(device, physical[i], surface, &family)) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(physical[i], &properties);
            int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 1;
            if (score > best_score) {
                best_score = score;
                device->physical_device = physical[i];
                device->queue_family = family;
                device->properties = properties;
            }
        }
    }
    if (best_score < 0)
        return Graphics_NO_DEVICE;
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                     .queueFamilyIndex = device->queue_family,
                                     .queueCount = 1U,
                                     .pQueuePriorities = &priority};
    VkPhysicalDeviceVulkan13Features f13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE};
    VkPhysicalDeviceVulkan12Features f12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &f13,
        .bufferDeviceAddress = device->enabled.buffer_device_address,
        .scalarBlockLayout = device->enabled.scalar_block_layout};
    VkPhysicalDeviceFeatures f = {.samplerAnisotropy = device->enabled.sampler_anisotropy};
    const char *extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo info = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                               .pNext = &f12,
                               .queueCreateInfoCount = 1U,
                               .pQueueCreateInfos = &queue,
                               .pEnabledFeatures = &f,
                               .enabledExtensionCount = device->presentation ? 1U : 0U,
                               .ppEnabledExtensionNames = &extension};
    Graphics_Result result =
        graphics_vk_result(device,
                           vkCreateDevice(device->physical_device, &info, NULL, &device->device),
                           Graphics_NO_DEVICE);
#ifdef GRAPHICS_TESTING
    if (result == Graphics_NONE && graphics_test_failure(GRAPHICS_FAIL_DEVICE))
        result = Graphics_NO_MEMORY;
#endif
    if (result == Graphics_NONE) {
        vkGetDeviceQueue(device->device, device->queue_family, 0U, &device->queue);
        vkGetPhysicalDeviceMemoryProperties(device->physical_device, &device->memory_properties);
        VkPipelineCacheCreateInfo cache = {.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        result = graphics_vk_result(
            device,
            vkCreatePipelineCache(device->device, &cache, NULL, &device->pipeline_cache),
            Graphics_NO_MEMORY);
    }
    return result;
}

Graphics_Result graphics_init(const Graphics_Device_Desc_t *desc, Graphics_Device_t **out) {
    if (desc == NULL || out == NULL || *out != NULL ||
        (desc->synchronization_validation && !desc->validation))
        return Graphics_ERROR_INVALID_ARGUMENT;
    Graphics_Device_t *device = (Graphics_Device_t *)calloc(1U, sizeof(*device));
    if (device == NULL)
        return Graphics_NO_MEMORY;
    device->enabled = *desc;
    device->presentation = desc->window != NULL;
    device->free_allocation = 1U;
    for (uint32_t i = 0U; i < GRAPHICS_MAX_ALLOCATIONS; ++i)
        device->allocations[i].next = i + 1U < GRAPHICS_MAX_ALLOCATIONS ? i + 2U : 0U;
    Graphics_Result result = create_instance(device);
#ifdef GRAPHICS_TESTING
    if (result == Graphics_NONE && graphics_test_failure(GRAPHICS_FAIL_INSTANCE))
        result = Graphics_NO_MEMORY;
#endif
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (result == Graphics_NONE && device->presentation &&
        !SDL_Vulkan_CreateSurface(desc->window, device->instance, NULL, &surface))
        result = Graphics_NO_SURFACE;
    if (result == Graphics_NONE)
        result = create_device(device, surface);
    if (surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(device->instance, surface, NULL);
    device->enabled.window = NULL;
    if (result == Graphics_NONE)
        *out = device;
    else
        graphics_device_cleanup(device);
    return result;
}

Graphics_Result graphics_wait_idle(Graphics_Device_t *device) {
    if (device == NULL || device->device == VK_NULL_HANDLE)
        return Graphics_NO_DEVICE;
    return graphics_vk_result(device, vkDeviceWaitIdle(device->device), Graphics_ERROR_FENCE);
}

VkDevice graphics_native_device(const Graphics_Device_t *device) {
    return device != NULL ? device->device : VK_NULL_HANDLE;
}
const VkPhysicalDeviceProperties *graphics_device_properties(const Graphics_Device_t *device) {
    return device != NULL ? &device->properties : NULL;
}
Graphics_Stats_t graphics_stats(const Graphics_Device_t *device) {
    return device != NULL ? device->stats : (Graphics_Stats_t){0};
}
VkResult graphics_last_vk_result(const Graphics_Device_t *device) {
    return device != NULL ? device->last_vk_result : VK_SUCCESS;
}
