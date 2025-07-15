#ifndef VULKAN_IMPL_H
#define VULKAN_IMPL_H

#include <stdio.h>
#include <string.h>

// TODO: ERASE AND REPLACE
//
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include <SDL/SDL2.h>

#include "../math/math.h"
#include "../memory/memory.h"
#include "../types/types.h"
#include "../vector/vector.h"

#define VK_CHECK(x)                                                            \
do {                                                                           \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
        printf("Detected Vulkan error: %s", string_VkResult(err));             \
        abort();                                                               \
    }                                                                          \
} while (0)

// -----------------------------------------------------------------------------

typedef struct api_window api_window;
struct api_window {
	SDL_Window*  Window;
	F64          Width;
	F64          Height;
	vkSurfaceKHR Surface;
}

// -----------------------------------------------------------------------------

typedef struct queue_family_indices queue_family_indices;
struct queue_family_indices {
    U32 GraphicsAndCompute;
    U32 Presentation;
}

// -----------------------------------------------------------------------------

typedef struct swapchain_support_details swapchain_support_details;
struct swapchain_support_details {
    vkSurfaceCapabilitiesKHR Capabilities    ;
    vkSurfaceFormatKHR       Formats[]       ;
    vkPresentModeKHR         PresentModes[]  ;
    U32                      FormatCount     ;
    U32                      PresentModeCount;
}

// -----------------------------------------------------------------------------

typedef struct vulkan_base vulkan_base;
struct vulkan_base {
    // -------------------- GLOBAL ------------------------------- //
    //
    const U32 MAX_FRAMES_IN_FLIGHT = 2;
    U32 WIDTH  = 1200;
    U32 HEIGHT = 1200;
    char VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
    char DEVICE_EXTENSIONS[] = {
                        "VK_KHR_swapchain",
	                    "VK_KHR_dynamic_rendering",
                        "VK_EXT_descriptor_indexing",
                        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
                        };
    // --------------------- GENERAL ----------------------------- //
	//
	vkInstance               Instance                  ;
	api_window               Window                    ;
	vk_device                Device                    ;
	vk_swapchain             Swapchain                 ;
	vk_descriptor_allocator  GlobalDescriptorAllocator ;
	vk_semaphore             Semaphores                ;

	// --------------------- Immediate Submite ------------------- //
	//
	vkFence         ImmFence         ;
	vkCommandBuffer ImmCommandBuffer ;
	vkCommandPool   ImmCommandPool   ;

	// --------------------- Command Buffers --------------------- //
	//
	vkCommandPool   CommandPool           [MAX_FRAMES_IN_FLIGHT];
	vkCommandBuffer CommandBuffers        [MAX_FRAMES_IN_FLIGHT];
	vkCommandBuffer ComputeCommandBuffers [MAX_FRAMES_IN_FLIGHT];

	// --------------------- Depth Image ------------------------- //
	//
	vk_image DepthImage;

	// --------------------- Frame data -------------------------- //
	//
	U64  CurrentFrame;
	F64  LastTimeFrame;
	bool FramebufferResized;
	F64  LastTime;
	U32  SwapchainImageIdx;

	// --------------------- Debug Utilities --------------------- //
	//
	vkDebugUtilsMessengerEXT DebugMessenger;

	// --------------------- VMA Allocator ----------------------- //
	//
	vmaAllocator GPUAllocator;
};

#endif // VULKAN_IMPL_H

#ifdef RENDER_IMPL

// ----------------------------- DEBUG ------------------------------
//
global VKAPI_ATTR VkBool32 VKAPI_CALL
DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
  fprintf(stderr, "validation layer: %s\n", pCallbackData->pMessage);

  return VK_FALSE;
}

// ------------------------------------------------------------------
//
global void PopulateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = DebugCallback;
}

// ------------------------------------------------------------------
//
global void
DestroyDebugUtilsMessengerEXT(VkInstance instance,
                              VkDebugUtilsMessengerEXT debugMessenger,
                              const VkAllocationCallbacks *pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

// ------------------------------------------------------------------
//
global VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}


#endif
