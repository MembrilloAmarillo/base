#include "vulkan_impl.h"

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

// ------------------------------------------------------------------

global void FramebufferResizeCallback(GLFWwindow *window, int width,
                                      int height) {
  vulkan_iface *app =
      reinterpret_cast<vulkan_iface *>(glfwGetWindowUserPointer(window));
  app->FramebufferResized = true;
}

// -------------------------- CLASS FUNCTIONS -----------------------
//

VkSemaphoreCreateInfo
vulkan_iface::SemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0) {
  VkSemaphoreCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = flags;
  return info;
}

// ------------------------------------------------------------------

VkFenceCreateInfo vulkan_iface::FenceCreateInfo(VkFenceCreateFlags flags = 0) {
  VkFenceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  info.pNext = nullptr;

  info.flags = flags;

  return info;
}

// ------------------------------------------------------------------

void vulkan_iface::InitSyncStructures() {
  // create syncronization structures
  // one fence to control when the gpu has finished rendering the frame,
  // and 2 semaphores to syncronize rendering with swapchain
  // we want the fence to start signalled so we can wait on it on the first
  // frame
  VkFenceCreateInfo fenceCreateInfo =
      FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
  VkSemaphoreCreateInfo semaphoreCreateInfo = SemaphoreCreateInfo();

  Semaphores.ImageAvailable.Init(RenderArena,  MAX_FRAMES_IN_FLIGHT);
  Semaphores.RenderFinished.Init(RenderArena,  MAX_FRAMES_IN_FLIGHT);
  Semaphores.ComputeFinished.Init(RenderArena, MAX_FRAMES_IN_FLIGHT);
  Semaphores.InFlight.Init(RenderArena,        MAX_FRAMES_IN_FLIGHT);
  Semaphores.ComputeInFlight.Init(RenderArena, MAX_FRAMES_IN_FLIGHT);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_CHECK(vkCreateFence(Device.LogicalDevice, &fenceCreateInfo, nullptr,
                           &Semaphores.InFlight.At(i)));

    VK_CHECK(vkCreateSemaphore(Device.LogicalDevice, &semaphoreCreateInfo,
                               nullptr, &Semaphores.ImageAvailable.At(i)));
    VK_CHECK(vkCreateSemaphore(Device.LogicalDevice, &semaphoreCreateInfo,
                               nullptr, &Semaphores.RenderFinished.At(i)));
    VK_CHECK(vkCreateFence(Device.LogicalDevice, &fenceCreateInfo, nullptr,
                           &Semaphores.ComputeInFlight.At(i)));

    VK_CHECK(vkCreateSemaphore(Device.LogicalDevice, &semaphoreCreateInfo,
                               nullptr, &Semaphores.ComputeFinished.At(i)));
  }
}

// ------------------------------------------------------------------

void vulkan_iface::TransitionImage(VkCommandBuffer cmd, VkImage image,
                                   VkImageLayout currentLayout,
                                   VkImageLayout newLayout) {
  VkImageMemoryBarrier2 imageBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  imageBarrier.pNext = nullptr;

  imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.dstAccessMask =
      VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

  imageBarrier.oldLayout = currentLayout;
  imageBarrier.newLayout = newLayout;

  VkImageAspectFlags aspectMask =
      (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
          ? VK_IMAGE_ASPECT_DEPTH_BIT
          : VK_IMAGE_ASPECT_COLOR_BIT;

  VkImageSubresourceRange subImage{};
  subImage.aspectMask = aspectMask;
  subImage.baseMipLevel = 0;
  subImage.levelCount = VK_REMAINING_MIP_LEVELS;
  subImage.baseArrayLayer = 0;
  subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

  imageBarrier.subresourceRange = subImage;
  imageBarrier.image = image;

  VkDependencyInfo depInfo{};
  depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  depInfo.pNext = nullptr;

  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers = &imageBarrier;

  vkCmdPipelineBarrier2(cmd, &depInfo);
}

// ------------------------------------------------------------------

void vulkan_iface::DrawGeometry(VkCommandBuffer cmd) {
  vk_image *DrawImage = &TextureImage[0];
  // begin a render pass  connected to our draw image
  VkRenderingAttachmentInfo colorAttachment = AttachmentInfo(
      DrawImage->ImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkExtent2D DrawExtent = {DrawImage->Width, DrawImage->Height};
  VkRenderingInfo renderInfo =
      RenderingInfo(DrawExtent, &colorAttachment, nullptr);
  vkCmdBeginRendering(cmd, &renderInfo);
#if 0
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, TrianglePipeline);

	//set dynamic viewport and scissor
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = DrawExtent.width;
	viewport.height = DrawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = DrawExtent.width;
	scissor.extent.height = DrawExtent.height;

	vkCmdSetScissor(cmd, 0, 1, &scissor);

	//launch a draw command to draw 3 vertices
	vkCmdDraw(cmd, 3, 1, 0, 0);
#endif

  for (U32 i = 0; i < Pipelines.GetLength(); i += 1) {
    VkPipeline Pipe = Pipelines.At(i).Pipeline;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipe);

    // set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = DrawExtent.width;
    viewport.height = DrawExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = DrawExtent.width;
    scissor.extent.height = DrawExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // launch a draw command to draw 3 vertices
    vkCmdDraw(cmd, 3, 1, 0, 0);
  }

  vkCmdEndRendering(cmd);
}

// ------------------------------------------------------------------

VkCommandBufferBeginInfo
vulkan_iface::CommandBufferBeginInfo(VkCommandBufferUsageFlags flags) {
  VkCommandBufferBeginInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  info.pNext = nullptr;

  info.pInheritanceInfo = nullptr;
  info.flags = flags;
  return info;
}

// ------------------------------------------------------------------

vk_buffer vulkan_iface::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;

	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;
	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	vk_buffer newBuffer;

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(this->GPUAllocator, &bufferInfo, &vmaallocInfo, &newBuffer.Buffer, &newBuffer.Allocation,
		&newBuffer.AllocationInfo));

	return newBuffer;
}
// ------------------------------------------------------------------

