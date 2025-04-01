#ifndef VULKAN_IMPL_H
#define VULKAN_IMPL_H

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "../types/types.h"
#include "../math/math.h"

class window {
    public:
    i32          Width;
    i32          Height;
    GLFWwindow*  Window;
    VkSurfaceKHR Surface;
    Vec2         ScalingFactor;
    bool         FocusedWindow;
};

class vulkan_iface {
    public:
    global constexpr char* VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
    global constexpr char* DEVICE_EXTENSIONS[] =  {
        "VK_KHR_swapchain",
    	"VK_KHR_dynamic_rendering",
    	"VK_EXT_descriptor_indexing"
    };
    global constexpr u64 MAX_FRAMES_IN_FLIGHT = 2;

    private:
};

#endif // VULKAN_IMPL_H
