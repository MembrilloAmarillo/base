#include <SDL3/SDL_loadso.h>
#include <SDL3/SDL_video.h>
#include <stdio.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_vulkan.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#include "graphics_internal.h"

Color_Target_Desc_t g_default_color_target[1] = {0};
Rasterization_State_t g_default_rasterization_state = {0};

/**
 * Forward declaration ----------------------------------------------
 */

static uint32_t find_memory_type(VkPhysicalDevice physical_device,
                                 uint32_t type_filter,
                                 VkMemoryPropertyFlags properties);

Graphics_Result graphics_load_functions() {
  Graphics_Result tResult = Graphics_NONE;

  if (!SDL_Vulkan_LoadLibrary(NULL)) {
    tResult = Graphics_NO_LIBRARY;
  }

  return tResult;
}

Graphics_Result graphics_get_win_size(Graphics_Device_t *device, int *w_out,
                                      int *h_out) {
  if (device == NULL || w_out == NULL || h_out == NULL) {
    return Graphics_NO_MEMORY;
  }
  if (SDL_GetWindowSizeInPixels(device->window, w_out, h_out)) {
    return Graphics_NONE;
  } else {
    return Graphics_NO_WINDOW;
  }
}

Color_Target_Desc_t graphics_color_target_desc_default() {
  Color_Target_Desc_t color_target = {
    .format = FORMAT_RGBA8_SRGB,

    .blend = {
      .enabled = false,

      .color = {
        .source = BLEND_FACTOR_ONE,
        .destination = BLEND_FACTOR_ZERO,
        .operation = BLEND_OP_ADD,
      },

      .alpha = {
        .source = BLEND_FACTOR_ONE,
        .destination = BLEND_FACTOR_ZERO,
        .operation = BLEND_OP_ADD,
      },
    },

    .write_mask = 0xF,
  };

  return color_target;
}

Rasterization_State_t graphics_rasterization_state_default() {
  Rasterization_State_t rasterization = {
    .cull = CULL_MODE_NONE,
    .depth_bias_constant = 0.0f,
    .depth_bias_clamp = 0.0f,
    .depth_bias_slope = 0.0f,
  };

  return rasterization;
}

/**
 * Init func --------------------------------------------------------
 */

static VKAPI_ATTR VkBool32 VKAPI_CALL graphics_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data) {
  (void)message_types;
  (void)user_data;

  const char *level = "VERBOSE";
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    level = "ERROR";
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    level = "WARNING";
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    level = "INFO";
  }

  fprintf(stderr, "[VULKAN][%s][%s] %s\n", level,
          callback_data->pMessageIdName ? callback_data->pMessageIdName : "",
          callback_data->pMessage ? callback_data->pMessage : "");
  return VK_FALSE;
}

