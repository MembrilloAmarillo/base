#include <SDL3/SDL_loadso.h>
#include <SDL3/SDL_video.h>
#include <stdio.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_vulkan.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#include "graphics.h"

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
    .format = FORMAT_UNDEFINED,

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

  VkFormat image_format = VK_FORMAT_B8G8R8A8_SRGB;
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
      tResult = Graphics_NO_DEPTH_FORMAT;
    }

    depth_img_mem = graphics_img_alloc(tDevice, depth_image);
    depth_img_view = graphics_img_view_create(
        tDevice->device, depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

    if (depth_img_mem == VK_NULL_HANDLE) {
      tResult = Graphics_NO_DEVICE_MEM;
    }
    if (depth_img_view == VK_NULL_HANDLE) {
      tResult = Graphics_NO_IMAGE_VIEW;
    }
  }

  if (tResult == Graphics_NONE) {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT && tResult == Graphics_NONE; i++) {
      tResult = graphics_new_semaphore(tDevice, &image_semaphores[i]);
      tResult = graphics_new_fence(tDevice, &fences[i]);
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

  if (desc == NULL || desc->first_stage_spirv == NULL || desc->first_stage_spirv_size == 0) {
    return Graphics_NO_MEMORY;
  }

  if (desc->color_target_count > MAX_COLOR_ATTACHMENTS) {
    return Graphics_NO_MEMORY;
  }

  // pso layout
  if(desc->set_layout) {
    assert(desc->n_ranges < SMALL_VECTOR_CAPACITY);
    assert(desc->n_resources < SMALL_VECTOR_CAPACITY);

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
    return Graphics_ERROR_RESOURCE;
  }
  if (desc->fragment_spirv_size > 0 &&
      vkCreateShaderModule(device->device, &fragment_module_info, NULL,
                           &fragment_module) != VK_SUCCESS) {
    vkDestroyShaderModule(device->device, first_module, NULL);
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

  VkResult vk_result = vkCreateGraphicsPipelines(
    device->device,
    VK_NULL_HANDLE,
    1,
    &pso_info,
    NULL,
    &pso_out->pipeline
  );

  vkDestroyShaderModule(device->device, fragment_module, NULL);
  vkDestroyShaderModule(device->device, first_module, NULL);

  if (vk_result != VK_SUCCESS) {
    pso_out->pipeline = VK_NULL_HANDLE;
    return Graphics_NO_MEMORY;
  }

  return Graphics_NONE;
}

Graphics_Result graphics_bind_pso(Graphics_Device_t *device, VkPipeline pipeline) {
  return graphics_bind_pso_layout(device, pipeline, VK_NULL_HANDLE);
}

Graphics_Result graphics_bind_pso_layout(Graphics_Device_t *device, VkPipeline pipeline, VkPipelineLayout layout) {
  Graphics_Result tResult = Graphics_NONE;

  if(device == NULL) {
    tResult = Graphics_NO_DEVICE;
  } else {
    device->pipeline_layout = layout;
    VkCommandBuffer cmd = device->command_buffers[device->frame_index];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  }

  return tResult;
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
  Graphics_Result tResult = Graphics_NONE;

  VkMemoryPropertyFlags required  = 0;
  VkMemoryPropertyFlags preferred = 0;
  VkMemoryPropertyFlags avoided   = 0;

  switch (alloc_type) {
  case CPU_VISIBLE: {
    required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    preferred = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  } break;

  case GPU_ONLY: {
    required = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    avoided = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  } break;

  case CACHED: {
    required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    preferred = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  } break;
  }

  VkBufferUsageFlags usage =
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
      VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
      VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  if (vkCreateBuffer(device->device, &buffer_info, NULL, &buffer->buffer) !=
      VK_SUCCESS) {
    return Graphics_NO_BUFFER;
  }

  VkMemoryRequirements requirements = {0};
  vkGetBufferMemoryRequirements(device->device, buffer->buffer, &requirements);

  VkPhysicalDeviceMemoryProperties memory_properties = {0};
  vkGetPhysicalDeviceMemoryProperties(device->physical_device,
                                      &memory_properties);

  uint32_t memory_type_index = UINT32_MAX;
  int32_t best_score = INT32_MIN;

  for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
    if ((requirements.memoryTypeBits & (1u << i)) == 0) {
      continue;
    }

    VkMemoryPropertyFlags flags =
        memory_properties.memoryTypes[i].propertyFlags;

    if ((flags & required) != required) {
      continue;
    }

    int32_t score = 0;

    if (preferred != 0) {
      VkMemoryPropertyFlags preferred_flags = flags & preferred;

      while (preferred_flags != 0) {
        score += (preferred_flags & 1u) ? 1 : 0;
        preferred_flags >>= 1u;
      }
    }

    if (avoided != 0 && (flags & avoided) != 0) {
      score -= 1000;
    }

    if (memory_type_index == UINT32_MAX || score > best_score) {
      memory_type_index = i;
      best_score = score;
    }
  }

  if (memory_type_index == UINT32_MAX) {
    vkDestroyBuffer(device->device, buffer->buffer, NULL);
    buffer->buffer = VK_NULL_HANDLE;

    return Graphics_NO_DEVICE_MEM;
  }

  VkMemoryAllocateFlagsInfo allocation_flags = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
  };

  VkMemoryAllocateInfo allocation_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &allocation_flags,
      .allocationSize = requirements.size,
      .memoryTypeIndex = memory_type_index,
  };

  if (vkAllocateMemory(device->device, &allocation_info, NULL,
                       &buffer->memory) != VK_SUCCESS) {
    vkDestroyBuffer(device->device, buffer->buffer, NULL);

    buffer->buffer = VK_NULL_HANDLE;
    buffer->memory = VK_NULL_HANDLE;

    return Graphics_NO_DEVICE_MEM;
  }

  if (vkBindBufferMemory(device->device, buffer->buffer, buffer->memory, 0) !=
      VK_SUCCESS) {
    vkFreeMemory(device->device, buffer->memory, NULL);
    vkDestroyBuffer(device->device, buffer->buffer, NULL);

    buffer->buffer = VK_NULL_HANDLE;
    buffer->memory = VK_NULL_HANDLE;

    return Graphics_NO_DEVICE_MEM;
  }

  buffer->size = size;
  buffer->usage = usage;
  buffer->type = memory_properties.memoryTypes[memory_type_index].propertyFlags;
  buffer->mapped = NULL;
  buffer->address = 0;

  if ((buffer->type & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
    if (vkMapMemory(device->device, buffer->memory, 0, VK_WHOLE_SIZE, 0,
                    &buffer->mapped) != VK_SUCCESS) {
      vkFreeMemory(device->device, buffer->memory, NULL);
      vkDestroyBuffer(device->device, buffer->buffer, NULL);

      buffer->buffer = VK_NULL_HANDLE;
      buffer->memory = VK_NULL_HANDLE;
      buffer->mapped = NULL;

      return Graphics_NO_MEMORY;
    }
  }

  VkBufferDeviceAddressInfo address_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = buffer->buffer,
  };

  buffer->address = vkGetBufferDeviceAddress(device->device, &address_info);

  return tResult;
}