void vulkan_iface::ImmediateSubmit(
    std::function<void(VkCommandBuffer cmd)> &&function) {
  VK_CHECK(vkResetFences(Device.LogicalDevice, 1, &ImmFence));
  VK_CHECK(vkResetCommandBuffer(ImmCommandBuffer, 0));

  VkCommandBuffer cmd = ImmCommandBuffer;

  VkCommandBufferBeginInfo cmdBeginInfo =
      CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  function(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdinfo = CommandBufferSubmitInfo(cmd);
  VkSubmitInfo2 submit = SubmitInfo(&cmdinfo, nullptr, nullptr);

  // submit command buffer to the queue and execute it.
  //  _renderFence will now block until the graphic commands finish execution
  VK_CHECK(vkQueueSubmit2(Device.GraphicsQueue, 1, &submit, ImmFence));

  VK_CHECK(
      vkWaitForFences(Device.LogicalDevice, 1, &ImmFence, true, 9999999999));
}

// ------------------------------------------------------------------

void vulkan_iface::UploadMesh(vector<U32> &indices,
                              vector<vertex2d> &vertices) {
  const U64 vertexBufferSize = vertices.GetLength() * sizeof(vertex2d);
  const U64 indexBufferSize = indices.GetLength() * sizeof(U32);

  gpu_mesh_buffer NewSurface;

  NewSurface.VertexBuffer = AllocateBuffer(
      vertexBufferSize,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);

  VkBufferDeviceAddressInfo deviceAdressInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = NewSurface.VertexBuffer.Buffer};

  NewSurface.VertexBufferAddress =
      vkGetBufferDeviceAddress(Device.LogicalDevice, &deviceAdressInfo);

  NewSurface.IndexBuffer = AllocateBuffer(indexBufferSize,
                                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                          VMA_MEMORY_USAGE_GPU_ONLY);

  vk_buffer staging = AllocateBuffer(vertexBufferSize + indexBufferSize,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VMA_MEMORY_USAGE_CPU_ONLY);

  void *data = staging.Allocation->GetMappedData();

  // copy vertex buffer
  memcpy(data, vertices.GetData(), vertexBufferSize);
  // copy index buffer
  memcpy((char *)data + vertexBufferSize, indices.GetData(), indexBufferSize);

  ImmediateSubmit([&](VkCommandBuffer cmd) {
    VkBufferCopy vertexCopy{0};
    vertexCopy.dstOffset = 0;
    vertexCopy.srcOffset = 0;
    vertexCopy.size = vertexBufferSize;

    vkCmdCopyBuffer(cmd, staging.Buffer, NewSurface.VertexBuffer.Buffer, 1,
                    &vertexCopy);

    VkBufferCopy indexCopy{0};
    indexCopy.dstOffset = 0;
    indexCopy.srcOffset = vertexBufferSize;
    indexCopy.size = indexBufferSize;

    vkCmdCopyBuffer(cmd, staging.Buffer, NewSurface.IndexBuffer.Buffer, 1,
                    &indexCopy);
  });

  DestroyBuffer(staging);
}

// ------------------------------------------------------------------