Graphics_Result graphics_init(Graphics_Device_t *tDevice) {
  if (tDevice == NULL || tDevice->device != VK_NULL_HANDLE) {
    return Graphics_ERROR_INVALID_ARGUMENT;
  }
  *tDevice = (Graphics_Device_t){0};

  g_default_color_target[0] = graphics_color_target_desc_default();
  g_default_rasterization_state = graphics_rasterization_state_default();

  Graphics_Result tResult = Graphics_NONE;
  VkInstance instance = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;

  uint32_t queue_family_count = 0;
  uint32_t queue_family = 0;

  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkImage swapchain_images[SMALL_VECTOR_CAPACITY] = {0};
  VkImageView swapchain_img_views[SMALL_VECTOR_CAPACITY] = {0};
  uint32_t swapchain_image_count = 0;

  VkImage depth_image = VK_NULL_HANDLE;
  VkImageView depth_img_view = VK_NULL_HANDLE;
  VkDeviceMemory depth_img_mem = VK_NULL_HANDLE;

  VkFence fences[MAX_FRAMES_IN_FLIGHT] = {VK_NULL_HANDLE};
  VkSemaphore image_semaphores[MAX_FRAMES_IN_FLIGHT] = {VK_NULL_HANDLE};
  VkSemaphore render_complete_semaphores[SMALL_VECTOR_CAPACITY] = {
      VK_NULL_HANDLE};

  VkCommandPool command_pool = VK_NULL_HANDLE;
  VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT] = {VK_NULL_HANDLE};

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    tResult = Graphics_NO_LIBRARY;
  }

  if (tResult == Graphics_NONE) {
    tResult = graphics_load_functions();
  }

  if (tResult == Graphics_NONE) {

    VkApplicationInfo app_info =
        (VkApplicationInfo){.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .pApplicationName = "simple graphics",
                            .apiVersion = VK_API_VERSION_1_3};

    uint32_t sdl_extension_count = 0;
    char const *const *sdl_extensions =
        SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
    if (sdl_extensions == NULL) {
      fprintf(stderr, "[GRAPHICS] Cannot get Vulkan extensions: %s\n",
              SDL_GetError());
      return Graphics_EXTENSION_NON_AVAILABLE;
    }
    if (sdl_extension_count >= SMALL_VECTOR_CAPACITY) {
      return Graphics_NO_MEMORY;
    }

    const char *instance_extensions[SMALL_VECTOR_CAPACITY] = {0};
    uint32_t instance_extension_count = sdl_extension_count;
    bool has_debug_utils = false;
    for (uint32_t i = 0; i < sdl_extension_count; i++) {
      instance_extensions[i] = sdl_extensions[i];
      if (strcmp(sdl_extensions[i], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
        has_debug_utils = true;
      }
    }
    if (!has_debug_utils) {
      instance_extensions[instance_extension_count++] =
          VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }

    const char *validation_layer = "VK_LAYER_KHRONOS_validation";
    VkDebugUtilsMessengerCreateInfoEXT debug_ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = graphics_debug_callback,
    };
    VkInstanceCreateInfo instance_ci = (VkInstanceCreateInfo){
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &debug_ci,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = &validation_layer,
        .enabledExtensionCount = instance_extension_count,
        .ppEnabledExtensionNames = instance_extensions,
    };
    VkResult result = vkCreateInstance(&instance_ci, NULL, &instance);
    if (result != VK_SUCCESS) {
      if (result == VK_ERROR_LAYER_NOT_PRESENT) {
        fprintf(stderr, "[GRAPHICS] Required Vulkan layer %s is unavailable. "
                        "Install the Vulkan validation layers.\n",
                validation_layer);
        tResult = Graphics_LAYER_NON_AVAILABLE;
      } else if (result == VK_ERROR_EXTENSION_NOT_PRESENT) {
        fprintf(stderr, "[GRAPHICS] A required Vulkan instance extension is "
                        "unavailable (including %s).\n",
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        tResult = Graphics_EXTENSION_NON_AVAILABLE;
      } else {
        fprintf(stderr, "[GRAPHICS] vkCreateInstance failed: %d\n", result);
        tResult = Graphics_NO_DEVICE;
      }
    } else {
      PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger =
          (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
              instance, "vkCreateDebugUtilsMessengerEXT");
      result = create_debug_messenger
                   ? create_debug_messenger(instance, &debug_ci, NULL,
                                            &tDevice->debug_messenger)
                   : VK_ERROR_EXTENSION_NOT_PRESENT;
      if (result != VK_SUCCESS) {
        fprintf(stderr,
                "[GRAPHICS] Cannot create Vulkan debug messenger: %d\n",
                result);
        vkDestroyInstance(instance, NULL);
        instance = VK_NULL_HANDLE;
        tResult = result == VK_ERROR_EXTENSION_NOT_PRESENT
                      ? Graphics_EXTENSION_NON_AVAILABLE
                      : Graphics_NO_DEVICE;
      }
    }
  }

  uint32_t device_count = 0;
  uint32_t selected_device = 0;
  VkPhysicalDevice devices_buffer[SMALL_VECTOR_CAPACITY] = {0};
  if (tResult == Graphics_NONE) {
    if (vkEnumeratePhysicalDevices(instance, &device_count, NULL) !=
        VK_SUCCESS) {
      tResult = Graphics_NO_DEVICE;
    } else if (device_count == 0U) {
      tResult = Graphics_NO_DEVICE;
    } else if (device_count > SMALL_VECTOR_CAPACITY) {
      tResult = Graphics_OUT_OF_RANGE;
    }
  }

  if (tResult == Graphics_NONE) {
    fprintf(stdout, "[GRAPHICS] Num devices: %d\n", device_count);
    if (vkEnumeratePhysicalDevices(instance, &device_count, devices_buffer) !=
        VK_SUCCESS) {
      tResult = Graphics_NO_DEVICE;
    }
    if (tResult == Graphics_NONE) {
      for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties2 device_properties =
            (VkPhysicalDeviceProperties2){
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        vkGetPhysicalDeviceProperties2(devices_buffer[i], &device_properties);
        fprintf(stdout, "Selected device: %s\n",
                device_properties.properties.deviceName);
      }
    }
  }

  VkQueueFamilyProperties queue_family_buffer[SMALL_VECTOR_CAPACITY];
  if (tResult == Graphics_NONE) {
    vkGetPhysicalDeviceQueueFamilyProperties(devices_buffer[selected_device],
                                             &queue_family_count, NULL);
    if (queue_family_count == 0U || queue_family_count > SMALL_VECTOR_CAPACITY) {
      tResult = Graphics_OUT_OF_RANGE;
    }
  }
  if (tResult == Graphics_NONE) {
    vkGetPhysicalDeviceQueueFamilyProperties(devices_buffer[selected_device],
                                             &queue_family_count,
                                             queue_family_buffer);

    for (uint32_t i = 0; i < queue_family_count; i++) {
      if (queue_family_buffer[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        queue_family = i;
        break;
      }
    }

    if (!SDL_Vulkan_GetPresentationSupport(
            instance, devices_buffer[selected_device], queue_family)) {
      tResult = Graphics_NO_PRESENTATION_FAMILY;
    }
  }

  const char *device_extensions[SMALL_VECTOR_CAPACITY] = {0};
  device_extensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
  if (tResult == Graphics_NONE) {
    const float qfpriorities = 1.0f;
    VkDeviceQueueCreateInfo queue_ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family,
        .queueCount = 1,
        .pQueuePriorities = &qfpriorities};
    VkPhysicalDeviceVulkan12Features enabled_vk12_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .scalarBlockLayout = true,
        .bufferDeviceAddress = true};
    VkPhysicalDeviceVulkan13Features enabled_vk13_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &enabled_vk12_features,
        .synchronization2 = true,
        .dynamicRendering = true};
    VkPhysicalDeviceFeatures enabled_vk10_features = {.samplerAnisotropy =
                                                          VK_TRUE};

    VkDeviceCreateInfo device_ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabled_vk13_features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_ci,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &device_extensions[0],
        .pEnabledFeatures = &enabled_vk10_features};

    if (vkCreateDevice(devices_buffer[selected_device], &device_ci, NULL,
                       &tDevice->device) != VK_SUCCESS) {
      tResult = Graphics_NO_DEVICE;
    }
    tDevice->physical_device = devices_buffer[selected_device];
    if (tResult == Graphics_NONE) {
      tDevice->vkCmdPushDataEXT_fn = (PFN_vkCmdPushDataEXT)vkGetDeviceProcAddr(
          tDevice->device, "vkCmdPushDataEXT");
    }
  }

  if (tResult == Graphics_NONE) {
    tDevice->window = SDL_CreateWindow(
        "simple", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    if (tDevice->window == NULL) {
      tResult = Graphics_NO_WINDOW;
    } else {
      if (!SDL_Vulkan_CreateSurface(tDevice->window, instance, NULL,
                                    &surface)) {
        tResult = Graphics_NO_SURFACE;
      }
    }
  }

  VkExtent2D swapchain_extent = {0};
  VkSurfaceCapabilitiesKHR surface_cap = {0};

  if (tResult == Graphics_NONE) {
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            devices_buffer[selected_device], surface, &surface_cap) !=
        VK_SUCCESS) {
      tResult = Graphics_NO_CAPABILITIES;
    } else if (surface_cap.currentExtent.width == 0xFFFFFFFF) {
      swapchain_extent = (VkExtent2D){1280, 720};
    } else {
      swapchain_extent = surface_cap.currentExtent;
    }
  }

  VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;
  VkSwapchainCreateInfoKHR swapchain_ci = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
      .minImageCount = surface_cap.minImageCount,
      .imageFormat = image_format,
      .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
      .imageExtent = swapchain_extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR};
  if (tResult == Graphics_NONE &&
      vkCreateSwapchainKHR(tDevice->device, &swapchain_ci, NULL, &swapchain) !=
          VK_SUCCESS) {
    tResult = Graphics_NO_SWAPCHAIN;
  }

  if (tResult == Graphics_NONE) {
    if (vkGetSwapchainImagesKHR(tDevice->device, swapchain,
                                &swapchain_image_count, NULL) != VK_SUCCESS) {
      tResult = Graphics_NO_SWAPCHAIN;
    } else if (swapchain_image_count > SMALL_VECTOR_CAPACITY) {
      tResult = Graphics_OUT_OF_RANGE;
    }
  }

  if (tResult == Graphics_NONE) {
    if (vkGetSwapchainImagesKHR(tDevice->device, swapchain,
                                &swapchain_image_count,
                                swapchain_images) != VK_SUCCESS) {
      tResult = Graphics_NO_SWAPCHAIN;
    } else {
      for (uint32_t i = 0; i < swapchain_image_count; i++) {
        VkImageViewCreateInfo viewCI = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = image_format,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .levelCount = 1,
                                 .layerCount = 1}};
        if (vkCreateImageView(tDevice->device, &viewCI, NULL,
                              &swapchain_img_views[i]) != VK_SUCCESS) {
          swapchain_img_views[i] = VK_NULL_HANDLE;
          tResult = Graphics_NO_SWAPCHAIN;
        }
      }
    }
  }

  VkFormat depth_format_list[] = {VK_FORMAT_D32_SFLOAT_S8_UINT,
                                  VK_FORMAT_D24_UNORM_S8_UINT};

  VkFormat depth_format = VK_FORMAT_UNDEFINED;

  if (tResult == Graphics_NONE) {
    for (uint32_t i = 0;
         i < sizeof(depth_format_list) / sizeof(depth_format_list[0]); i++) {
      VkFormat format = depth_format_list[i];

      VkFormatProperties2 format_properties = {
          .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, .pNext = NULL};

      vkGetPhysicalDeviceFormatProperties2(tDevice->physical_device, format,
                                           &format_properties);

      if (format_properties.formatProperties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        depth_format = format;
        break;
      }
    }

    if (depth_format == VK_FORMAT_UNDEFINED) {
      tResult = Graphics_NO_DEPTH_FORMAT;
    }
  }

  if (tResult == Graphics_NONE) {
    VkImageCreateInfo depth_ci = graphics_img_create_info(
        depth_format, (VkExtent3D){swapchain_extent.width, swapchain_extent.height, 1}, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    VkResult result =
        vkCreateImage(tDevice->device, &depth_ci, NULL, &depth_image);

    if (result != VK_SUCCESS) {
      depth_image = VK_NULL_HANDLE;
      tResult = Graphics_NO_DEPTH_FORMAT;
    }

    if (tResult == Graphics_NONE) {
      tResult = graphics_img_alloc(tDevice, depth_image, &depth_img_mem);
    }
    if (tResult == Graphics_NONE) {
      tResult = graphics_img_view_create(
          tDevice->device, depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, &depth_img_view);
    }
  }

  if (tResult == Graphics_NONE) {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT && tResult == Graphics_NONE; i++) {
      tResult = graphics_new_semaphore(tDevice, &image_semaphores[i]);
      if (tResult == Graphics_NONE) {
        tResult = graphics_new_fence(tDevice, &fences[i]);
      }
    }
    for (uint32_t i = 0; i < swapchain_image_count && tResult == Graphics_NONE;
         i++) {
      tResult = graphics_new_semaphore(tDevice, &render_complete_semaphores[i]);
    }
  }

  if (tResult == Graphics_NONE) {
    tResult = graphics_create_commands(tDevice, queue_family, &command_pool,
                                       command_buffers);
  }

  tDevice->instance = instance;
  tDevice->surface = surface;
  tDevice->queue_family_count = queue_family_count;
  tDevice->queue_family = queue_family;
  tDevice->swapchain = swapchain;
  tDevice->swapchain_extent = swapchain_extent;
  memcpy(tDevice->swapchain_images, swapchain_images,
         SMALL_VECTOR_CAPACITY * sizeof(VkImage));
  memcpy(tDevice->swapchain_img_views, swapchain_img_views,
         SMALL_VECTOR_CAPACITY * sizeof(VkImageView));
  tDevice->swapchain_image_count = swapchain_image_count;
  tDevice->depth_image = depth_image;
  tDevice->depth_img_view = depth_img_view;
  tDevice->depth_img_mem = depth_img_mem;
  tDevice->depth_format = depth_format;
  memcpy(tDevice->fences, fences, MAX_FRAMES_IN_FLIGHT * sizeof(VkFence));
  memcpy(tDevice->image_semaphores, image_semaphores,
         MAX_FRAMES_IN_FLIGHT * sizeof(VkSemaphore));
  memcpy(tDevice->render_complete_semaphores, render_complete_semaphores,
         SMALL_VECTOR_CAPACITY * sizeof(VkSemaphore));
  tDevice->command_pool = command_pool;
  memcpy(tDevice->command_buffers, command_buffers,
         MAX_FRAMES_IN_FLIGHT * sizeof(VkCommandBuffer));

  return tResult;
}

/**
 * pipelines --------------------------------------------------------
 */

/**
 * Pipeline initialization helper
 */