Graphics_Result graphics_memcpy(Graphics_Device_t *device,
                                Gpu_Buffer_t *buffer_dst, void *data,
                                Gpu_Range_t range) {

    if (buffer_dst->mapped && (range.size - range.offset) <= buffer_dst->size) {
      memcpy(buffer_dst->mapped + range.offset, data, range.size);
      return Graphics_NONE;
    }
    return !buffer_dst->mapped ? Graphics_NO_MAPPEABLE : Graphics_OUT_OF_RANGE;
}

Graphics_Result graphics_copy(Graphics_Device_t *device, Gpu_Buffer_t *buffer_dst, Gpu_Buffer_t *buffer_src, Gpu_Range_t range, VkCommandBuffer cmd) {

    bool local_cmd = false;
    if (cmd == VK_NULL_HANDLE) {
        local_cmd = true;
        graphics_single_time_command_buffer(device, device->command_pool, &cmd);
    }
    
    if (buffer_dst->size <= range.size && buffer_src->size <= range.size) {
        VkBufferCopy copy_reg = (VkBufferCopy){.srcOffset = range.offset,
            .dstOffset = range.offset,
            .size = range.size};
        if(local_cmd) {graphics_begin_command_buffer(cmd);}
        vkCmdCopyBuffer(cmd, buffer_src->buffer, buffer_dst->buffer, 1,
                        &copy_reg);
        if(local_cmd) {graphics_end_command_buffer(cmd);}
        return Graphics_NONE;
    }
    return Graphics_NO_MEMORY;
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

Gpu_Texture_t graphics_create_texture(
    Graphics_Device_t *device,
    int width,
    int height
    ) {
    VkImageCreateInfo img_info = graphics_img_create_info(
        VK_FORMAT_R8G8B8A8_UINT , (VkExtent3D){width, height, 1},
        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    VkImage image = VK_NULL_HANDLE;
    VkResult vk_result = vkCreateImage(device->device, &img_info, NULL, &image);
    assert(vk_result == VK_SUCCESS);

    VkDeviceMemory img_memory = graphics_img_alloc(device, image);
    VkImageView img_view = graphics_img_view_create(device->device, image,
                                                    VK_FORMAT_R8G8B8A8_UINT ,
                                                    VK_IMAGE_ASPECT_COLOR_BIT);
    assert(img_view != VK_NULL_HANDLE);

    return (Gpu_Texture_t){
        .image = image,
        .view = img_view,
        .memory = img_memory,
        .format = VK_FORMAT_R8G8B8A8_UINT ,
        .array_layers = 1,
        .mip_levels   = 1,
        .barrier_aspects = VK_IMAGE_ASPECT_COLOR_BIT,
        .extent = (VkExtent3D){(uint32_t)width, (uint32_t)height, 1},
        .layout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
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
  if (tDevice == NULL || tDevice->device == VK_NULL_HANDLE ||
      tDevice->physical_device == VK_NULL_HANDLE) {
    return Graphics_NO_DEVICE;
  }
  if (tTexture == NULL || data == NULL ||
      tDevice->command_pool == VK_NULL_HANDLE ||
      tTexture->image == VK_NULL_HANDLE ||
      tTexture->format != VK_FORMAT_R8G8B8A8_UINT ||
      tTexture->extent.width == 0 || tTexture->extent.height == 0 ||
      tTexture->extent.depth != 1 || tTexture->mip_levels != 1 ||
      tTexture->array_layers != 1 ||
      tTexture->barrier_aspects != VK_IMAGE_ASPECT_COLOR_BIT ||
      (tTexture->layout != VK_IMAGE_LAYOUT_UNDEFINED &&
       tTexture->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
    return Graphics_ERROR_INVALID_ARGUMENT;
  }

  size_t width = tTexture->extent.width;
  size_t height = tTexture->extent.height;
  if (width > SIZE_MAX / 4 || height > SIZE_MAX / (width * 4) ||
      data_size != width * height * 4) {
    return Graphics_ERROR_INVALID_ARGUMENT;
  }

  Gpu_Buffer_t staging = {0};
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  Graphics_Result tResult = graphics_alloc(tDevice, &staging, data_size, CPU_VISIBLE);
  if (tResult != Graphics_NONE) {
    return tResult;
  }
  memcpy(staging.mapped, data, data_size);

  tResult = graphics_single_time_command_buffer(
      tDevice, tDevice->command_pool, &command_buffer);
  if (tResult != Graphics_NONE) {
    goto cleanup;
  }
  tResult = graphics_begin_command_buffer(command_buffer);
  if (tResult != Graphics_NONE) {
    goto cleanup;
  }

  bool first_upload = tTexture->layout == VK_IMAGE_LAYOUT_UNDEFINED;
  graphics_issue_image_barrier(
      command_buffer, tTexture->image,
      first_upload ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      first_upload ? VK_ACCESS_2_NONE : VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
      tTexture->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkBufferImageCopy copy = {
      .imageSubresource = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel = 0,
          .baseArrayLayer = 0,
          .layerCount = 1,
      },
      .imageExtent = tTexture->extent,
  };
  vkCmdCopyBufferToImage(command_buffer, staging.buffer, tTexture->image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  graphics_issue_image_barrier(
      command_buffer, tTexture->image,
      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  tResult = graphics_end_command_buffer(command_buffer);
  if (tResult != Graphics_NONE) {
    goto cleanup;
  }

  VkQueue queue = VK_NULL_HANDLE;
  vkGetDeviceQueue(tDevice->device, tDevice->queue_family, 0, &queue);
  VkCommandBufferSubmitInfo command_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
      .commandBuffer = command_buffer,
  };
  VkSubmitInfo2 submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .commandBufferInfoCount = 1,
      .pCommandBufferInfos = &command_info,
  };
  if (vkQueueSubmit2(queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
    tResult = Graphics_ERROR_QUEUE_SUBMIT;
    goto cleanup;
  }
  if (vkQueueWaitIdle(queue) != VK_SUCCESS) {
    tResult = Graphics_ERROR_FENCE;
    goto cleanup;
  }
  tTexture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

cleanup:
  if (command_buffer != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(tDevice->device, tDevice->command_pool, 1, &command_buffer);
  }
  vkUnmapMemory(tDevice->device, staging.memory);
  vkDestroyBuffer(tDevice->device, staging.buffer, NULL);
  vkFreeMemory(tDevice->device, staging.memory, NULL);

  return tResult;
}

/**
 * ------------------------------------------------------------------
 */

VkDeviceMemory graphics_img_alloc(Graphics_Device_t *tDevice, VkImage image) {
  VkMemoryRequirements memory_req;

  vkGetImageMemoryRequirements(tDevice->device, image, &memory_req);

  VkMemoryAllocateInfo mem_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = NULL,

      .allocationSize = memory_req.size,

      .memoryTypeIndex =
          find_memory_type(tDevice->physical_device, memory_req.memoryTypeBits,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

  VkDeviceMemory img_memory = VK_NULL_HANDLE;

  VkResult result =
      vkAllocateMemory(tDevice->device, &mem_alloc_info, NULL, &img_memory);

  assert(result == VK_SUCCESS);

  result = vkBindImageMemory(tDevice->device, image, img_memory, 0);

  assert(result == VK_SUCCESS);

  return img_memory;
}

/**
 * ------------------------------------------------------------------
 */

VkImageView graphics_img_view_create(VkDevice device, VkImage image,
                                     VkFormat format,
                                     VkImageAspectFlags aspect) {
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

  VkImageView img_view = VK_NULL_HANDLE;

  VkResult result = vkCreateImageView(device, &view_ci, NULL, &img_view);

  assert(result == VK_SUCCESS);

  return img_view;
}

/**
 * ------------------------------------------------------------------
 */

Graphics_Result graphics_create_sampler(Graphics_Device_t *device,
                                        VkSampler *sampler_out) {
  Graphics_Result tResult = Graphics_NONE;
  if (device == NULL || device->device == VK_NULL_HANDLE || sampler_out == NULL) {
    return Graphics_ERROR_INVALID_ARGUMENT;
  }
  VkSamplerCreateInfo sampler_ci = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .anisotropyEnable = VK_TRUE,
      .maxAnisotropy = 8.0f,
      .maxLod = VK_LOD_CLAMP_NONE,
  };
  if (vkCreateSampler(device->device, &sampler_ci, NULL, sampler_out) !=
      VK_SUCCESS) {
    tResult = Graphics_NO_SAMPLER;
  }

  return tResult;
}

/**
 * ------------------------------------------------------------------
 */

Graphics_Result graphics_create_resources(Graphics_Device_t *device,
                                          uint32_t n_descriptors,
                                          Gpu_Resource_t *resource_out) {
  if (device == NULL || resource_out == NULL || n_descriptors == 0) {
    return Graphics_ERROR_INVALID_ARGUMENT;
  }

  *resource_out = (Gpu_Resource_t){0};

  VkDescriptorSetLayoutBinding binding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = n_descriptors,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = NULL,
  };

  VkDescriptorSetLayoutBinding binding2 = {
      .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = n_descriptors,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
      .pImmutableSamplers = NULL,
  };

  VkDescriptorSetLayoutBinding binding3 = {
      .binding = 2,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = n_descriptors,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
      .pImmutableSamplers = NULL,
  };

  VkDescriptorSetLayoutBinding bindings[] = {binding, binding2, binding3};

  VkDescriptorSetLayoutCreateInfo layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .bindingCount = 3,
      .pBindings = bindings,
  };

  VkResult result = vkCreateDescriptorSetLayout(device->device, &layout_info,
                                                NULL, &resource_out->layout);

  if (result != VK_SUCCESS) {
    goto error;
  }

  VkDescriptorPoolSize pool_sizes[] = {
      {
          .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = n_descriptors,
      },
      {
          .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = n_descriptors,
      },
      {
          .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = n_descriptors,
      },
  };

  VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .maxSets = 1,
      .poolSizeCount = 3,
      .pPoolSizes = pool_sizes,
  };

  result = vkCreateDescriptorPool(device->device, &pool_info, NULL,
                                  &resource_out->pool);

  if (result != VK_SUCCESS) {
    goto error;
  }

  VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = NULL,
      .descriptorPool = resource_out->pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &resource_out->layout,
  };

  result = vkAllocateDescriptorSets(device->device, &alloc_info, &resource_out->set);

  if (result != VK_SUCCESS) {
    goto error;
  }

  return Graphics_NONE;

error:

  if (resource_out->pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device->device, resource_out->pool, NULL);
  }

  if (resource_out->layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device->device, resource_out->layout, NULL);
  }

  *resource_out = (Gpu_Resource_t){0};

  return Graphics_ERROR_RESOURCE;
}