void vulkan_iface::BeginDrawing() {
  U32 FrameIdx = CurrentFrame;
  // wait until the gpu has finished rendering the last frame. Timeout of 1
  // second
  VK_CHECK(vkWaitForFences(Device.LogicalDevice, 1,
                           &Semaphores.InFlight.At(FrameIdx), true, 1000000000));
  VK_CHECK(
      vkResetFences(Device.LogicalDevice, 1, &Semaphores.InFlight.At(FrameIdx)));
  uint32_t swapchainImageIndex;
  VkResult result = vkAcquireNextImageKHR(
      Device.LogicalDevice, Swapchain.Swapchain, 1000000000,
      Semaphores.ImageAvailable.At(FrameIdx), nullptr, &swapchainImageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    // recreate_swapchain(vi)
    printf("[TODO] We have to recreate swapchain!!\n");
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    printf("[ERROR] Could not acquire next image");
    exit(1);
  }

  VkCommandBufferBeginInfo cmdBeginInfo = {};
  cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cmdBeginInfo.pNext = nullptr;

  cmdBeginInfo.pInheritanceInfo = nullptr;
  cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  // naming it cmd for shorter writing
  VkCommandBuffer cmd = CommandBuffers[FrameIdx];

  //VK_CHECK(
  //    vkResetFences(Device.LogicalDevice, 1, &Semaphores.InFlight.At(FrameIdx)));

  // now that we are sure that the commands finished executing, we can safely
  // reset the command buffer to begin recording again.
  VK_CHECK(vkResetCommandBuffer(cmd, 0));

  // start the command buffer recording
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  vk_image *DrawImage = &TextureImage[0];
  TransitionImage(cmd, DrawImage->Image, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_GENERAL);

  DrawBackground(cmd);

  TransitionImage(cmd, DrawImage->Image, VK_IMAGE_LAYOUT_GENERAL,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  DrawGeometry(cmd);

  TransitionImage(cmd, DrawImage->Image,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  TransitionImage(cmd, Swapchain.Images[swapchainImageIndex],
                  VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkExtent2D DrawExtent = {DrawImage->Width, DrawImage->Height};
  CopyImageToImage(cmd, DrawImage->Image, Swapchain.Images[swapchainImageIndex],
                   DrawExtent, Swapchain.Extent);
  TransitionImage(cmd, Swapchain.Images[swapchainImageIndex],
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  DrawImgui(cmd, Swapchain.ImageViews[swapchainImageIndex]);

  TransitionImage(cmd, Swapchain.Images[swapchainImageIndex],
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // finalize the command buffer (we can no longer add commands, but it can now
  // be executed)
  VK_CHECK(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdinfo = CommandBufferSubmitInfo(cmd);

  VkSemaphoreSubmitInfo waitInfo =
      SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                          Semaphores.ImageAvailable.At(FrameIdx));
  VkSemaphoreSubmitInfo signalInfo =
      SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                          Semaphores.RenderFinished.At(FrameIdx));

  VkSubmitInfo2 submit = SubmitInfo(&cmdinfo, &signalInfo, &waitInfo);

  VK_CHECK(vkQueueSubmit2(Device.GraphicsQueue, 1, &submit,
                          Semaphores.InFlight.At(FrameIdx)));

  // prepare present
  //  this will put the image we just rendered to into the visible window.
  //  we want to wait on the _renderSemaphore for that,
  //  as its necessary that drawing commands have finished before the image is
  //  displayed to the user
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = nullptr;
  presentInfo.pSwapchains = &Swapchain.Swapchain;
  presentInfo.swapchainCount = 1;

  presentInfo.pWaitSemaphores = &Semaphores.RenderFinished.At(FrameIdx);
  presentInfo.waitSemaphoreCount = 1;

  presentInfo.pImageIndices = &swapchainImageIndex;

  result = vkQueuePresentKHR(Device.PresentationQueue, &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      FramebufferResized) {
    FramebufferResized = false;
    printf("[TODO] We have to recreate swapchain!!\n");
    return;
  } else if (result != VK_SUCCESS) {
    printf("[ERROR] Failed to present swapchain image");
    exit(1);
  }

  // increase the number of frames drawn
  CurrentFrame = (CurrentFrame+1) % MAX_FRAMES_IN_FLIGHT;
}

// ------------------------------------------------------------------

VkCommandPoolCreateInfo
vulkan_iface::CommandPoolCreateInfo(U32 queueFamilyIndex,
                                    VkCommandPoolCreateFlags flags /*= 0*/) {
  VkCommandPoolCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  info.pNext = nullptr;
  info.queueFamilyIndex = queueFamilyIndex;
  info.flags = flags;
  return info;
}

// ------------------------------------------------------------------

VkCommandBufferAllocateInfo
vulkan_iface::CommandBufferAllocateInfo(VkCommandPool pool,
                                        uint32_t count /*= 1*/) {
  VkCommandBufferAllocateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.pNext = nullptr;

  info.commandPool = pool;
  info.commandBufferCount = count;
  info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  return info;
}

// ------------------------------------------------------------------

void vulkan_iface::InitCommands() {
  VkCommandPoolCreateInfo commandPoolInfo =
      CommandPoolCreateInfo(Device.FamilyIndices.GraphicsAndCompute,
                            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_CHECK(vkCreateCommandPool(Device.LogicalDevice, &commandPoolInfo,
                                 nullptr, &CommandPool[i]));

    // allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo =
        CommandBufferAllocateInfo(CommandPool[i], 1);

    VK_CHECK(vkAllocateCommandBuffers(Device.LogicalDevice, &cmdAllocInfo,
                                      &CommandBuffers[i]));
  }
}

// ------------------------------------------------------------------

void vulkan_iface::CreateImageViews() {
  Swapchain.N_ImageViews = Swapchain.N_Images;
  if (Swapchain.ImageViews == nullptr) {
    Swapchain.ImageViews =
        RenderArena->Push<VkImageView>(Swapchain.N_ImageViews);
  }
  for (U32 i = 0; i < Swapchain.N_ImageViews; ++i) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = Swapchain.Images[i];
    ;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = Swapchain.Format;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(Device.LogicalDevice, &createInfo, nullptr,
                          &Swapchain.ImageViews[i]) != VK_SUCCESS) {
      fprintf(stderr, "[ERROR] Could not create image view\n");
      exit(1);
    }
  }
}

// ------------------------------------------------------------------

void vulkan_iface::CreateSwapchain() {
  swapchain_support_details swap_chain_support =
      QuerySwapChainSupport(Device.PhysicalDevice);

  U32 imageCount = swap_chain_support.Capabilities.minImageCount + 1;

  VkSurfaceFormatKHR surface_format;
  VkPresentModeKHR present_mode;
  VkExtent2D extent;

  if (swap_chain_support.Capabilities.maxImageCount > 0 &&
      imageCount > swap_chain_support.Capabilities.maxImageCount) {
    imageCount = swap_chain_support.Capabilities.maxImageCount;
  }

  for (U32 i = 0; i < swap_chain_support.FormatCount; ++i) {
    if (swap_chain_support.Formats[i].format == VK_FORMAT_R8G8B8A8_UNORM &&
        swap_chain_support.Formats[i].colorSpace ==
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surface_format = swap_chain_support.Formats[i];
      break;
    }
  }

  bool found_mailbox = false;

#ifndef ENABLE_VSYNC

  for (U32 i = 0; i < swap_chain_support.PresentModeCount; ++i) {
    if (swap_chain_support.PresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
      present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
      found_mailbox = true;
      break;
    }
  }

#endif

  if (!found_mailbox) {
    present_mode = VK_PRESENT_MODE_FIFO_KHR;
  }

  if (swap_chain_support.Capabilities.currentExtent.width != U32_MAX) {
    extent = swap_chain_support.Capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(Window.Window, &width, &height);

    extent = {(U32)width, (U32)height};

    extent.width = Clamp(extent.width,
                         swap_chain_support.Capabilities.minImageExtent.width,
                         swap_chain_support.Capabilities.maxImageExtent.width);

    extent.height = Clamp(
        extent.height, swap_chain_support.Capabilities.minImageExtent.height,
        swap_chain_support.Capabilities.maxImageExtent.height);
  }

  VkSwapchainCreateInfoKHR CreateInfo{};
  CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  CreateInfo.surface = Window.Surface;
  CreateInfo.minImageCount = imageCount;
  CreateInfo.imageFormat = surface_format.format;
  CreateInfo.imageColorSpace = surface_format.colorSpace;
  CreateInfo.imageExtent = extent;
  CreateInfo.imageArrayLayers = 1;
  CreateInfo.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  CreateInfo.preTransform = swap_chain_support.Capabilities.currentTransform;
  CreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  CreateInfo.presentMode = present_mode;
  CreateInfo.clipped = VK_TRUE;
  CreateInfo.oldSwapchain = VK_NULL_HANDLE;

  queue_family_indices indices = FindQueueFamilies(Device.PhysicalDevice);

  U32 qFamilyIndices[] = {indices.GraphicsAndCompute, indices.Presentation};

  if (indices.GraphicsAndCompute != indices.Presentation) {
    CreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    CreateInfo.queueFamilyIndexCount = 2;
    CreateInfo.pQueueFamilyIndices = qFamilyIndices;
  } else {
    CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  if (vkCreateSwapchainKHR(Device.LogicalDevice, &CreateInfo, nullptr,
                           &Swapchain.Swapchain) != VK_SUCCESS) {
    fprintf(stderr, "[ERROR] Could not create swap chain\n");
    exit(1);
  }

  vkGetSwapchainImagesKHR(Device.LogicalDevice, Swapchain.Swapchain,
                          &(Swapchain.N_Images), nullptr);

  if (Swapchain.Images == nullptr) {
    Swapchain.Images = RenderArena->Push<VkImage>(Swapchain.N_Images);
  }
  vkGetSwapchainImagesKHR(Device.LogicalDevice, Swapchain.Swapchain,
                          &Swapchain.N_Images, Swapchain.Images);

  Swapchain.Format = surface_format.format;
  Swapchain.Extent = extent;
  Swapchain.Capabilities = swap_chain_support.Capabilities;

  // draw image size will match the window
  VkExtent3D drawImageExtent = {(U32)Window.Width, (U32)Window.Height, 1};

  if (N_TextureImages == 0) {
    TextureImage = RenderArena->Push<vk_image>(1);
    N_TextureImages += 1;
  }
  vk_image *DrawImage = &TextureImage[0];

  DrawImage->Format = VK_FORMAT_R16G16B16A16_SFLOAT;
  DrawImage->Width = Window.Width;
  DrawImage->Height = Window.Height;

  VkImageUsageFlags drawImageUsages{};
  drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  VkImageCreateInfo ImgCreateInfo =
      ImageCreateInfo(DrawImage->Format, drawImageUsages, drawImageExtent);

  VmaAllocationCreateInfo VmaImgCreateInfo = {};
  VmaImgCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  VmaImgCreateInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vmaCreateImage(GPUAllocator, &ImgCreateInfo, &VmaImgCreateInfo,
                 &DrawImage->Image, &DrawImage->Alloc, nullptr);

  VkImageViewCreateInfo ImgVwCreateInfo = ImageViewCreateInfo(
      DrawImage->Format, DrawImage->Image, VK_IMAGE_ASPECT_COLOR_BIT);

  VK_CHECK(vkCreateImageView(Device.LogicalDevice, &ImgVwCreateInfo, nullptr,
                             &DrawImage->ImageView));

  MainDeletionQueue.PushBack([=]() {
    vkDestroyImageView(Device.LogicalDevice, DrawImage->ImageView, nullptr);
    vmaDestroyImage(GPUAllocator, DrawImage->Image, DrawImage->Alloc);
  });
}

// ------------------------------------------------------------------

void vulkan_iface::InitDescriptors() {
  // create a descriptor pool that will hold 10 sets with 1 image each
  vector<vk_descriptor_allocator::pool_size_ratio> sizes(TempArena, 1);
  sizes.PushBack({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1});

  GlobalDescriptorAllocator.InitPool(TempArena, Device.LogicalDevice, 10,
                                     sizes);

  // make the descriptor set layout for our compute draw
  {
    vk_descriptor_set builder;
    builder.Init(RenderArena, 1);
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    DrawImageDescriptorLayout =
        builder.Build(Device.LogicalDevice, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  DrawImageDescriptors = GlobalDescriptorAllocator.Allocate(
      Device.LogicalDevice, DrawImageDescriptorLayout);

  vk_image *DrawImage = &TextureImage[0];
  VkDescriptorImageInfo imgInfo{};
  imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  imgInfo.imageView = DrawImage->ImageView;

  VkWriteDescriptorSet drawImageWrite = {};
  drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  drawImageWrite.pNext = nullptr;

  drawImageWrite.dstBinding = 0;
  drawImageWrite.dstSet = DrawImageDescriptors;
  drawImageWrite.descriptorCount = 1;
  drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  drawImageWrite.pImageInfo = &imgInfo;

  vkUpdateDescriptorSets(Device.LogicalDevice, 1, &drawImageWrite, 0, nullptr);

  MainDeletionQueue.PushBack([&]() {
    GlobalDescriptorAllocator.DestroyPool(Device.LogicalDevice);
    vkDestroyDescriptorSetLayout(Device.LogicalDevice,
                                 DrawImageDescriptorLayout, nullptr);
  });
}

// ------------------------------------------------------------------

void vulkan_iface::InitBackgroundPipelines() {
  VkPipelineLayoutCreateInfo computeLayout{};
  computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  computeLayout.pNext = nullptr;
  computeLayout.pSetLayouts = &DrawImageDescriptorLayout;
  computeLayout.setLayoutCount = 1;

  VK_CHECK(vkCreatePipelineLayout(Device.LogicalDevice, &computeLayout, nullptr,
                                  &BackgroundComputePipelineLayout));

  // layout code
  VkShaderModule computeDrawShader;
  if (!LoadShaderModule("./samples/vulkan_sample/compute.comp.spv",
                        Device.LogicalDevice, &computeDrawShader)) {
    printf("[ERROR] Error when building the compute shader \n");
  }

  VkPipelineShaderStageCreateInfo stageinfo{};
  stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageinfo.pNext = nullptr;
  stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stageinfo.module = computeDrawShader;
  stageinfo.pName = "main";

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType =
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = BackgroundComputePipelineLayout;
  computePipelineCreateInfo.stage = stageinfo;

  VK_CHECK(vkCreateComputePipelines(Device.LogicalDevice, VK_NULL_HANDLE, 1,
                                    &computePipelineCreateInfo, nullptr,
                                    &BackgroundComputePipeline));

  vkDestroyShaderModule(Device.LogicalDevice, computeDrawShader, nullptr);

  MainDeletionQueue.PushBack([&]() {
    vkDestroyPipelineLayout(Device.LogicalDevice,
                            BackgroundComputePipelineLayout, nullptr);
    vkDestroyPipeline(Device.LogicalDevice, BackgroundComputePipeline, nullptr);
  });
}

// ------------------------------------------------------------------

void vulkan_iface::InitPipelines() { InitBackgroundPipelines(); }

// ------------------------------------------------------------------

void vulkan_iface::AddPipeline(vk_pipeline_builder &builder,
                               const char *vert_path, const char *frag_path) {
  VkShaderModule triangleFragShader;
  if (!LoadShaderModule("./samples/vulkan_sample/ColoredTriangle.frag.spv",
                        Device.LogicalDevice, &triangleFragShader)) {
    printf("[ERROR] when building the triangle fragment shader module\n");
  } else {
    printf("[INFO] Triangle fragment shader succesfully loaded\n");
  }

  VkShaderModule triangleVertexShader;
  if (!LoadShaderModule("./samples/vulkan_sample/ColoredTriangle.vert.spv",
                        Device.LogicalDevice, &triangleVertexShader)) {
    printf("[ERROR] when building the triangle vertex shader module\n");
  } else {
    printf("[INFO] Triangle vertex shader succesfully loaded\n");
  }

  VkPipelineLayout PipelineLayout;
  VkPipelineLayoutCreateInfo pipeline_layout_info = PipelineLayoutCreateInfo();
  VK_CHECK(vkCreatePipelineLayout(Device.LogicalDevice, &pipeline_layout_info,
                                  nullptr, &PipelineLayout));

  vk_image *DrawImage = &TextureImage[0];

  // use the triangle layout we created
  builder.PipelineLayout = PipelineLayout;
  // connecting the vertex and pixel shaders to the pipeline
  builder.SetShaders(triangleVertexShader, triangleFragShader);
  // connect the image format we will draw into, from draw image
  builder.SetColorAttachmentFormat(DrawImage->Format);

  VkPipeline B_Pipeline = builder.BuildPipeline(Device.LogicalDevice);

  Pipelines.PushBack({PipelineLayout, B_Pipeline});

  // clean structures
  vkDestroyShaderModule(Device.LogicalDevice, triangleFragShader, nullptr);
  vkDestroyShaderModule(Device.LogicalDevice, triangleVertexShader, nullptr);

  MainDeletionQueue.PushBack([&]() {
    vkDestroyPipelineLayout(Device.LogicalDevice, TrianglePipelineLayout,
                            nullptr);
    vkDestroyPipeline(Device.LogicalDevice, TrianglePipeline, nullptr);
  });
}

// ------------------------------------------------------------------

void vulkan_iface::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment = AttachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = RenderingInfo(Swapchain.Extent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

// ------------------------------------------------------------------

void vulkan_iface::InitImgui() {
  // 1: create descriptor pool for IMGUI
  //  the size of the pool is very oversize, but it's copied from imgui demo
  //  itself.
  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;

  VkDescriptorPool imguiPool;
  VK_CHECK(vkCreateDescriptorPool(Device.LogicalDevice, &pool_info, nullptr, &imguiPool));

  // 2: initialize imgui library

  // this initializes the core structures of imgui
  ImGui::CreateContext();

  // this initializes imgui for GLFW
  ImGui_ImplGlfw_InitForVulkan(Window.Window, true);
  // this initializes imgui for Vulkan
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = Instance;
  init_info.PhysicalDevice = Device.PhysicalDevice;
  init_info.Device = Device.LogicalDevice;
  init_info.Queue = Device.GraphicsQueue;
  init_info.DescriptorPool = imguiPool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.UseDynamicRendering = true;

  //dynamic rendering parameters for imgui to use
  init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &Swapchain.Format;


  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info);

  ImGui_ImplVulkan_CreateFontsTexture();

  // add the destroy the imgui created structures
  MainDeletionQueue.PushBack([=]() {
      ImGui_ImplVulkan_Shutdown();
      vkDestroyDescriptorPool(Device.LogicalDevice, imguiPool, nullptr);
  });
}

// ------------------------------------------------------------------

void vulkan_iface::InitTrianglePipeline() {
  VkShaderModule triangleFragShader;
  if (!LoadShaderModule("./samples/vulkan_sample/ColoredTriangle.frag.spv",
                        Device.LogicalDevice, &triangleFragShader)) {
    printf("[ERROR] when building the triangle fragment shader module\n");
  } else {
    printf("[INFO] Triangle fragment shader succesfully loaded\n");
  }

  VkShaderModule triangleVertexShader;
  if (!LoadShaderModule("./samples/vulkan_sample/ColoredTriangle.vert.spv",
                        Device.LogicalDevice, &triangleVertexShader)) {
    printf("[ERROR] when building the triangle vertex shader module\n");
  } else {
    printf("[INFO] Triangle vertex shader succesfully loaded\n");
  }

  // build the pipeline layout that controls the inputs/outputs of the shader
  // we are not using descriptor sets or other systems yet, so no need to use
  // anything other than empty default
  VkPipelineLayoutCreateInfo pipeline_layout_info = PipelineLayoutCreateInfo();
  VK_CHECK(vkCreatePipelineLayout(Device.LogicalDevice, &pipeline_layout_info,
                                  nullptr, &TrianglePipelineLayout));

  // Two shader stages, vertex and fragment shader
  //
  vk_pipeline_builder pipelineBuilder(TempArena, 2);

  // use the triangle layout we created
  pipelineBuilder.PipelineLayout = TrianglePipelineLayout;
  // connecting the vertex and pixel shaders to the pipeline
  pipelineBuilder.SetShaders(triangleVertexShader, triangleFragShader);
  // it will draw triangles
  pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  // filled triangles
  pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
  // no backface culling
  pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  // no multisampling
  pipelineBuilder.SetMultisamplingNone();
  // no blending
  pipelineBuilder.DisableBlending();
  // no depth testing
  pipelineBuilder.DisableDepthTest();

  vk_image *DrawImage = &TextureImage[0];
  // connect the image format we will draw into, from draw image
  pipelineBuilder.SetColorAttachmentFormat(DrawImage->Format);
  pipelineBuilder.SetDepthFormat(VK_FORMAT_UNDEFINED);

  // finally build the pipeline
  TrianglePipeline = pipelineBuilder.BuildPipeline(Device.LogicalDevice);

  // clean structures
  vkDestroyShaderModule(Device.LogicalDevice, triangleFragShader, nullptr);
  vkDestroyShaderModule(Device.LogicalDevice, triangleVertexShader, nullptr);

  MainDeletionQueue.PushBack([&]() {
    vkDestroyPipelineLayout(Device.LogicalDevice, TrianglePipelineLayout,
                            nullptr);
    vkDestroyPipeline(Device.LogicalDevice, TrianglePipeline, nullptr);
  });
}

// ------------------------------------------------------------------

vulkan_iface::vulkan_iface(const char *window_name = "Base") {

  // -------------- Arena creation --------------------------------
  //
  RenderArena = (Arena *)malloc(sizeof(Arena));
  RenderArena->Init(64 << 10, 1 << 30);

  TempArena = (Arena *)malloc(sizeof(Arena));
  TempArena->Init(64 << 10, 1 << 30);

  // MainDeletionQueue = RenderArena->Push<vector<std::function<void()>>>(2);
  //*MainDeletionQueue = vector<std::function<void()>>();
  printf("[INFO] Reserved render arena: %ld\n", RenderArena->GetReservedSize());
  printf("[INFO] Reserved temp arena  : %ld\n", TempArena->GetReservedSize());
  MainDeletionQueue.Init(RenderArena, (U32)256);
  printf("[INFO] Max Deletion Queue size: %d\n",
         MainDeletionQueue.GetCapacity());

  // -------------- Window creaation ------------------------------
  //
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  U32 width = 1920;
  U32 height = 1080;

  Window.Width = width;
  Window.Height = height;
  Window.Window =
      glfwCreateWindow(width, height, window_name, nullptr, nullptr);

  glfwSetWindowUserPointer(Window.Window, this);
  glfwSetFramebufferSizeCallback(Window.Window, FramebufferResizeCallback);
  glfwSetInputMode(Window.Window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);
  // glfwSetMouseButtonCallback( Window.Window, MouseButtonCallback );
  // glfwSetCharCallback( Window.Window, CharacterCallback);
  // glfwSetKeyCallback( Window.Window, KeyCallback);

  LastTime = glfwGetTime();

// ---------- Check validation layers ---------------------------
//
#ifndef NDEBUG
  if (!CheckValidationLayerSupport()) {
    fprintf(stderr, "[ERROR] Validation layers could not be found!!\n");
    exit(1);
  }
#endif

  // ---------------------- Vulkan Instance -----------------------
  //
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = window_name;
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 0);
  appInfo.apiVersion = VK_API_VERSION_1_3;

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  U32 ExtensionCount;
  char **extensions = GetRequiredExtensions(&ExtensionCount);
  createInfo.enabledExtensionCount = static_cast<uint32_t>(ExtensionCount);
  createInfo.ppEnabledExtensionNames = (const char **)extensions;

  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

  createInfo.enabledLayerCount = 1;
  createInfo.ppEnabledLayerNames = VALIDATION_LAYERS;

// Set for the debug info the validation layers we want to use
#ifndef NDEBUG
  PopulateDebugMessengerCreateInfo(debugCreateInfo);
  createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
#else
  createInfo.enabledLayerCount = 0;
  createInfo.pNext = nullptr;
#endif

  // CREATE INSTANCE
  //
  if (vkCreateInstance(&createInfo, nullptr, &Instance) != VK_SUCCESS) {
    fprintf(stderr, "[ERROR] Failed to create instance\n");
    exit(1);
  }

  // SETUP WINDOW SURFACE
  //
  if (glfwCreateWindowSurface(Instance, Window.Window, nullptr,
                              &Window.Surface) != VK_SUCCESS) {
    fprintf(stderr, "[ERROR] Could not create window surface\n");
    exit(1);
  }

#ifndef NDEBUG

  // SETUP DEBUG MESSENGER
  //
  VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
  PopulateDebugMessengerCreateInfo(debug_create_info);

  if (CreateDebugUtilsMessengerEXT(Instance, &debug_create_info, nullptr,
                                   &DebugMessenger) != VK_SUCCESS) {
    fprintf(stderr, "[ERROR] Failed to setup debug messenger\n");
    exit(1);
  }

#endif

  // ---------------------- SETUP DEVICE --------------------------
  //
  queue_family_indices qfi;
  U32 DeviceCount = 0;
  vkEnumeratePhysicalDevices(Instance, &DeviceCount, nullptr);
  if (DeviceCount == 0) {
    fprintf(stderr, "[ERROR] Could not find any GPU with vulkan support\n");
    exit(1);
  }
  VkPhysicalDevice *Devices = TempArena->Push<VkPhysicalDevice>(DeviceCount);
  vkEnumeratePhysicalDevices(Instance, &DeviceCount, Devices);

  for (U32 i = 0; i < DeviceCount; ++i) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(Devices[i], &properties);
    printf("Device Name: %s\n", properties.deviceName);

    if (IsSuitableDevice(Devices[i])) {
      Device.PhysicalDevice = Devices[i];
      break;
    }
  }

  if (Device.PhysicalDevice == VK_NULL_HANDLE) {
    fprintf(stderr, "[ERROR] Could not find a suitable GPU!\n");
    exit(1);
  }

  qfi = FindQueueFamilies(Device.PhysicalDevice);
  Device.FamilyIndices = qfi;

  F32 queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_create_info{};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = qfi.GraphicsAndCompute;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;

  // Vulkan 1.3 features
  VkPhysicalDeviceVulkan13Features features13 = {};
  features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features13.dynamicRendering = VK_TRUE;
  features13.synchronization2 = VK_TRUE;
  features13.pNext = nullptr;

  // Vulkan 1.2 features
  VkPhysicalDeviceVulkan12Features features12 = {};
  features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  features12.bufferDeviceAddress = VK_TRUE;
  features12.descriptorIndexing = VK_TRUE;
  features12.pNext = &features13;

  // Core Vulkan features
  VkPhysicalDeviceFeatures2 deviceFeatures = {};
  deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  deviceFeatures.features.samplerAnisotropy = VK_TRUE;
  deviceFeatures.features.fillModeNonSolid = VK_TRUE;
  deviceFeatures.pNext = &features12;

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pQueueCreateInfos = &queue_create_info;
  create_info.queueCreateInfoCount = 1;
  create_info.pEnabledFeatures = nullptr;
  create_info.enabledExtensionCount = 4;
  create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS;
  create_info.pNext = &deviceFeatures;

#ifndef NDEBUG
  create_info.enabledLayerCount = 1;
  create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
#else
  create_info.enabledLayerCount = 0;
#endif

  if (vkCreateDevice(Device.PhysicalDevice, &create_info, nullptr,
                     &Device.LogicalDevice) != VK_SUCCESS) {
    fprintf(stderr, "[ERROR] Could not create logical device\n");
    exit(1);
  }

  vkGetDeviceQueue(Device.LogicalDevice, qfi.GraphicsAndCompute, 0,
                   &Device.GraphicsQueue);
  vkGetDeviceQueue(Device.LogicalDevice, qfi.Presentation, 0,
                   &Device.PresentationQueue);
  vkGetDeviceQueue(Device.LogicalDevice, qfi.GraphicsAndCompute, 0,
                   &Device.ComputeQueue);

  // initialize the memory allocator
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = Device.PhysicalDevice;
  allocatorInfo.device = Device.LogicalDevice;
  allocatorInfo.instance = Instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  VkResult result = vmaCreateAllocator(&allocatorInfo, &GPUAllocator);
  if (result != VK_SUCCESS) {
    fprintf(stderr, "[ERROR] Failed to create VMA allocator\n");
    exit(1);
  }

  MainDeletionQueue.PushBack([&]() { vmaDestroyAllocator(GPUAllocator); });

  CreateSwapchain();

  CreateImageViews();

  InitCommands();

  InitSyncStructures();

  InitDescriptors();

  // Support to 16 Pipelines as for now
  //
  Pipelines.Init(RenderArena, 16);

  InitPipelines();

  InitTrianglePipeline();

  InitImgui();
}

// -------------------- PRIVATE CLASS FUNCTIONS ---------------------
//

vk_buffer vulkan_iface::AllocateBuffer(size_t AllocSize,
                                       VkBufferUsageFlags Usage,
                                       VmaMemoryUsage MemoryUsage) {
  // allocate buffer
  VkBufferCreateInfo bufferInfo = {.sType =
                                       VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.pNext = nullptr;
  bufferInfo.size = AllocSize;

  bufferInfo.usage = Usage;

  VmaAllocationCreateInfo vmaallocInfo = {};
  vmaallocInfo.usage = MemoryUsage;
  vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  vk_buffer newBuffer;

  // allocate the buffer
  VK_CHECK(vmaCreateBuffer(GPUAllocator, &bufferInfo, &vmaallocInfo,
                           &newBuffer.Buffer, &newBuffer.Allocation,
                           &newBuffer.AllocationInfo));

  return newBuffer;
}

// ------------------------------------------------------------------

void vulkan_iface::DestroyBuffer(const vk_buffer &buffer) {
  vmaDestroyBuffer(GPUAllocator, buffer.Buffer, buffer.Allocation);
}

// ------------------------------------------------------------------

VkRenderingAttachmentInfo vulkan_iface::AttachmentInfo(VkImageView View,
                                                       VkClearValue *Clear,
                                                       VkImageLayout Layout) {
  VkRenderingAttachmentInfo colorAttachment{};
  colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  colorAttachment.pNext = nullptr;

  colorAttachment.imageView = View;
  colorAttachment.imageLayout = Layout;
  colorAttachment.loadOp =
      Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  if (Clear) {
    colorAttachment.clearValue = *Clear;
  }

  return colorAttachment;
}

// ------------------------------------------------------------------

VkRenderingInfo
vulkan_iface::RenderingInfo(VkExtent2D Extent,
                            VkRenderingAttachmentInfo *ColorInfo,
                            VkRenderingAttachmentInfo *DepthInfo) {
  VkRenderingInfo renderInfo{};
  renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderInfo.pNext = nullptr;

  renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, Extent};
  renderInfo.layerCount = 1;
  renderInfo.colorAttachmentCount = 1;
  renderInfo.pColorAttachments = ColorInfo;
  renderInfo.pDepthAttachment = DepthInfo;
  renderInfo.pStencilAttachment = nullptr;

  return renderInfo;
}

// ------------------------------------------------------------------

VkPipelineLayoutCreateInfo vulkan_iface::PipelineLayoutCreateInfo() {
  VkPipelineLayoutCreateInfo Layout{};
  Layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  Layout.pNext = nullptr;
  Layout.pSetLayouts = &DrawImageDescriptorLayout;
  Layout.setLayoutCount = 1;

  return Layout;
}

// ------------------------------------------------------------------

bool vulkan_iface::LoadShaderModule(const char *FilePath, VkDevice Device,
                                    VkShaderModule *OutShaderModule) {
  // open the file. With cursor at the end
  std::ifstream file(FilePath, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    return false;
  }

  // find what the size of the file is by looking up the location of the cursor
  // because the cursor is at the end, it gives the size directly in bytes
  size_t fileSize = (size_t)file.tellg();

  // spirv expects the buffer to be on U32, so make sure to reserve a int
  // vector big enough for the entire file
  vector<U32> buffer(TempArena, fileSize / sizeof(U32));
  // std::vector<U32> buffer(fileSize / sizeof(U32));

  // put file cursor at beginning
  file.seekg(0);

  // load the entire file into the buffer
  file.read((char *)buffer.GetData(), fileSize);
  buffer.SetLength(fileSize / sizeof(U32));

  // now that the file is loaded into the buffer, we can close it
  file.close();

  // create a new shader module, using the buffer we loaded
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.pNext = nullptr;

  // codeSize has to be in bytes, so multply the ints in the buffer by size of
  // int to know the real size of the buffer
  createInfo.codeSize = buffer.GetLength() * sizeof(uint32_t);
  createInfo.pCode = buffer.GetData();

  // check that the creation goes well.
  VkShaderModule shaderModule;
  if (vkCreateShaderModule(Device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    return false;
  }
  *OutShaderModule = shaderModule;
  return true;
}

// ------------------------------------------------------------------

VkImageCreateInfo vulkan_iface::ImageCreateInfo(VkFormat format,
                                                VkImageUsageFlags usageFlags,
                                                VkExtent3D extent) {
  VkImageCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.pNext = nullptr;

  info.imageType = VK_IMAGE_TYPE_2D;

  info.format = format;
  info.extent = extent;

  info.mipLevels = 1;
  info.arrayLayers = 1;

  // for MSAA. we will not be using it by default, so default it to 1 sample per
  // pixel.
  info.samples = VK_SAMPLE_COUNT_1_BIT;

  // optimal tiling, which means the image is stored on the best gpu format
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = usageFlags;

  return info;
}

// ------------------------------------------------------------------

void vulkan_iface::DrawBackground(VkCommandBuffer cmd) {
  U32 FrameIdx = CurrentFrame % MAX_FRAMES_IN_FLIGHT;

  vk_image *DrawImage = &TextureImage[0];
  // make a clear-color from frame number. This will flash with a 120 frame
  // period.
  VkClearColorValue clearValue;
  float flash = abs(sin(FrameIdx / 120.f));
  clearValue = {{0.0f, 0.0f, flash, 1.0f}};

  VkImageSubresourceRange clearRange{};
  clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  clearRange.baseMipLevel = 0;
  clearRange.levelCount = VK_REMAINING_MIP_LEVELS;
  clearRange.baseArrayLayer = 0;
  clearRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

  VkExtent2D Extent = {DrawImage->Width, DrawImage->Height};

  // clear image
  // vkCmdClearColorImage(cmd, DrawImage->Image, VK_IMAGE_LAYOUT_GENERAL,
  // &clearValue, 1, &clearRange);

  // bind the gradient drawing compute pipeline
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                    BackgroundComputePipeline);

  // bind the descriptor set containing the draw image for the compute pipeline
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          BackgroundComputePipelineLayout, 0, 1,
                          &DrawImageDescriptors, 0, nullptr);

  // execute the compute pipeline dispatch. We are using 16x16 workgroup size so
  // we need to divide by it
  vkCmdDispatch(cmd, ceil(Extent.width / 16.0), ceil(Extent.height / 16.0), 1);
}

// ------------------------------------------------------------------

void vulkan_iface::CopyImageToImage(VkCommandBuffer cmd, VkImage source,
                                    VkImage destination, VkExtent2D srcSize,
                                    VkExtent2D dstSize) {
  VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                          .pNext = nullptr};

  blitRegion.srcOffsets[1].x = srcSize.width;
  blitRegion.srcOffsets[1].y = srcSize.height;
  blitRegion.srcOffsets[1].z = 1;

  blitRegion.dstOffsets[1].x = dstSize.width;
  blitRegion.dstOffsets[1].y = dstSize.height;
  blitRegion.dstOffsets[1].z = 1;

  blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blitRegion.srcSubresource.baseArrayLayer = 0;
  blitRegion.srcSubresource.layerCount = 1;
  blitRegion.srcSubresource.mipLevel = 0;

  blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blitRegion.dstSubresource.baseArrayLayer = 0;
  blitRegion.dstSubresource.layerCount = 1;
  blitRegion.dstSubresource.mipLevel = 0;

  VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                            .pNext = nullptr};
  blitInfo.dstImage = destination;
  blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  blitInfo.srcImage = source;
  blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  blitInfo.filter = VK_FILTER_LINEAR;
  blitInfo.regionCount = 1;
  blitInfo.pRegions = &blitRegion;

  vkCmdBlitImage2(cmd, &blitInfo);
}