Gpu_PSO_Desc_t _graphics_pso_desc(Gpu_PSO_Desc_t desc) {

	// deal with defaults to see what can be done
	if(desc.color_targets == NULL) {
		desc.color_targets = g_default_color_target;
		desc.color_target_count = 1;
	}
	if(desc.rasterization_state == NULL) {
		desc.rasterization_state = &g_default_rasterization_state;
	}
	if(desc.first_stage_entry == NULL) {
		desc.first_stage_entry = "main";
	}
	if(desc.fragment_entry == NULL) {
		desc.fragment_entry = "main";
	}

	return desc;
}

Graphics_Result graphics_create_pso(
    Graphics_Device_t *device,
    Gpu_Pipeline_t *pso_out,
    Gpu_PSO_Desc_t *desc
) {
  if (device == NULL || device->device == VK_NULL_HANDLE || pso_out == NULL) {
    return Graphics_NO_DEVICE;
  }

  if (desc == NULL || desc->first_stage_spirv == NULL || desc->first_stage_spirv_size == 0 ||
      pso_out->pipeline != VK_NULL_HANDLE || pso_out->layout != VK_NULL_HANDLE ||
      desc->n_resources > SMALL_VECTOR_CAPACITY || desc->n_ranges > SMALL_VECTOR_CAPACITY ||
      (desc->n_resources != 0U && desc->resources == NULL) ||
      (desc->n_ranges != 0U && desc->ranges == NULL) ||
      (desc->fragment_spirv_size != 0U && desc->fragment_spirv == NULL)) {
    return Graphics_NO_MEMORY;
  }

  if (desc->color_target_count > MAX_COLOR_ATTACHMENTS) {
    return Graphics_NO_MEMORY;
  }

  // pso layout
  if(desc->set_layout) {

    VkPushConstantRange ranges[SMALL_VECTOR_CAPACITY] = {0};
    VkDescriptorSetLayout layouts[SMALL_VECTOR_CAPACITY] = {0};
    for(size_t i = 0; i < desc->n_ranges; i++) {
      ranges[i] = (VkPushConstantRange){
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = (uint32_t)desc->ranges[i].offset,
          .size = (uint32_t)desc->ranges[i].size,
      };
    }

    for(size_t i = 0; i < desc->n_resources; i++) {
      layouts[i] = desc->resources[i].layout;
    }

    VkPipelineLayoutCreateInfo layout = {0};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.pushConstantRangeCount = (uint32_t)desc->n_ranges;
    layout.pPushConstantRanges = ranges;
    layout.setLayoutCount = (uint32_t)desc->n_resources;
    layout.pSetLayouts = layouts;

    if(vkCreatePipelineLayout(device->device, &layout, NULL, &pso_out->layout) != VK_SUCCESS) {
      pso_out->layout = VK_NULL_HANDLE;
      return Graphics_ERROR_RESOURCE;
    }
  }

  VkShaderModuleCreateInfo first_stage_module_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = desc->first_stage_spirv_size * sizeof(uint32_t),
      .pCode = desc->first_stage_spirv,
  };

  VkShaderModuleCreateInfo fragment_module_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = desc->fragment_spirv_size * sizeof(uint32_t),
      .pCode = desc->fragment_spirv,
  };

  VkShaderModule first_module = VK_NULL_HANDLE;
  VkShaderModule fragment_module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device->device, &first_stage_module_info, NULL,
                           &first_module) != VK_SUCCESS) {
    vkDestroyPipelineLayout(device->device, pso_out->layout, NULL);
    *pso_out = (Gpu_Pipeline_t){0};
    return Graphics_ERROR_RESOURCE;
  }
  if (desc->fragment_spirv_size > 0 &&
      vkCreateShaderModule(device->device, &fragment_module_info, NULL,
                           &fragment_module) != VK_SUCCESS) {
    vkDestroyShaderModule(device->device, first_module, NULL);
    vkDestroyPipelineLayout(device->device, pso_out->layout, NULL);
    *pso_out = (Gpu_Pipeline_t){0};
    return Graphics_ERROR_RESOURCE;
  }

  VkPipelineShaderStageCreateInfo stages[2] = {
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage =
              desc->mesh ? VK_SHADER_STAGE_MESH_BIT_EXT : VK_SHADER_STAGE_VERTEX_BIT,
          .module = first_module,
          .pName = desc->first_stage_entry ? desc->first_stage_entry :
                   (desc->mesh ? "meshMain" : "vertexMain"),
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = fragment_module,
          .pName = desc->fragment_entry ? desc->fragment_entry : "fragmentMain",
      },
  };

  VkPipelineVertexInputStateCreateInfo vertex_input = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  };

  VkPipelineViewportStateCreateInfo viewport_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
  };

  VkPipelineRasterizationStateCreateInfo rasterization = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = desc->rasterization_state->cull == CULL_MODE_NONE
                      ? VK_CULL_MODE_NONE
                      : VK_CULL_MODE_BACK_BIT,
      .frontFace = desc->rasterization_state->cull == CULL_MODE_COUNTER_CLOCKWISE
                       ? VK_FRONT_FACE_CLOCKWISE
                       : VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = desc->rasterization_state->depth_bias_constant != 0.0f ||
                         desc->rasterization_state->depth_bias_clamp != 0.0f ||
                         desc->rasterization_state->depth_bias_slope != 0.0f,
      .depthBiasConstantFactor = desc->rasterization_state->depth_bias_constant,
      .depthBiasClamp = desc->rasterization_state->depth_bias_clamp,
      .depthBiasSlopeFactor = desc->rasterization_state->depth_bias_slope,
      .lineWidth = 1.0f,
  };

  VkPipelineMultisampleStateCreateInfo multisample = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  VkPipelineDepthStencilStateCreateInfo depth_stencil = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
  };

  VkPipelineColorBlendAttachmentState color_attachments[MAX_COLOR_ATTACHMENTS] =
      {0};

  for (size_t i = 0; i < desc->color_target_count; ++i) {
    const Color_Target_Desc_t *target = &desc->color_targets[i];

    color_attachments[i] = (VkPipelineColorBlendAttachmentState){
        .blendEnable = target->blend.enabled,
        .srcColorBlendFactor = (VkBlendFactor)target->blend.color.source,
        .dstColorBlendFactor = (VkBlendFactor)target->blend.color.destination,
        .colorBlendOp = (VkBlendOp)target->blend.color.operation,
        .srcAlphaBlendFactor = (VkBlendFactor)target->blend.alpha.source,
        .dstAlphaBlendFactor = (VkBlendFactor)target->blend.alpha.destination,
        .alphaBlendOp = (VkBlendOp)target->blend.alpha.operation,
        .colorWriteMask = target->write_mask,
    };
  }

  VkPipelineColorBlendStateCreateInfo color_blend = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = (uint32_t)desc->color_target_count,
      .pAttachments = desc->color_target_count > 0 ? color_attachments : NULL,
  };

  const VkDynamicState dynamic_states[] = {
      VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
      VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,

      VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
      VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
      VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,

      VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
      VK_DYNAMIC_STATE_STENCIL_OP,
      VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
      VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
      VK_DYNAMIC_STATE_STENCIL_REFERENCE,
  };

  VkPipelineDynamicStateCreateInfo dynamic_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount =
          (uint32_t)(sizeof(dynamic_states) / sizeof(dynamic_states[0])),
      .pDynamicStates = dynamic_states,
  };

  VkFormat color_formats[MAX_COLOR_ATTACHMENTS] = {0};

  for (size_t i = 0; i < desc->color_target_count; ++i) {
    color_formats[i] = (VkFormat)desc->color_targets[i].format;
  }

  VkPipelineRenderingCreateInfo rendering_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = (uint32_t)desc->color_target_count,
      .pColorAttachmentFormats = desc->color_target_count > 0 ? color_formats : NULL,
      .depthAttachmentFormat = desc->depth_format,
      .stencilAttachmentFormat = desc->stencil_format,
  };

  VkPipelineCreateFlags2CreateInfo flags_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
      .pNext = &rendering_info,
      .flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT,
  };

  VkGraphicsPipelineCreateInfo pso_info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = desc->set_layout ? (const void *)&rendering_info : (const void *)&flags_info,
      .stageCount = desc->fragment_spirv_size > 0 ? 2 : 1,
      .pStages = stages,
      .pVertexInputState = desc->mesh ? NULL : &vertex_input,
      .pInputAssemblyState = desc->mesh ? NULL : &input_assembly,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterization,
      .pMultisampleState = &multisample,
      .pDepthStencilState = &depth_stencil,
      .pColorBlendState = &color_blend,
      .pDynamicState = &dynamic_state,
      .layout = desc->set_layout ? pso_out->layout : VK_NULL_HANDLE,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };

  if (device->pipeline_cache == VK_NULL_HANDLE) {
    VkPipelineCacheCreateInfo cache_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    if (vkCreatePipelineCache(device->device, &cache_info, NULL, &device->pipeline_cache) != VK_SUCCESS) {
      device->pipeline_cache = VK_NULL_HANDLE;
    }
  }
  VkResult vk_result = vkCreateGraphicsPipelines(
    device->device,
    device->pipeline_cache,
    1,
    &pso_info,
    NULL,
    &pso_out->pipeline
  );

  vkDestroyShaderModule(device->device, fragment_module, NULL);
  vkDestroyShaderModule(device->device, first_module, NULL);

  if (vk_result != VK_SUCCESS) {
    vkDestroyPipeline(device->device, pso_out->pipeline, NULL);
    vkDestroyPipelineLayout(device->device, pso_out->layout, NULL);
    *pso_out = (Gpu_Pipeline_t){0};
    return Graphics_NO_MEMORY;
  }

  pso_out->owner = device->device;
  return Graphics_NONE;
}