/**
 * ------------------------------------------------------------------
 */

Graphics_Result graphics_update_resources(Graphics_Device_t *device,
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

  assert(!"Failed to find suitable memory type");
  return 0;
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
  if (vkAllocateCommandBuffers(tDevice->device, &cb_ci, command_buffer_out) !=
      VK_SUCCESS) {
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
  if (vkCreateSemaphore(device->device, &semaphore_ci, NULL, semaphore_out) !=
      VK_SUCCESS) {
    tResult = Graphics_NO_SEMAPHORE;
  }

  return tResult;
}

Graphics_Result graphics_new_fence(Graphics_Device_t *device,
                                   VkFence *fence_out) {
  Graphics_Result tResult = Graphics_NONE;
  VkFenceCreateInfo fence_ci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                .flags = VK_FENCE_CREATE_SIGNALED_BIT};

  if (vkCreateFence(device->device, &fence_ci, NULL, fence_out) != VK_SUCCESS) {
    tResult = Graphics_NO_FENCE;
  }

  return tResult;
}

void graphics_issue_image_barrier(VkCommandBuffer command_buffer, VkImage image,
                                  VkPipelineStageFlags2 src_stage,
                                  VkAccessFlags2 src_access,
                                  VkPipelineStageFlags2 dst_stage,
                                  VkAccessFlags2 dst_access,
                                  VkImageLayout old_layout,
                                  VkImageLayout new_layout) {

  if (src_access == VK_ACCESS_2_TRANSFER_WRITE_BIT &&
      dst_access == VK_ACCESS_2_SHADER_READ_BIT) {
    old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    new_layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
  }

  VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;

  if(src_access == VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) {
    aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  } else if(src_access == VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT) {
    aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  }

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

  result =
      vkResetFences(device->device, 1, &device->fences[device->frame_index]);

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

  graphics_issue_image_barrier(
    command_buffer,
    device->depth_image,
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

      .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
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

  if (vkCreateSwapchainKHR(device->device, &swapchain_ci, NULL,
                           &device->swapchain) != VK_SUCCESS) {
    return Graphics_NO_SWAPCHAIN;
  }
  device->swapchain_extent = swapchain_ci.imageExtent;

  for (uint32_t i = 0; i < device->swapchain_image_count; ++i) {

    vkDestroyImageView(device->device, device->swapchain_img_views[i], NULL);

    vkDestroySemaphore(device->device, device->render_complete_semaphores[i],
                       NULL);
  }

  if (vkGetSwapchainImagesKHR(device->device, device->swapchain,
                              &device->swapchain_image_count,
                              NULL) != VK_SUCCESS) {
    return Graphics_NO_SWAPCHAIN;
  }

  if (device->swapchain_image_count > SMALL_VECTOR_CAPACITY) {
    return Graphics_NO_SWAPCHAIN;
  }

  if (vkGetSwapchainImagesKHR(device->device, device->swapchain,
                              &device->swapchain_image_count,
                              device->swapchain_images) != VK_SUCCESS) {
    return Graphics_NO_SWAPCHAIN;
  }

  for (uint32_t i = 0; i < device->swapchain_image_count; ++i) {

    device->swapchain_img_views[i] = graphics_img_view_create(
        device->device, device->swapchain_images[i], VK_FORMAT_B8G8R8A8_SRGB,
        VK_IMAGE_ASPECT_COLOR_BIT);
  }

  for (uint32_t i = 0; i < device->swapchain_image_count; ++i) {

    result =
        graphics_new_semaphore(device, &device->render_complete_semaphores[i]);

    if (result != Graphics_NONE) {
      return result;
    }
  }

  // Old swapchain can now go
  vkDestroySwapchainKHR(device->device, old_swapchain, NULL);

  vkDestroyImageView(device->device, device->depth_img_view, NULL);

  vkDestroyImage(device->device, device->depth_image, NULL);

  vkFreeMemory(device->device, device->depth_img_mem, NULL);

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
    return Graphics_NO_DEPTH_FORMAT;
  }

  device->depth_img_mem = graphics_img_alloc(device, device->depth_image);

  device->depth_img_view = graphics_img_view_create(
      device->device, device->depth_image, device->depth_format,
      VK_IMAGE_ASPECT_DEPTH_BIT);

  return Graphics_NONE;
}