// ------------------------------------------------------------------

VkImageViewCreateInfo
vulkan_iface::ImageViewCreateInfo(VkFormat format, VkImage image,
                                  VkImageAspectFlags aspectFlags) {
  // build a image-view for the depth image to use for rendering
  VkImageViewCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.pNext = nullptr;

  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.image = image;
  info.format = format;
  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount = 1;
  info.subresourceRange.aspectMask = aspectFlags;

  return info;
}

// ------------------------------------------------------------------

swapchain_support_details
vulkan_iface::QuerySwapChainSupport(VkPhysicalDevice Device) {
  swapchain_support_details details;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device, Window.Surface,
                                            &details.Capabilities);

  vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Window.Surface,
                                       &details.FormatCount, nullptr);

  if (details.FormatCount != 0) {
    details.Formats = TempArena->Push<VkSurfaceFormatKHR>(details.FormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Window.Surface,
                                         &details.FormatCount, details.Formats);
  }

  vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Window.Surface,
                                            &details.PresentModeCount, nullptr);

  if (details.PresentModeCount != 0) {
    details.PresentModes =
        TempArena->Push<VkPresentModeKHR>(details.PresentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Window.Surface,
                                              &details.PresentModeCount,
                                              details.PresentModes);
  }

  return details;
}

// ------------------------------------------------------------------