Graphics_Result graphics_draw(VkCommandBuffer cmd, uint32_t n_vertices, uint32_t n_instances) {
    vkCmdDraw(cmd, n_vertices, n_instances, 0, 0);
    return Graphics_NONE;
}

Graphics_Result graphics_bind_pso(Graphics_Device_t *device, Gpu_Pipeline_t pipeline) {
  Graphics_Result tResult = Graphics_NONE;

  if (device == NULL || device->device == VK_NULL_HANDLE) {
    tResult = Graphics_NO_DEVICE;
  } else if (device->frame_index >= MAX_FRAMES_IN_FLIGHT ||
             pipeline.pipeline == VK_NULL_HANDLE || pipeline.owner != device->device) {
    tResult = Graphics_ERROR_INVALID_ARGUMENT;
  } else if (device->command_buffers[device->frame_index] == VK_NULL_HANDLE) {
    tResult = Graphics_NO_COMMAND_BUFFER;
  } else {
    device->pipeline_layout = pipeline.layout;
    VkCommandBuffer cmd = device->command_buffers[device->frame_index];
    Graphics_Command_State_t *state = &device->command_state[device->frame_index];
    if (!state->valid || state->pipeline != pipeline.pipeline) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
      ++device->stats.pipeline_binds;
      state->pipeline = pipeline.pipeline;
    }
    if (!state->valid) {
      // Initialize once per recording, then preserve explicit dynamic state.
      vkCmdSetDepthTestEnable(cmd, VK_FALSE);
      vkCmdSetDepthWriteEnable(cmd, VK_TRUE);
      vkCmdSetDepthCompareOp(cmd, VK_COMPARE_OP_LESS);
      vkCmdSetStencilTestEnable(cmd, VK_FALSE);
      vkCmdSetStencilOp(cmd, VK_STENCIL_FACE_FRONT_AND_BACK,
                      VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                      VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);
      vkCmdSetStencilCompareMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, UINT32_MAX);
      vkCmdSetStencilWriteMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, UINT32_MAX);
      vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, 0U);
      device->stats.dynamic_state_commands += 8U;
      state->valid = true;
      state->depth_enabled = false;
      state->stencil_enabled = false;
    }
  }

  return tResult;
}

Graphics_Result graphics_set_depth_test_enabled(Graphics_Device_t *device, bool enabled) {
  Graphics_Result tResult = Graphics_NONE;

  if (device == NULL || device->device == VK_NULL_HANDLE) {
    tResult = Graphics_NO_DEVICE;
  } else if (device->frame_index >= MAX_FRAMES_IN_FLIGHT) {
    tResult = Graphics_ERROR_INVALID_ARGUMENT;
  } else if (device->command_buffers[device->frame_index] == VK_NULL_HANDLE) {
    tResult = Graphics_NO_COMMAND_BUFFER;
  } else {
    Graphics_Command_State_t *state = &device->command_state[device->frame_index];
    if (!state->valid || state->depth_enabled != enabled) {
      vkCmdSetDepthTestEnable(device->command_buffers[device->frame_index],
                            enabled ? VK_TRUE : VK_FALSE);
      ++device->stats.dynamic_state_commands;
      state->depth_enabled = enabled;
    }
  }

  return tResult;
}

Graphics_Result graphics_set_stencil_test_enabled(Graphics_Device_t *device, bool enabled) {
  Graphics_Result tResult = Graphics_NONE;

  if (device == NULL || device->device == VK_NULL_HANDLE) {
    tResult = Graphics_NO_DEVICE;
  } else if (device->frame_index >= MAX_FRAMES_IN_FLIGHT) {
    tResult = Graphics_ERROR_INVALID_ARGUMENT;
  } else if (device->command_buffers[device->frame_index] == VK_NULL_HANDLE) {
    tResult = Graphics_NO_COMMAND_BUFFER;
  } else {
    Graphics_Command_State_t *state = &device->command_state[device->frame_index];
    if (!state->valid || state->stencil_enabled != enabled) {
      vkCmdSetStencilTestEnable(device->command_buffers[device->frame_index],
                              enabled ? VK_TRUE : VK_FALSE);
      ++device->stats.dynamic_state_commands;
      state->stencil_enabled = enabled;
    }
  }

  return tResult;
}

void graphics_invalidate_state(Graphics_Device_t *device) {
  if (device != NULL && device->frame_index < MAX_FRAMES_IN_FLIGHT) {
    device->command_state[device->frame_index] = (Graphics_Command_State_t){0};
    device->pipeline_layout = VK_NULL_HANDLE;
  }
}

Graphics_Result graphics_push_constant(Graphics_Device_t *device, const void *data, size_t size) {
  //vkCmdPushConstants(cmd, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *pValues)

  if (device == NULL || data == NULL || size == 0) {
      return Graphics_ERROR_INVALID_ARGUMENT;
  }
  if (device->pipeline_layout != VK_NULL_HANDLE) {
    vkCmdPushConstants(device->command_buffers[device->frame_index],
                       device->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, (uint32_t)size, data);
    return Graphics_NONE;
  }
  if (device->vkCmdPushDataEXT_fn == NULL) {
    return Graphics_EXTENSION_NON_AVAILABLE;
  }
  VkPushDataInfoEXT info = {
      .sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
      .offset = 0,
      .data = {
          .address = data,
          .size = size,
      },
  };

  device->vkCmdPushDataEXT_fn(
      device->command_buffers[device->frame_index],
      &info
  );

  return Graphics_NONE;
}

/**
 * buffer alloc func ------------------------------------------------
 */

Graphics_Result graphics_alloc(Graphics_Device_t *device, Gpu_Buffer_t *buffer,
                               VkDeviceSize size, Gpu_Alloc_Type_t alloc_type) {
  Gpu_Buffer_Desc_t desc = {
    .size = size,
    .usage =
       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
       VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
       VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    .memory = alloc_type,
  };
  return graphics_create_buffer(device, &desc, buffer);
}

Graphics_Result graphics_memcpy(Graphics_Device_t *device,
                                Gpu_Buffer_t *buffer_dst, const void *data,
                                Gpu_Range_t range) {
    Graphics_Result result = Graphics_NONE;
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        result = Graphics_NO_DEVICE;
    } else if (buffer_dst == NULL || data == NULL || buffer_dst->buffer == VK_NULL_HANDLE || buffer_dst->owner != device->device) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    } else if (range.offset > buffer_dst->size || range.size > buffer_dst->size - range.offset) {
        result = Graphics_OUT_OF_RANGE;
    } else if (buffer_dst->mapped == NULL) {
        result = Graphics_NO_MAPPEABLE;
    } else {
        (void)memcpy((uint8_t*)buffer_dst->mapped + range.offset, data, range.size);
    }
    return result;
}

Graphics_Result graphics_copy(Graphics_Device_t *device, Gpu_Buffer_t *buffer_dst, Gpu_Buffer_t *buffer_src, Gpu_Range_t range, VkCommandBuffer cmd) {
    Graphics_Result result = Graphics_NONE;
    bool local_cmd = cmd == VK_NULL_HANDLE;
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        result = Graphics_NO_DEVICE;
    } else if (buffer_dst == NULL || buffer_src == NULL ||
               buffer_dst->owner != device->device || buffer_src->owner != device->device ||
               buffer_dst->buffer == VK_NULL_HANDLE || buffer_src->buffer == VK_NULL_HANDLE ||
               buffer_dst->buffer == buffer_src->buffer || range.size == 0U ||
               (buffer_src->usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0U ||
               (buffer_dst->usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0U) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
    } else if (range.offset > buffer_dst->size || range.size > buffer_dst->size - range.offset ||
               range.offset > buffer_src->size || range.size > buffer_src->size - range.offset) {
        result = Graphics_OUT_OF_RANGE;
    }
    if (result == Graphics_NONE && local_cmd) {
        result = graphics_single_time_command_buffer(device, device->command_pool, &cmd);
        if (result == Graphics_NONE) {
            result = graphics_begin_command_buffer(cmd);
        }
    }
    if (result == Graphics_NONE) {
        VkBufferCopy region = {range.offset, range.offset, range.size};
        graphics_issue_memory_barrier(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT);
        vkCmdCopyBuffer(cmd, buffer_src->buffer, buffer_dst->buffer, 1U, &region);
        graphics_issue_memory_barrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_2_HOST_BIT,
            VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_HOST_READ_BIT);
        if (local_cmd) {
            result = graphics_end_command_buffer(cmd);
            if (result == Graphics_NONE) {
                result = graphics_submit_and_wait(device, cmd);
            }
        }
    }
    if (local_cmd && cmd != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device->device, device->command_pool, 1U, &cmd);
    }
    return result;
}

/**
 * ------------------------------------------------------------------
 */
VkImageCreateInfo graphics_img_create_info(VkFormat format, VkExtent3D extent,
                                           VkSampleCountFlags sample_count,
                                           VkImageTiling tiling,
                                           VkImageUsageFlags usage) {
    VkImageCreateInfo image_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = sample_count,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,

      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
  };

  return image_ci;
}

/**
 * ------------------------------------------------------------------
 */

Graphics_Result graphics_texture_data(
  Graphics_Device_t *tDevice,
  Gpu_Texture_t *tTexture,
  const void *data,
  size_t data_size
) {
  Graphics_Result tResult = Graphics_NONE;
  Gpu_Upload_Batch_t batch = {0};
  tResult = graphics_create_upload_batch(tDevice, data_size, &batch);
  if (tResult == Graphics_NONE) {
    tResult = graphics_begin_uploads(tDevice, &batch);
  }
  if (tResult == Graphics_NONE) {
    tResult = graphics_upload_texture(tDevice, &batch, tTexture, data, data_size);
  }
  if (tResult == Graphics_NONE) {
    tResult = graphics_submit_uploads(tDevice, &batch);
  }
  if (batch.owner != VK_NULL_HANDLE) {
    Graphics_Result cleanup = graphics_destroy_upload_batch(tDevice, &batch);
    if (tResult == Graphics_NONE) {
      tResult = cleanup;
    }
  }
  return tResult;
}

/**
 * ------------------------------------------------------------------
 */

Graphics_Result graphics_img_alloc(Graphics_Device_t *device, VkImage image, VkDeviceMemory *memory_out) {
  Graphics_Result result = Graphics_NONE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  if (device == NULL || device->device == VK_NULL_HANDLE || device->physical_device == VK_NULL_HANDLE) {
    result = Graphics_NO_DEVICE;
  } else if (image == VK_NULL_HANDLE || memory_out == NULL || *memory_out != VK_NULL_HANDLE) {
    result = Graphics_ERROR_INVALID_ARGUMENT;
  } else {
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device->device, image, &requirements);
    uint32_t type = find_memory_type(device->physical_device, requirements.memoryTypeBits,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (type == UINT32_MAX) {
      result = Graphics_NO_DEVICE_MEM;
    } else {
      VkMemoryDedicatedAllocateInfo dedicated = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, .image = image,
      };
      VkMemoryAllocateInfo info = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
          .pNext = &dedicated,
          .allocationSize = requirements.size, .memoryTypeIndex = type,
      };
      if (vkAllocateMemory(device->device, &info, NULL, &memory) != VK_SUCCESS) {
        result = Graphics_NO_DEVICE_MEM;
      } else if (vkBindImageMemory(device->device, image, memory, 0U) != VK_SUCCESS) {
        vkFreeMemory(device->device, memory, NULL);
        result = Graphics_NO_DEVICE_MEM;
      } else {
        *memory_out = memory;
      }
    }
  }
  return result;
}

/**
 * ------------------------------------------------------------------
 */

Graphics_Result graphics_img_view_create(VkDevice device, VkImage image,
                                     VkFormat format,
                                     VkImageAspectFlags aspect, VkImageView *view_out) {
  Graphics_Result result = Graphics_NONE;
  VkImageViewCreateInfo view_ci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,

      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,

      .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .a = VK_COMPONENT_SWIZZLE_IDENTITY},

      .subresourceRange = {.aspectMask = aspect,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};

  if (device == VK_NULL_HANDLE || image == VK_NULL_HANDLE || view_out == NULL || *view_out != VK_NULL_HANDLE) {
    result = Graphics_ERROR_INVALID_ARGUMENT;
  } else if (vkCreateImageView(device, &view_ci, NULL, view_out) != VK_SUCCESS) {
    *view_out = VK_NULL_HANDLE;
    result = Graphics_NO_IMAGE_VIEW;
  }
  return result;
}

/**
 * ------------------------------------------------------------------
 */

Graphics_Result graphics_create_sampler(Graphics_Device_t *device,
                                        VkSampler *sampler_out) {
  Graphics_Result result = Graphics_NONE;
  if (device == NULL || device->device == VK_NULL_HANDLE || device->physical_device == VK_NULL_HANDLE) {
    result = Graphics_NO_DEVICE;
  } else if (sampler_out == NULL || *sampler_out != VK_NULL_HANDLE) {
    result = Graphics_ERROR_INVALID_ARGUMENT;
  } else {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device->physical_device, &properties);
    VkSamplerCreateInfo sampler_ci = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .anisotropyEnable = VK_TRUE,
      .maxAnisotropy = properties.limits.maxSamplerAnisotropy < 8.0f ? properties.limits.maxSamplerAnisotropy : 8.0f,
      .maxLod = VK_LOD_CLAMP_NONE,
    };
    if (vkCreateSampler(device->device, &sampler_ci, NULL, sampler_out) !=
      VK_SUCCESS) {
      *sampler_out = VK_NULL_HANDLE;
      result = Graphics_NO_SAMPLER;
    }
  }

  return result;
}

/**
 * ------------------------------------------------------------------
 */

static Graphics_Result allocate_descriptor_set(Graphics_Device_t *device,
    const VkDescriptorPoolSize *sizes, uint32_t size_count, Gpu_Resource_t *resource) {
  Graphics_Result result = Graphics_NONE;
  bool allocated = false;
  for (uint32_t i = 0U; i < GRAPHICS_MAX_DESCRIPTOR_POOLS && !allocated && result == Graphics_NONE; ++i) {
    Graphics_Descriptor_Pool_t *pool = &device->descriptor_pools[i];
    bool created = pool->pool == VK_NULL_HANDLE;
    if (created) {
      VkDescriptorPoolSize budgets[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 128U}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128U},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 128U}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 128U},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 128U}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 128U},
      };
      for (uint32_t b = 0U; b < 6U; ++b) {
        for (uint32_t s = 0U; s < size_count; ++s) {
          if (budgets[b].type == sizes[s].type && budgets[b].descriptorCount < sizes[s].descriptorCount) {
            budgets[b].descriptorCount = sizes[s].descriptorCount;
          }
        }
      }
      VkDescriptorPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 64U, .poolSizeCount = 6U, .pPoolSizes = budgets,
      };
      if (vkCreateDescriptorPool(device->device, &info, NULL, &pool->pool) != VK_SUCCESS) {
        pool->pool = VK_NULL_HANDLE;
        result = Graphics_NO_MEMORY;
      } else {
        ++device->stats.descriptor_pool_creations;
        for (uint32_t b = 0U; b < 6U; ++b) {
          pool->available[b] = budgets[b].descriptorCount;
        }
      }
    }
    bool fits = pool->live_sets < 64U;
    for (uint32_t s = 0U; s < size_count && fits; ++s) {
      uint32_t type_index = graphics_descriptor_type_index(sizes[s].type);
      fits = type_index < 6U && pool->available[type_index] >= sizes[s].descriptorCount;
    }
    if (result == Graphics_NONE && fits) {
      VkDescriptorSetAllocateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool->pool, .descriptorSetCount = 1U, .pSetLayouts = &resource->layout,
      };
      VkResult vk_result = vkAllocateDescriptorSets(device->device, &info, &resource->set);
      if (vk_result == VK_SUCCESS) {
        resource->pool = pool->pool;
        ++pool->live_sets;
        for (uint32_t s = 0U; s < size_count; ++s) {
          uint32_t type_index = graphics_descriptor_type_index(sizes[s].type);
          pool->available[type_index] -= sizes[s].descriptorCount;
        }
        allocated = true;
      } else {
        resource->set = VK_NULL_HANDLE;
        if (created) {
          vkDestroyDescriptorPool(device->device, pool->pool, NULL);
          *pool = (Graphics_Descriptor_Pool_t){0};
          result = Graphics_ERROR_RESOURCE;
        } else if (vk_result != VK_ERROR_OUT_OF_POOL_MEMORY && vk_result != VK_ERROR_FRAGMENTED_POOL) {
          result = Graphics_ERROR_RESOURCE;
        }
      }
    }
  }
  if (result == Graphics_NONE && !allocated) {
    result = Graphics_NO_MEMORY;
  }
  return result;
}