bool vulkan_iface::CheckDeviceExtensionSupport(VkPhysicalDevice Device) {
  U32 extensionCount;
  vkEnumerateDeviceExtensionProperties(Device, nullptr, &extensionCount,
                                       nullptr);

  VkExtensionProperties *available_extensions =
      TempArena->Push<VkExtensionProperties>(extensionCount);
  // std::vector<VkExtensionProperties> available_extensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(Device, nullptr, &extensionCount,
                                       available_extensions);

  U32 TotalExtensions = extensionCount;
  for (U32 extIdx = 0; extIdx < 4; extIdx += 1) {
    bool extFound = false;
    for (U32 i = 0; i < extensionCount; ++i) {
      if (strcmp(available_extensions[i].extensionName,
                 DEVICE_EXTENSIONS[extIdx]) == 0) {
        printf("[INFO] Extension %s found\n", DEVICE_EXTENSIONS[extIdx]);
        extFound = true;
      }
    }
    if (!extFound) {
      printf("[ERROR] EXTENSION: %s | NOT_FOUND", DEVICE_EXTENSIONS[extIdx]);
      TotalExtensions -= 1;
    }
  }

  if (TotalExtensions == extensionCount) {
    return true;
  } else {
    return false;
  }
}

// ------------------------------------------------------------------