/**
 * ------------------------------------------------------------------
 */

Graphics_Result graphics_create_resources(Graphics_Device_t *device,
                                       const Gpu_Resource_Create_Info_t *desc,
                                       Gpu_Resource_t *resource_out) {
  Graphics_Result tResult = Graphics_NONE;
  Gpu_Resource_t resource = {0};
  VkDescriptorSetLayoutBinding bindings[MAX_RESOURCE_BINDINGS] = {0};
  // One pool entry per descriptor type, including repeated types in the layout.
  VkDescriptorPoolSize pool_sizes[6] = {0};
  uint32_t pool_size_count = 0U;
  Gpu_Resource_Create_Info_t resource_info = {0};
  if (desc != NULL) {
    resource_info = *desc;
  }

  if (device == NULL || device->device == VK_NULL_HANDLE ||
      resource_out == NULL || resource_out->set != VK_NULL_HANDLE ||
      resource_info.binding_count == 0U || resource_info.bindings == NULL) {
    tResult = Graphics_ERROR_INVALID_ARGUMENT;
  } else if (resource_info.binding_count > MAX_RESOURCE_BINDINGS) {
    tResult = Graphics_OUT_OF_RANGE;
  }

  for (uint32_t i = 0U;
       (i < resource_info.binding_count) && (tResult == Graphics_NONE); ++i) {
    const Gpu_Binding_Desc_t *binding = &resource_info.bindings[i];
    switch (binding->type) {
      case SAMPLER:
      case IMAGE_SAMPLER:
      case SAMPLED_IMAGE:
      case STORAGE_IMAGE:
      case UNIFORM_BUFFER:
      case STORAGE_BUFFER:
        break;
      default:
        tResult = Graphics_ERROR_INVALID_ARGUMENT;
        break;
    }
    if (binding->stages == 0U || binding->count == 0U) {
      tResult = Graphics_ERROR_INVALID_ARGUMENT;
    }

    if (tResult == Graphics_NONE) {
      VkDescriptorType type = (VkDescriptorType)binding->type;
      bindings[i] = (VkDescriptorSetLayoutBinding){
          .binding = i,
          .descriptorType = type,
          .descriptorCount = binding->count,
          .stageFlags = binding->stages,
      };

      uint32_t pool_index = 0U;
      while ((pool_index < pool_size_count) && (pool_sizes[pool_index].type != type)) {
        ++pool_index;
      }
      if (pool_index == pool_size_count) {
        pool_sizes[pool_index].type = type;
        ++pool_size_count;
      }
      if (pool_sizes[pool_index].descriptorCount >
          UINT32_MAX - binding->count) {
        tResult = Graphics_OUT_OF_RANGE;
      } else {
        pool_sizes[pool_index].descriptorCount += binding->count;
      }
    }
  }

  if (tResult == Graphics_NONE) {
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = resource_info.binding_count,
        .pBindings = bindings,
    };
    if (vkCreateDescriptorSetLayout(device->device, &layout_info, NULL,
                                    &resource.layout) != VK_SUCCESS) {
      resource.layout = VK_NULL_HANDLE;
      tResult = Graphics_ERROR_RESOURCE;
    }
  }

  if (tResult == Graphics_NONE) {
    tResult = allocate_descriptor_set(device, pool_sizes, pool_size_count, &resource);
  }

  if (tResult == Graphics_NONE) {
    resource.owner = device->device;
    resource.binding_count = resource_info.binding_count;
    (void)memcpy(resource.bindings, bindings, resource_info.binding_count * sizeof(bindings[0]));
    *resource_out = resource;
  } else {
    if (resource.layout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(device->device, resource.layout, NULL);
    }
  }

  return tResult;
}

/**
 * ------------------------------------------------------------------
 */
Graphics_Result
graphics_update_resources(Graphics_Device_t *device,
                          Gpu_Resource_Desc_t resource_desc) {
  Graphics_Result result = Graphics_NONE;
  VkDescriptorImageInfo image_info[SMALL_VECTOR_CAPACITY] = {0};
  VkDescriptorBufferInfo buffer_info[SMALL_VECTOR_CAPACITY] = {0};
  VkDescriptorType type = resource_desc.descriptor_type;
  bool buffer_type = type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  if (device == NULL || device->device == VK_NULL_HANDLE) {
    result = Graphics_NO_DEVICE;
  } else if (resource_desc.resource == NULL || resource_desc.resource->owner != device->device ||
             resource_desc.resource->set == VK_NULL_HANDLE || resource_desc.descriptor_count == 0U ||
             (buffer_type ? resource_desc.buffers == NULL : resource_desc.textures == NULL)) {
    result = Graphics_ERROR_INVALID_ARGUMENT;
  } else if (resource_desc.descriptor_count > SMALL_VECTOR_CAPACITY ||
             resource_desc.dst_binding >= resource_desc.resource->binding_count) {
    result = Graphics_OUT_OF_RANGE;
  } else {
    const VkDescriptorSetLayoutBinding *binding = &resource_desc.resource->bindings[resource_desc.dst_binding];
    if (type != binding->descriptorType) {
      result = Graphics_ERROR_INVALID_ARGUMENT;
    } else if (resource_desc.dst_array_elem > binding->descriptorCount ||
               resource_desc.descriptor_count > binding->descriptorCount - resource_desc.dst_array_elem) {
      result = Graphics_OUT_OF_RANGE;
    }
  }
  VkPhysicalDeviceProperties properties = {0};
  if (result == Graphics_NONE && buffer_type) {
    vkGetPhysicalDeviceProperties(device->physical_device, &properties);
  }
  for (uint32_t i = 0U; i < resource_desc.descriptor_count && result == Graphics_NONE; ++i) {
    if (buffer_type) {
      const Gpu_Buffer_t *buffer = &resource_desc.buffers[i];
      VkBufferUsageFlags usage = type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      VkDeviceSize max_range = type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? properties.limits.maxUniformBufferRange : properties.limits.maxStorageBufferRange;
      if (buffer->owner != device->device || buffer->buffer == VK_NULL_HANDLE ||
          (buffer->usage & usage) == 0U || buffer->size == 0U || buffer->size > max_range) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
      } else {
        buffer_info[i] = (VkDescriptorBufferInfo){buffer->buffer, 0U, buffer->size};
      }
    } else {
      const Gpu_Texture_t *texture = &resource_desc.textures[i];
      bool needs_sampler = type == VK_DESCRIPTOR_TYPE_SAMPLER || type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      bool needs_image = type != VK_DESCRIPTOR_TYPE_SAMPLER;
      VkImageUsageFlags usage = type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_SAMPLED_BIT;
      if ((needs_sampler && texture->sampler == VK_NULL_HANDLE) ||
          (needs_image && (texture->owner != device->device || texture->view == VK_NULL_HANDLE ||
           (texture->usage & usage) == 0U ||
           (texture->layout != VK_IMAGE_LAYOUT_GENERAL &&
            (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE || texture->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))))) {
        result = Graphics_ERROR_INVALID_ARGUMENT;
      } else {
        image_info[i] = (VkDescriptorImageInfo){texture->sampler, texture->view, texture->layout};
      }
    }
  }
  if (result == Graphics_NONE) {
    VkWriteDescriptorSet dset = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = NULL,
      .dstSet = resource_desc.resource->set,
      .dstBinding = resource_desc.dst_binding,
      .dstArrayElement = resource_desc.dst_array_elem,
      .descriptorCount = resource_desc.descriptor_count,
      .descriptorType = resource_desc.descriptor_type,
      .pImageInfo = buffer_type ? NULL : image_info,
      .pBufferInfo = buffer_type ? buffer_info : NULL,
      .pTexelBufferView = NULL
    };
    vkUpdateDescriptorSets(device->device, 1U, &dset, 0U, NULL);
  }
  return result;
}

/**
 * ------------------------------------------------------------------
 */

Graphics_Result graphics_update_resources_raw(Graphics_Device_t *device,
                                          VkWriteDescriptorSet descriptor_set) {
  vkUpdateDescriptorSets(device->device, 1, &descriptor_set, 0, NULL);

  return Graphics_NONE;
}

/**
 * ------------------------------------------------------------------
 */

static uint32_t find_memory_type(VkPhysicalDevice physical_device,
                                 uint32_t type_filter,
                                 VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties mem_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
    if ((type_filter & (1u << i)) &&
        (mem_properties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
      return i;
    }
  }

  return UINT32_MAX;
}

/**
 * Command buffer ---------------------------------------------------
 */

Graphics_Result graphics_create_commands(
    Graphics_Device_t *device, uint32_t queue_family,
    VkCommandPool *command_pool_out,
    VkCommandBuffer command_buffers_out[MAX_FRAMES_IN_FLIGHT]) {
  if (device == NULL || device->device == VK_NULL_HANDLE) {
    return Graphics_NO_DEVICE;
  }

  if (command_pool_out == NULL || command_buffers_out == NULL) {
    return Graphics_NO_MEMORY;
  }

  *command_pool_out = VK_NULL_HANDLE;

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    command_buffers_out[i] = VK_NULL_HANDLE;
  }

  VkCommandPoolCreateInfo command_pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queue_family,
  };

  if (vkCreateCommandPool(device->device, &command_pool_info, NULL,
                          command_pool_out) != VK_SUCCESS) {
    return Graphics_NO_MEMORY;
  }

  VkCommandBufferAllocateInfo command_buffer_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = *command_pool_out,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
  };

  if (vkAllocateCommandBuffers(device->device, &command_buffer_info,
                               command_buffers_out) != VK_SUCCESS) {
    vkDestroyCommandPool(device->device, *command_pool_out, NULL);
    *command_pool_out = VK_NULL_HANDLE;

    return Graphics_NO_MEMORY;
  }

  return Graphics_NONE;
}

/**
 * Single time command buffers --------------------------------------
 */

Graphics_Result
graphics_single_time_command_buffer(Graphics_Device_t *tDevice,
                                    VkCommandPool command_pool,
                                    VkCommandBuffer *command_buffer_out) {
  Graphics_Result tResult = Graphics_NONE;
  VkCommandBufferAllocateInfo cb_ci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = command_pool,
      .commandBufferCount = 1};
  if (tDevice == NULL || tDevice->device == VK_NULL_HANDLE) {
    tResult = Graphics_NO_DEVICE;
  } else if (command_buffer_out == NULL || command_pool == VK_NULL_HANDLE) {
    tResult = Graphics_ERROR_INVALID_ARGUMENT;
  } else if (vkAllocateCommandBuffers(tDevice->device, &cb_ci, command_buffer_out) != VK_SUCCESS) {
    *command_buffer_out = VK_NULL_HANDLE;
    tResult = Graphics_NO_COMMAND_BUFFER;
  }

  return tResult;
}

Graphics_Result graphics_begin_command_buffer(VkCommandBuffer command_buffer) {
  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
    return Graphics_NO_MEMORY;
  }

  return Graphics_NONE;
}

// --------------------------------------------------------------- //

Graphics_Result graphics_end_command_buffer(VkCommandBuffer command_buffer) {
  if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
    return Graphics_NO_MEMORY;
  }

  return Graphics_NONE;
}

/**
 * Sync functions ---------------------------------------------------
 */

Graphics_Result graphics_new_semaphore(Graphics_Device_t *device,
                                       VkSemaphore *semaphore_out) {
  Graphics_Result tResult = Graphics_NONE;
  VkSemaphoreCreateInfo semaphore_ci = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  if (device == NULL || device->device == VK_NULL_HANDLE) {
    tResult = Graphics_NO_DEVICE;
  } else if (semaphore_out == NULL || *semaphore_out != VK_NULL_HANDLE) {
    tResult = Graphics_ERROR_INVALID_ARGUMENT;
  } else if (vkCreateSemaphore(device->device, &semaphore_ci, NULL, semaphore_out) !=
      VK_SUCCESS) {
    *semaphore_out = VK_NULL_HANDLE;
    tResult = Graphics_NO_SEMAPHORE;
  }

  return tResult;
}

Graphics_Result graphics_new_fence(Graphics_Device_t *device,
                                   VkFence *fence_out) {
  Graphics_Result tResult = Graphics_NONE;
  VkFenceCreateInfo fence_ci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                .flags = VK_FENCE_CREATE_SIGNALED_BIT};

  if (device == NULL || device->device == VK_NULL_HANDLE) {
    tResult = Graphics_NO_DEVICE;
  } else if (fence_out == NULL || *fence_out != VK_NULL_HANDLE) {
    tResult = Graphics_ERROR_INVALID_ARGUMENT;
  } else if (vkCreateFence(device->device, &fence_ci, NULL, fence_out) != VK_SUCCESS) {
    *fence_out = VK_NULL_HANDLE;
    tResult = Graphics_NO_FENCE;
  }

  return tResult;
}

void graphics_issue_image_barrier(VkCommandBuffer command_buffer, VkImage image,
                                  VkImageAspectFlags aspect_flags,
                                  VkPipelineStageFlags2 src_stage,
                                  VkAccessFlags2 src_access,
                                  VkPipelineStageFlags2 dst_stage,
                                  VkAccessFlags2 dst_access,
                                  VkImageLayout old_layout,
                                  VkImageLayout new_layout) {

  VkImageMemoryBarrier2 image_barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

      .srcStageMask = src_stage,
      .srcAccessMask = src_access,

      .dstStageMask = dst_stage,
      .dstAccessMask = dst_access,

      .oldLayout = old_layout,
      .newLayout = new_layout,

      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

      .image = image,

      .subresourceRange =
          {
              .aspectMask = aspect_flags,
              .baseMipLevel = 0,
              .levelCount = VK_REMAINING_MIP_LEVELS,
              .baseArrayLayer = 0,
              .layerCount = VK_REMAINING_ARRAY_LAYERS,
          },
  };

  VkDependencyInfo dependency_info = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &image_barrier,
  };

  vkCmdPipelineBarrier2(command_buffer, &dependency_info);
}

void graphics_issue_memory_barrier(VkCommandBuffer command_buffer,
                                   VkPipelineStageFlags2 src_stage,
                                   VkAccessFlags2 src_access,
                                   VkPipelineStageFlags2 dst_stage,
                                   VkAccessFlags2 dst_access) {
  VkMemoryBarrier2 memory_barrier = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,

      .srcStageMask = src_stage,
      .srcAccessMask = src_access,

      .dstStageMask = dst_stage,
      .dstAccessMask = dst_access,
  };

  VkDependencyInfo dependency_info = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .memoryBarrierCount = 1,
      .pMemoryBarriers = &memory_barrier,
  };

  vkCmdPipelineBarrier2(command_buffer, &dependency_info);
}

Graphics_Result graphics_start_frame(Graphics_Device_t *device) {
  if (device == NULL || device->device == VK_NULL_HANDLE) {
    return Graphics_NO_DEVICE;
  }

  VkResult result =
      vkWaitForFences(device->device, 1, &device->fences[device->frame_index],
                      VK_TRUE, UINT64_MAX);

  if (result != VK_SUCCESS) {
    return Graphics_ERROR_FENCE;
  }

  result = vkAcquireNextImageKHR(device->device, device->swapchain, UINT64_MAX,
                                 device->image_semaphores[device->frame_index],
                                 VK_NULL_HANDLE, &device->image_index);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    return Graphics_SWAPCHAIN_OUT_OF_DATE;
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    return Graphics_NO_SWAPCHAIN;
  }

  result = vkResetFences(device->device, 1, &device->fences[device->frame_index]);

  if (result != VK_SUCCESS) {
    return Graphics_ERROR_FENCE;
  }

  VkCommandBuffer command_buffer = device->command_buffers[device->frame_index];

  result = vkResetCommandBuffer(command_buffer, 0);

  if (result != VK_SUCCESS) {
    return Graphics_NO_COMMAND_BUFFER;
  }

  Graphics_Result graphics_result = graphics_begin_command_buffer(command_buffer);

  if (graphics_result != Graphics_NONE) {
    return graphics_result;
  }

  graphics_invalidate_state(device);
  graphics_issue_image_barrier(
    command_buffer,
    device->depth_image,
    VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
  );

  graphics_issue_image_barrier(
    command_buffer,
    device->swapchain_images[device->image_index],
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
  );

  return Graphics_NONE;
}

// --------------------------------------------------------------- //

Graphics_Result graphics_end_frame(Graphics_Device_t *device) {
  if (device == NULL || device->device == VK_NULL_HANDLE) {
    return Graphics_NO_DEVICE;
  }

  VkCommandBuffer command_buffer = device->command_buffers[device->frame_index];

  graphics_issue_image_barrier(
    command_buffer,
    device->swapchain_images[device->image_index],
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    0,
    VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  );

  Graphics_Result graphics_result = graphics_end_command_buffer(command_buffer);

  if (graphics_result != Graphics_NONE) {
    return graphics_result;
  }

  VkQueue queue = VK_NULL_HANDLE;

  vkGetDeviceQueue(device->device, device->queue_family, 0, &queue);

  VkSemaphoreSubmitInfo wait_semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = device->image_semaphores[device->frame_index],
      .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
  };

  VkCommandBufferSubmitInfo command_buffer_submit_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
      .commandBuffer = command_buffer,
  };

  VkSemaphoreSubmitInfo signal_semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = device->render_complete_semaphores[device->image_index],
      .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
  };

  VkSubmitInfo2 submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,

      .waitSemaphoreInfoCount = 1,
      .pWaitSemaphoreInfos = &wait_semaphore_info,

      .commandBufferInfoCount = 1,
      .pCommandBufferInfos = &command_buffer_submit_info,

      .signalSemaphoreInfoCount = 1,
      .pSignalSemaphoreInfos = &signal_semaphore_info,
  };

  VkResult result = vkQueueSubmit2(queue, 1, &submit_info,
                                   device->fences[device->frame_index]);

  if (result != VK_SUCCESS) {
    fprintf(stderr, "vkQueueSubmit2: result=%d, image=%u\n", (int)result, device->image_index);
    return Graphics_ERROR_QUEUE_SUBMIT;
  }

  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,

      .waitSemaphoreCount = 1,
      .pWaitSemaphores =
          &device->render_complete_semaphores[device->image_index],

      .swapchainCount = 1,
      .pSwapchains = &device->swapchain,
      .pImageIndices = &device->image_index,
  };

  result = vkQueuePresentKHR(queue, &present_info);

  device->frame_index = (device->frame_index + 1) % MAX_FRAMES_IN_FLIGHT;

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    return Graphics_SWAPCHAIN_OUT_OF_DATE;
  }

  if (result != VK_SUCCESS) {
    return Graphics_ERROR_PRESENT;
  }

  return Graphics_NONE;
}