queue_family_indices vulkan_iface::FindQueueFamilies(VkPhysicalDevice &device) {
  queue_family_indices indices;
  indices.GraphicsAndCompute = (1 << 30) - 1;
  indices.Presentation = (1 << 30) - 1;

  U32 queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           nullptr);

  VkQueueFamilyProperties *queue_families =
      TempArena->Push<VkQueueFamilyProperties>(queue_family_count);
  // std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           queue_families);

  size_t i;
  for (i = 0; i < queue_family_count; ++i) {
    if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
        (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
      indices.GraphicsAndCompute = i;
    }
    VkBool32 present_support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, Window.Surface,
                                         &present_support);

    if (present_support) {
      indices.Presentation = i;
    }

    if (indices.GraphicsAndCompute != ((1 << 30) - 1) &&
        indices.Presentation != ((1 << 30) - 1)) {
      break;
    }
    i++;
  }

  return indices;
}

// ------------------------------------------------------------------

bool vulkan_iface::IsSuitableDevice(VkPhysicalDevice &device) {
  queue_family_indices indices = FindQueueFamilies(device);

  bool extensions_supported = CheckDeviceExtensionSupport(device);

  bool swap_chain_adequate = false;

  if (extensions_supported) {
    swapchain_support_details swap_chain_support =
        QuerySwapChainSupport(device);
    swap_chain_adequate =
        swap_chain_support.Formats != 0 && swap_chain_support.PresentModes != 0;
  }

  return indices.GraphicsAndCompute != U32_MAX &&
         indices.Presentation != U32_MAX && extensions_supported &&
         swap_chain_adequate;
}