Graphics_Result graphics_begin_render(Graphics_Device_t *device)
{
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        return Graphics_NO_DEVICE;
    }

    const uint32_t width = device->swapchain_extent.width;
    const uint32_t height = device->swapchain_extent.height;

    VkCommandBuffer cb =
        device->command_buffers[device->frame_index];

    VkClearValue color_clear = {
        .color = {
            .float32 = {0.01f, 0.01f, 0.01f, 1.0f}
        }
    };

    VkClearValue depth_clear = {
        .depthStencil = {
            .depth = 1.0f,
            .stencil = 0
        }
    };

    VkRenderingAttachmentInfo color_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView =
            device->swapchain_img_views[device->image_index],
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = color_clear,
    };

    VkRenderingAttachmentInfo depth_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = device->depth_img_view,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = depth_clear,
    };

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,

        .renderArea = {
            .offset = {0, 0},
            .extent = {
                .width = (uint32_t)width,
                .height = (uint32_t)height,
            },
        },

        .layerCount = 1,

        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,

        .pDepthAttachment = &depth_attachment,
    };

    vkCmdBeginRendering(
        cb,
        &rendering_info
    );

    return Graphics_NONE;
}

Graphics_Result graphics_end_render(Graphics_Device_t *device)
{
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        return Graphics_NO_DEVICE;
    }

    VkCommandBuffer cb =
        device->command_buffers[device->frame_index];

    vkCmdEndRendering(cb);

    return Graphics_NONE;
}

Graphics_Result graphics_push_viewport(Graphics_Device_t *device)
{
    if (device == NULL || device->device == VK_NULL_HANDLE) {
        return Graphics_NO_DEVICE;
    }

    const uint32_t width = device->swapchain_extent.width;
    const uint32_t height = device->swapchain_extent.height;

    VkCommandBuffer cb =
        device->command_buffers[device->frame_index];

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)width,
        .height = (float)height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {
            .width = (uint32_t)width,
            .height = (uint32_t)height,
        },
    };

    vkCmdSetViewportWithCount(
        cb,
        1,
        &viewport
    );

    vkCmdSetScissorWithCount(
        cb,
        1,
        &scissor
    );

    return Graphics_NONE;
}

/**
 * Graphics update swapchain -------------------------------------
 */

Graphics_Result graphics_update_swapchain(Graphics_Device_t *device) {
  if (device == NULL || device->device == VK_NULL_HANDLE) {
    return Graphics_NO_DEVICE;
  }

  int width = 0;
  int height = 0;

  Graphics_Result result = graphics_get_win_size(device, &width, &height);

  if (result != Graphics_NONE) {
    return result;
  }

  if (width == 0 || height == 0) {
    return Graphics_SWAPCHAIN_OUT_OF_DATE;
  }

  // Wait until old swapchain resources are no longer in use.
  if (vkDeviceWaitIdle(device->device) != VK_SUCCESS) {
    return Graphics_NO_DEVICE;
  }

  VkSurfaceCapabilitiesKHR surface_cap = {0};

  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device,
                                                device->surface,
                                                &surface_cap) != VK_SUCCESS) {
    return Graphics_NO_CAPABILITIES;
  }

  if (surface_cap.currentExtent.width != UINT32_MAX) {
    width = (int)surface_cap.currentExtent.width;
    height = (int)surface_cap.currentExtent.height;
  } else {
    if ((uint32_t)width < surface_cap.minImageExtent.width) width = (int)surface_cap.minImageExtent.width;
    if ((uint32_t)height < surface_cap.minImageExtent.height) height = (int)surface_cap.minImageExtent.height;
    if ((uint32_t)width > surface_cap.maxImageExtent.width) width = (int)surface_cap.maxImageExtent.width;
    if ((uint32_t)height > surface_cap.maxImageExtent.height) height = (int)surface_cap.maxImageExtent.height;
  }
  if (width == 0 || height == 0) {
    return Graphics_SWAPCHAIN_OUT_OF_DATE;
  }

  VkSwapchainKHR old_swapchain = device->swapchain;

  VkSwapchainCreateInfoKHR swapchain_ci = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = device->surface,

      .minImageCount = surface_cap.minImageCount,

      .imageFormat = VK_FORMAT_R8G8B8A8_SRGB,
      .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,

      .imageExtent =
          {
              .width = (uint32_t)width,
              .height = (uint32_t)height,
          },

      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,

      .preTransform = surface_cap.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,

      .presentMode = VK_PRESENT_MODE_FIFO_KHR,

      .oldSwapchain = old_swapchain,
  };

  VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
  if (vkCreateSwapchainKHR(device->device, &swapchain_ci, NULL,
                           &new_swapchain) != VK_SUCCESS) {
    return Graphics_NO_SWAPCHAIN;
  }
  device->swapchain = new_swapchain;
  device->swapchain_extent = swapchain_ci.imageExtent;

  for (uint32_t i = 0; i < device->swapchain_image_count; ++i) {

    vkDestroyImageView(device->device, device->swapchain_img_views[i], NULL);
    device->swapchain_img_views[i] = VK_NULL_HANDLE;

    vkDestroySemaphore(device->device, device->render_complete_semaphores[i],
                       NULL);
    device->render_complete_semaphores[i] = VK_NULL_HANDLE;
  }

  vkDestroySwapchainKHR(device->device, old_swapchain, NULL);
  device->swapchain_image_count = 0U;
  if (vkGetSwapchainImagesKHR(device->device, device->swapchain,
                              &device->swapchain_image_count,
                              NULL) != VK_SUCCESS) {
    return Graphics_NO_SWAPCHAIN;
  }

  if (device->swapchain_image_count > SMALL_VECTOR_CAPACITY) {
    device->swapchain_image_count = 0U;
    return Graphics_NO_SWAPCHAIN;
  }

  if (vkGetSwapchainImagesKHR(device->device, device->swapchain,
                              &device->swapchain_image_count,
                              device->swapchain_images) != VK_SUCCESS) {
    return Graphics_NO_SWAPCHAIN;
  }

  for (uint32_t i = 0; i < device->swapchain_image_count; ++i) {

    result = graphics_img_view_create(
        device->device, device->swapchain_images[i], VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_ASPECT_COLOR_BIT, &device->swapchain_img_views[i]);
    if (result != Graphics_NONE) {
      return result;
    }
  }

  for (uint32_t i = 0; i < device->swapchain_image_count; ++i) {

    result =
        graphics_new_semaphore(device, &device->render_complete_semaphores[i]);

    if (result != Graphics_NONE) {
      return result;
    }
  }

  vkDestroyImageView(device->device, device->depth_img_view, NULL);
  device->depth_img_view = VK_NULL_HANDLE;

  vkDestroyImage(device->device, device->depth_image, NULL);
  device->depth_image = VK_NULL_HANDLE;

  vkFreeMemory(device->device, device->depth_img_mem, NULL);
  device->depth_img_mem = VK_NULL_HANDLE;

  // recreate depth image (sublime text format option made this ugly thing)
  VkImageCreateInfo depth_ci = graphics_img_create_info(device->depth_format,
    (VkExtent3D){
      .width = (uint32_t)width,
      .height = (uint32_t)height,
      .depth = 1,
    },
    VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
  );

  if (vkCreateImage(device->device, &depth_ci, NULL, &device->depth_image) !=
      VK_SUCCESS) {
    device->depth_image = VK_NULL_HANDLE;
    return Graphics_NO_DEPTH_FORMAT;
  }

  result = graphics_img_alloc(device, device->depth_image, &device->depth_img_mem);
  if (result == Graphics_NONE) {
    result = graphics_img_view_create(
        device->device, device->depth_image, device->depth_format,
        VK_IMAGE_ASPECT_DEPTH_BIT, &device->depth_img_view);
  }
  return result;
}