// ------------------------------------------------------------------

bool vulkan_iface::CheckValidationLayerSupport() {
  uint32_t LayerCount;
  vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);

  VkLayerProperties *availableLayers =
      RenderArena->Push<VkLayerProperties>(LayerCount);
  vkEnumerateInstanceLayerProperties(&LayerCount, availableLayers);

  for (uint32_t i = 0; i < LayerCount; ++i) {
    if (strcmp(availableLayers[i].layerName, VALIDATION_LAYERS[0]) == 0) {
      RenderArena->Pop<VkLayerProperties>(LayerCount);
      return true;
    }
  }

  RenderArena->Pop<VkLayerProperties>(LayerCount);
  return false;
}

// ------------------------------------------------------------------

char **vulkan_iface::GetRequiredExtensions(uint32_t *ExtensionCount) {
  U32 glfwExtensionCount = 0;
  const char **glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

#ifndef NDEBUG
  // Add the extension that goes for debugging
  //
  U64 tmp = glfwExtensionCount + 1;
  char **extensions = RenderArena->Push<char *>(tmp);
#else
  char **extensions = RenderArena->Push<char *>(glfwExtensionCount);
#endif

  U64 i;
  for (i = 0; i < glfwExtensionCount; ++i) {
    extensions[i] = RenderArena->Push<char>(strlen(glfwExtensions[i]) + 1);
    strcpy(extensions[i], glfwExtensions[i]);
  }

#ifndef NDEBUG
  extensions[glfwExtensionCount] =
      RenderArena->Push<char>(strlen(VK_EXT_DEBUG_UTILS_EXTENSION_NAME));
  strcpy(extensions[glfwExtensionCount], VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  ++glfwExtensionCount;
#endif

  *ExtensionCount = glfwExtensionCount;

  return extensions;
}

// ------------------------------------------------------------------

VkCommandBufferSubmitInfo
vulkan_iface::CommandBufferSubmitInfo(VkCommandBuffer cmd) {
  VkCommandBufferSubmitInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  info.pNext = nullptr;
  info.commandBuffer = cmd;
  info.deviceMask = 0;

  return info;
}

// ------------------------------------------------------------------

VkSemaphoreSubmitInfo
vulkan_iface::SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask,
                                  VkSemaphore semaphore) {
  VkSemaphoreSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  submitInfo.pNext = nullptr;
  submitInfo.semaphore = semaphore;
  submitInfo.stageMask = stageMask;
  submitInfo.deviceIndex = 0;
  submitInfo.value = 1;

  return submitInfo;
}

// ------------------------------------------------------------------

VkSubmitInfo2
vulkan_iface::SubmitInfo(VkCommandBufferSubmitInfo *cmd,
                         VkSemaphoreSubmitInfo *signalSemaphoreInfo,
                         VkSemaphoreSubmitInfo *waitSemaphoreInfo) {
  VkSubmitInfo2 info = {};
  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  info.pNext = nullptr;

  info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
  info.pWaitSemaphoreInfos = waitSemaphoreInfo;

  info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
  info.pSignalSemaphoreInfos = signalSemaphoreInfo;

  info.commandBufferInfoCount = 1;
  info.pCommandBufferInfos = cmd;

  return info;
}

// =============== vk_pipeline_builder CLASS ========================

void vk_pipeline_builder::SetShaders(VkShaderModule vertexShader,
                                     VkShaderModule fragmentShader) {
  ShaderStages.Clear();

  ShaderStages.PushBack(vk_pipeline_builder::PipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_VERTEX_BIT, vertexShader, "main"));

  ShaderStages.PushBack(vk_pipeline_builder::PipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader, "main"));
}

// ------------------------------------------------------------------

void vk_pipeline_builder::SetInputTopology(VkPrimitiveTopology topology) {
  InputAssembly.topology = topology;
  // we are not going to use primitive restart on the entire tutorial so leave
  // it on false
  InputAssembly.primitiveRestartEnable = VK_FALSE;
}
// ------------------------------------------------------------------

void vk_pipeline_builder::SetMultisamplingNone() {
  Multisampling.sampleShadingEnable = VK_FALSE;
  // multisampling defaulted to no multisampling (1 sample per pixel)
  Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  Multisampling.minSampleShading = 1.0f;
  Multisampling.pSampleMask = nullptr;
  // no alpha to coverage either
  Multisampling.alphaToCoverageEnable = VK_FALSE;
  Multisampling.alphaToOneEnable = VK_FALSE;
}

// ------------------------------------------------------------------

void vk_pipeline_builder::DisableDepthTest() {
  DepthStencil.depthTestEnable = VK_FALSE;
  DepthStencil.depthWriteEnable = VK_FALSE;
  DepthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
  DepthStencil.depthBoundsTestEnable = VK_FALSE;
  DepthStencil.stencilTestEnable = VK_FALSE;
  DepthStencil.front = {};
  DepthStencil.back = {};
  DepthStencil.minDepthBounds = 0.f;
  DepthStencil.maxDepthBounds = 1.f;
}

// ------------------------------------------------------------------

VkPipeline vk_pipeline_builder::BuildPipeline(VkDevice Device) {
  // make viewport state from our stored viewport and scissor.
  // at the moment we wont support multiple viewports or scissors
  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.pNext = nullptr;

  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  // setup dummy color blending. We arent using transparent objects yet
  // the blending is just "no blend", but we do write to the color attachment
  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.pNext = nullptr;

  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &ColorBlendAttachment;

  // completely clear VertexInputStateCreateInfo, as we have no need for it
  VkPipelineVertexInputStateCreateInfo _vertexInputInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  // build the actual pipeline
  // we now use all of the info structs we have been writing into into this one
  // to create the pipeline
  VkGraphicsPipelineCreateInfo pipelineInfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  // connect the renderInfo to the pNext extension mechanism
  pipelineInfo.pNext = &RenderInfo;

  pipelineInfo.stageCount = ShaderStages.GetLength();
  pipelineInfo.pStages = ShaderStages.GetData();
  pipelineInfo.pVertexInputState = &_vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &InputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &Rasterizer;
  pipelineInfo.pMultisampleState = &Multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDepthStencilState = &DepthStencil;
  pipelineInfo.layout = PipelineLayout;

  VkDynamicState state[] = {VK_DYNAMIC_STATE_VIEWPORT,
                            VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamicInfo.pDynamicStates = &state[0];
  dynamicInfo.dynamicStateCount = 2;

  pipelineInfo.pDynamicState = &dynamicInfo;
  // its easy to error out on create graphics pipeline, so we handle it a bit
  // better than the common VK_CHECK case
  VkPipeline newPipeline;
  if (vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &newPipeline) != VK_SUCCESS) {
    printf("[ERROR] Failed to create pipeline");
    return VK_NULL_HANDLE;
  } else {
    return newPipeline;
  }
}
