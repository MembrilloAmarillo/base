#include "vulkan_impl.h"

// ----------------------------- DEBUG ------------------------------ 
//
global VKAPI_ATTR VkBool32 VKAPI_CALL 
DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData
) 
{
	fprintf( stderr, "validation layer: %s\n", pCallbackData->pMessage );
 
	return VK_FALSE;
}

// ------------------------------------------------------------------
//
global void
PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
}

// ------------------------------------------------------------------
//
global void
DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) 
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

// ------------------------------------------------------------------
//
global VkResult
CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) 
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

// ------------------------------------------------------------------


global void
FramebufferResizeCallback( GLFWwindow* window, int width, int height ) 
{
	vulkan_iface* app = reinterpret_cast<vulkan_iface*>(glfwGetWindowUserPointer(window));
	app->FramebufferResized = true;
}

// -------------------------- CLASS FUNCTIONS -----------------------
//

VkSemaphoreCreateInfo vulkan_iface::SemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0 )
{
	VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

// ------------------------------------------------------------------

VkFenceCreateInfo vulkan_iface::FenceCreateInfo(VkFenceCreateFlags flags = 0 )
{
	VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = nullptr;

    info.flags = flags;

    return info;
}

// ------------------------------------------------------------------

void vulkan_iface::InitSyncStructures()
{
	//create syncronization structures
	//one fence to control when the gpu has finished rendering the frame,
	//and 2 semaphores to syncronize rendering with swapchain
	//we want the fence to start signalled so we can wait on it on the first frame
	VkFenceCreateInfo fenceCreateInfo = FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = SemaphoreCreateInfo();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VK_CHECK(vkCreateFence(Device.LogicalDevice, &fenceCreateInfo, nullptr, &Semaphores.InFlight[i]));

		VK_CHECK(vkCreateSemaphore(Device.LogicalDevice, &semaphoreCreateInfo, nullptr, &Semaphores.ImageAvailable[i]));
		VK_CHECK(vkCreateSemaphore(Device.LogicalDevice, &semaphoreCreateInfo, nullptr, &Semaphores.RenderFinished[i]));
	}
}

// ------------------------------------------------------------------

void vulkan_iface::TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier2 imageBarrier {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    imageBarrier.pNext = nullptr;

    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageSubresourceRange subImage {};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

	imageBarrier.subresourceRange = subImage;
    imageBarrier.image = image;

    VkDependencyInfo depInfo {};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

// ------------------------------------------------------------------

void vulkan_iface::BeginDrawing()
{
	u32 FrameIdx = CurrentFrame % MAX_FRAMES_IN_FLIGHT;
	// wait until the gpu has finished rendering the last frame. Timeout of 1
	// second
	VK_CHECK(vkWaitForFences(Device.LogicalDevice, 1, &Semaphores.InFlight[FrameIdx], true, 1000000000));
	VK_CHECK(vkResetFences(Device.LogicalDevice, 1, &Semaphores.InFlight[FrameIdx]));

	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(Device.LogicalDevice, Swapchain.Swapchain, 1000000000, Semaphores.ImageAvailable[FrameIdx], nullptr, &swapchainImageIndex));

	VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.pNext = nullptr;

    cmdBeginInfo.pInheritanceInfo = nullptr;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	//naming it cmd for shorter writing
	VkCommandBuffer cmd = CommandBuffers[FrameIdx];

	// now that we are sure that the commands finished executing, we can safely
	// reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	//start the command buffer recording
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	
	//make the swapchain image into writeable mode before rendering
	TransitionImage(cmd, Swapchain.Images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	//make a clear-color from frame number. This will flash with a 120 frame period.
	VkClearColorValue clearValue;
	float flash = abs(sin(FrameIdx / 120.f));
	clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

	VkImageSubresourceRange clearRange {};
    clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearRange.baseMipLevel = 0;
    clearRange.levelCount = VK_REMAINING_MIP_LEVELS;
    clearRange.baseArrayLayer = 0;
    clearRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	//clear image
	vkCmdClearColorImage(cmd, Swapchain.Images[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

	//make the swapchain image into presentable mode
	TransitionImage(cmd, Swapchain.Images[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdinfo = CommandBufferSubmitInfo(cmd);	
	
	VkSemaphoreSubmitInfo waitInfo = SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, Semaphores.ImageAvailable[FrameIdx]);
	VkSemaphoreSubmitInfo signalInfo = SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, Semaphores.RenderFinished[FrameIdx]);	
	
	VkSubmitInfo2 submit = SubmitInfo(&cmdinfo, &signalInfo, &waitInfo);	

	VK_CHECK(vkQueueSubmit2(Device.GraphicsQueue, 1, &submit, Semaphores.InFlight[FrameIdx]));

	//prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &Swapchain.Swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &Semaphores.RenderFinished[FrameIdx];
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(Device.GraphicsQueue, &presentInfo));

	//increase the number of frames drawn
	CurrentFrame++;
}

// ------------------------------------------------------------------

VkCommandPoolCreateInfo 
vulkan_iface::CommandPoolCreateInfo(u32 queueFamilyIndex, VkCommandPoolCreateFlags flags /*= 0*/)
{
    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = nullptr;
    info.queueFamilyIndex = queueFamilyIndex;
    info.flags = flags;
    return info;
}

// ------------------------------------------------------------------

VkCommandBufferAllocateInfo
vulkan_iface::CommandBufferAllocateInfo(VkCommandPool pool, uint32_t count /*= 1*/)
{
    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = nullptr;

    info.commandPool = pool;
    info.commandBufferCount = count;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    return info;
}

// ------------------------------------------------------------------ 

void vulkan_iface::InitCommands()
{
	VkCommandPoolCreateInfo commandPoolInfo = CommandPoolCreateInfo(Device.FamilyIndices.GraphicsAndCompute, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VK_CHECK(vkCreateCommandPool(Device.LogicalDevice, &commandPoolInfo, nullptr, &CommandPool[i]));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = CommandBufferAllocateInfo(CommandPool[i], 1);

		VK_CHECK(vkAllocateCommandBuffers(Device.LogicalDevice, &cmdAllocInfo, &CommandBuffers[i]));
	}
}

// ------------------------------------------------------------------ 

void vulkan_iface::CreateImageViews()
{
	Swapchain.N_ImageViews = Swapchain.N_Images;
	if( Swapchain.ImageViews == nullptr )
	{
		Swapchain.ImageViews  = RenderArena->Push<VkImageView>(Swapchain.N_ImageViews);
	}
	for( u32 i = 0; i < Swapchain.N_ImageViews; ++i )
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image                           = Swapchain.Images[i];;
		createInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format                          = Swapchain.Format;
		createInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel   = 0;
		createInfo.subresourceRange.levelCount     = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount     = 1;
  
		if (vkCreateImageView( Device.LogicalDevice, &createInfo, nullptr, &Swapchain.ImageViews[i]) != VK_SUCCESS)
		{
			fprintf( stderr, "[ERROR] Could not create image view\n" );
			exit( 1 );
		}
	}
}

// ------------------------------------------------------------------ 

void vulkan_iface::CreateSwapchain()
{
	u32 U32_MAX = (1 << 30) - 1;
	swapchain_support_details swap_chain_support = QuerySwapChainSupport( Device.PhysicalDevice );
 
	u32 imageCount = swap_chain_support.Capabilities.minImageCount + 1;
 
	VkSurfaceFormatKHR surface_format;
	VkPresentModeKHR present_mode;
	VkExtent2D extent;
 
	if (swap_chain_support.Capabilities.maxImageCount > 0 &&
		imageCount > swap_chain_support.Capabilities.maxImageCount)
	{
		imageCount = swap_chain_support.Capabilities.maxImageCount;
	}
 
	for( u32 i = 0; i < swap_chain_support.FormatCount; ++i )
	{
		if (swap_chain_support.Formats[i].format     == VK_FORMAT_R8G8B8A8_SRGB &&
			swap_chain_support.Formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR )
		{
			surface_format = swap_chain_support.Formats[i];
			break;
		}
	}
 
	bool found_mailbox = false;

	#ifndef ENABLE_VSYNC
 
	for( u32 i = 0; i < swap_chain_support.PresentModeCount; ++i )
	{
		if (swap_chain_support.PresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			found_mailbox = true;
			break;
		}
	}
 
	#endif
 
	if (!found_mailbox) { present_mode = VK_PRESENT_MODE_FIFO_KHR; }
 
	if( swap_chain_support.Capabilities.currentExtent.width != U32_MAX )
	{
		extent = swap_chain_support.Capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize( Window.Window, &width, &height );
  
		extent = {(u32)width, (u32)height};
  
		extent.width = Clamp(
			extent.width, 
			swap_chain_support.Capabilities.minImageExtent.width,
			swap_chain_support.Capabilities.maxImageExtent.width
		);

		extent.height = Clamp(
			extent.height, 
			swap_chain_support.Capabilities.minImageExtent.height,
			swap_chain_support.Capabilities.maxImageExtent.height
		);
	}
 
	VkSwapchainCreateInfoKHR CreateInfo{};
	CreateInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	CreateInfo.surface          = Window.Surface;
	CreateInfo.minImageCount    = imageCount;
	CreateInfo.imageFormat      = surface_format.format;
	CreateInfo.imageColorSpace  = surface_format.colorSpace;
	CreateInfo.imageExtent      = extent;
	CreateInfo.imageArrayLayers = 1;
	CreateInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	CreateInfo.preTransform     = swap_chain_support.Capabilities.currentTransform;
	CreateInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	CreateInfo.presentMode      = present_mode;
	CreateInfo.clipped          = VK_TRUE;
	CreateInfo.oldSwapchain     = VK_NULL_HANDLE;
  
	queue_family_indices indices = FindQueueFamilies( Device.PhysicalDevice );
 
	u32 qFamilyIndices[] = { indices.GraphicsAndCompute, indices.Presentation };
 
	if( indices.GraphicsAndCompute != indices.Presentation )
	{
		CreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		CreateInfo.queueFamilyIndexCount = 2;
		CreateInfo.pQueueFamilyIndices = qFamilyIndices;
	}
	else
	{
		CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}
 
	if( vkCreateSwapchainKHR( Device.LogicalDevice, &CreateInfo, nullptr, &Swapchain.Swapchain ) != VK_SUCCESS )
	{
		fprintf( stderr, "[ERROR] Could not create swap chain\n" );
		exit( 1 );
	}
 
	vkGetSwapchainImagesKHR(
		Device.LogicalDevice,
		Swapchain.Swapchain,
		&(Swapchain.N_Images),
		nullptr
	);
 
	if( Swapchain.Images == nullptr )
	{
		Swapchain.Images = RenderArena->Push<VkImage>(Swapchain.N_Images);
	}
	vkGetSwapchainImagesKHR(
		Device.LogicalDevice,
		Swapchain.Swapchain,
		&Swapchain.N_Images,
		Swapchain.Images
	);
 
	Swapchain.Format = surface_format.format;
	Swapchain.Extent = extent;
	Swapchain.Capabilities = swap_chain_support.Capabilities;
}

// ------------------------------------------------------------------ 

vulkan_iface::vulkan_iface( const char* window_name = "Base" ) {

	// -------------- Arena creation --------------------------------
	//
	RenderArena = (Arena*)malloc(sizeof(Arena));
	RenderArena->Init(64 << 10, 1 << 30);

	TempArena = (Arena*)malloc(sizeof(Arena));
	TempArena->Init(64 << 10, 256 << 20);

	// -------------- Window creaation ------------------------------
	//
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	u32 width = 1920;
	u32 height = 1080;

	Window.Width = width;
	Window.Height = height;
	Window.Window = glfwCreateWindow(width, height, window_name, nullptr, nullptr);

	glfwSetWindowUserPointer( Window.Window, this );
	glfwSetFramebufferSizeCallback( Window.Window, FramebufferResizeCallback );
	glfwSetInputMode( Window.Window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);
	//glfwSetMouseButtonCallback( Window.Window, MouseButtonCallback );
	//glfwSetCharCallback( Window.Window, CharacterCallback);
	//glfwSetKeyCallback( Window.Window, KeyCallback);

	LastTime = glfwGetTime();

	// ---------- Check validation layers ---------------------------
	//
	#ifdef DEBUG 
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

	u32 ExtensionCount;
	char** extensions = GetRequiredExtensions( &ExtensionCount );
	createInfo.enabledExtensionCount = static_cast<uint32_t>( ExtensionCount );
	createInfo.ppEnabledExtensionNames = (const char**)extensions;
 
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
 
	createInfo.enabledLayerCount   = 1;
	createInfo.ppEnabledLayerNames = VALIDATION_LAYERS;
 
	// Set for the debug info the validation layers we want to use
	#ifdef DEBUG
	PopulateDebugMessengerCreateInfo(debugCreateInfo);
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
	#else
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = nullptr;
	#endif
 
	// CREATE INSTANCE
	//
	if ( vkCreateInstance( &createInfo, nullptr, &Instance ) != VK_SUCCESS )
	{
		fprintf( stderr, "[ERROR] Failed to create instance\n" );
		exit( 1 );
	}
 
	// SETUP WINDOW SURFACE
	//
	if( glfwCreateWindowSurface( Instance, Window.Window, nullptr, &Window.Surface ) != VK_SUCCESS )
	{
		fprintf( stderr, "[ERROR] Could not create window surface\n" );
		exit( 1 );
	}

	#ifdef DEBUG
 
	// SETUP DEBUG MESSENGER
	//
	VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
	PopulateDebugMessengerCreateInfo(debug_create_info);
 
	if (CreateDebugUtilsMessengerEXT(Instance, &debug_create_info, nullptr, &DebugMessenger ) != VK_SUCCESS)
	{
		fprintf( stderr, "[ERROR] Failed to setup debug messenger\n" );
		exit( 1 );
	}
 
	#endif

	// ---------------------- SETUP DEVICE --------------------------
	//
	queue_family_indices qfi;
	u32 DeviceCount = 0;
	vkEnumeratePhysicalDevices( Instance, &DeviceCount, nullptr );
	if( DeviceCount == 0 )
	{
		fprintf( stderr, "[ERROR] Could not find any GPU with vulkan support\n" );
		exit( 1 );
	}
	VkPhysicalDevice* Devices = TempArena->Push<VkPhysicalDevice>(DeviceCount);
	vkEnumeratePhysicalDevices(Instance, &DeviceCount, Devices);

	for( u32 i = 0; i < DeviceCount; ++i )
	{
		if( IsSuitableDevice( Devices[i] ) )
		{
			Device.PhysicalDevice = Devices[i];
			break;
		}
	}

	if ( Device.PhysicalDevice == VK_NULL_HANDLE )
	{
		fprintf( stderr, "[ERROR] Could not find a suitable GPU!\n" );
		exit(1);
	}

	qfi = FindQueueFamilies(Device.PhysicalDevice);
	Device.FamilyIndices = qfi;

	f32 queue_priority = 1.0f;
	VkDeviceQueueCreateInfo queue_create_info{};
	queue_create_info.sType              = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex   = qfi.GraphicsAndCompute;
	queue_create_info.queueCount         = 1;
	queue_create_info.pQueuePriorities   = &queue_priority;

	VkPhysicalDeviceSynchronization2Features synchronization2Features = {};
	synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	synchronization2Features.synchronization2 = VK_TRUE;

	VkPhysicalDeviceFeatures device_features{};
	device_features.samplerAnisotropy = VK_TRUE;

	VkPhysicalDeviceFeatures2 deviceFeatures = {};
	deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures.pNext = &synchronization2Features; // Chain synchronization features
	deviceFeatures.features = device_features;

	VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
	indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	indexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
	indexingFeatures.runtimeDescriptorArray = VK_TRUE;
	indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
	indexingFeatures.pNext = &deviceFeatures;


	VkDeviceCreateInfo create_info{};
	create_info.sType                 = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos     = &queue_create_info;
	create_info.queueCreateInfoCount  = 1;
	create_info.pEnabledFeatures      = nullptr;
	create_info.enabledExtensionCount = 1;
	create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS;
	create_info.pNext = &indexingFeatures;

	#ifdef DEBUG
	create_info.enabledLayerCount   = 1;
	create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
	#else
	create_info.enabledLayerCount = 0;
	#endif
 
	if( vkCreateDevice( Device.PhysicalDevice, &create_info, nullptr, &Device.LogicalDevice ) != VK_SUCCESS )
	{
		fprintf( stderr, "[ERROR] Could not create logical device\n" );
		exit( 1 );
	}
 
	vkGetDeviceQueue(Device.LogicalDevice, qfi.GraphicsAndCompute, 0, &Device.GraphicsQueue);
	vkGetDeviceQueue(Device.LogicalDevice, qfi.Presentation, 0, &Device.PresentationQueue);
	//vkGetDeviceQueue(Device.LogicalDevice, qfi.GraphicsAndCompute, 0, &Device.ComputeQueue);

	CreateSwapchain();
	CreateImageViews();
	InitCommands();
	InitSyncStructures();
}

// -------------------- PRIVATE CLASS FUNCTIONS ---------------------
//

swapchain_support_details
vulkan_iface::QuerySwapChainSupport( VkPhysicalDevice Device )
{
	swapchain_support_details details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device, Window.Surface, &details.Capabilities);
 
	vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Window.Surface, &details.FormatCount, nullptr);
 
	if (details.FormatCount != 0) {
		details.Formats = TempArena->Push<VkSurfaceFormatKHR>( details.FormatCount );
		vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Window.Surface, &details.FormatCount, details.Formats);
	}
 
	vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Window.Surface, &details.PresentModeCount, nullptr);
 
	if (details.PresentModeCount != 0) {
		details.PresentModes = TempArena->Push<VkPresentModeKHR>( details.PresentModeCount );
		vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Window.Surface, &details.PresentModeCount, details.PresentModes);
	}
 
	return details;
}

// ------------------------------------------------------------------

bool
vulkan_iface::CheckDeviceExtensionSupport( VkPhysicalDevice Device )
{
	u32 extensionCount;
	vkEnumerateDeviceExtensionProperties(Device, nullptr, &extensionCount, nullptr);
 
	VkExtensionProperties* available_extensions = TempArena->Push<VkExtensionProperties>(extensionCount);
	vkEnumerateDeviceExtensionProperties(Device, nullptr, &extensionCount, available_extensions );
 
	u32 TotalExtensions = extensionCount;
	for (u32 extIdx = 0; extIdx < 3; extIdx += 1) {
		bool extFound = false;
		for (u32 i = 0; i < extensionCount; ++i)
		{
			if (strcmp(available_extensions[i].extensionName, DEVICE_EXTENSIONS[extIdx]) == 0)
			{
				extFound = true;
			}
		}
		if (!extFound) {
			TotalExtensions -= 1;
		}
	}

	if (TotalExtensions == extensionCount) {
		return true;
	} 
	else {
		return false;
	}
}

// ------------------------------------------------------------------

queue_family_indices
vulkan_iface::FindQueueFamilies( VkPhysicalDevice& device )
{
	queue_family_indices indices;
	indices.GraphicsAndCompute = (1 << 30) - 1;
	indices.Presentation       = (1 << 30) - 1;
 
	u32 queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( device, &queue_family_count, nullptr );
 
	VkQueueFamilyProperties* queue_families = TempArena->Push<VkQueueFamilyProperties>( queue_family_count );
	vkGetPhysicalDeviceQueueFamilyProperties( device, &queue_family_count, queue_families );
 
	size_t i;
	for( i = 0; i < queue_family_count; ++i )
	{
		if( (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) &&
			( queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) )
		{
			indices.GraphicsAndCompute = i;
		}
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR( device, i, Window.Surface, &present_support );
  
		if( present_support )
		{
			indices.Presentation = i;
		}
  
		if( indices.GraphicsAndCompute != ((1 << 30) - 1) && indices.Presentation != ((1 << 30) - 1) )
		{
			break;
		}
		i++;
	}
  
	return indices;
}

// ------------------------------------------------------------------

bool
vulkan_iface::IsSuitableDevice(VkPhysicalDevice& device) 
{
	u32 U32_MAX = (1 << 30) - 1;
	queue_family_indices indices = FindQueueFamilies( device );
 
	bool extensions_supported = CheckDeviceExtensionSupport( device );
 
	bool swap_chain_adequate = false;
 
	if( extensions_supported )
	{
		swapchain_support_details swap_chain_support = QuerySwapChainSupport( device );
		swap_chain_adequate = swap_chain_support.Formats != 0 && swap_chain_support.PresentModes != 0;  
	}
 
	return indices.GraphicsAndCompute != U32_MAX && indices.Presentation != U32_MAX &&
		extensions_supported && swap_chain_adequate;
}

// ------------------------------------------------------------------

bool vulkan_iface::CheckValidationLayerSupport()
{
	uint32_t LayerCount;
	vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);
 
	VkLayerProperties* availableLayers = RenderArena->Push<VkLayerProperties>(LayerCount);
	vkEnumerateInstanceLayerProperties( &LayerCount, availableLayers );
 
	for( uint32_t i = 0; i < LayerCount; ++i )
	{
		if ( strcmp( availableLayers[i].layerName, VALIDATION_LAYERS[0] ) == 0 )
		{
			RenderArena->Pop<VkLayerProperties>(LayerCount);
			return true;
		}
	}
 
	RenderArena->Pop<VkLayerProperties>(LayerCount);
	return false;
}

// ------------------------------------------------------------------

char**
vulkan_iface::GetRequiredExtensions( uint32_t* ExtensionCount )
{
	u32 glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
 
	#ifdef DEBUG
	// Add the extension that goes for debugging
	//
	u64 tmp = glfwExtensionCount + 1;
	char** extensions = RenderArena->Push<char*>( tmp );
	#else
	char** extensions = RenderArena->Push<char*>( glfwExtensionCount );
	#endif
 
	u64 i;
	for( i = 0; i < glfwExtensionCount; ++i )
	{
		extensions[i] = RenderArena->Push<char>(strlen( glfwExtensions[i] ) + 1 );
		strcpy( extensions[i], glfwExtensions[i] );
	}
 
	#ifdef DEBUG
	extensions[glfwExtensionCount] = RenderArena->Push<char>(strlen(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) );
	strcpy( extensions[glfwExtensionCount], VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
	++glfwExtensionCount;
	#endif
 
	*ExtensionCount = glfwExtensionCount;
 
	return extensions;
}

// ------------------------------------------------------------------

VkCommandBufferSubmitInfo vulkan_iface::CommandBufferSubmitInfo(VkCommandBuffer cmd)
{
	VkCommandBufferSubmitInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	info.pNext = nullptr;
	info.commandBuffer = cmd;
	info.deviceMask = 0;

	return info;
}

// ------------------------------------------------------------------

VkSemaphoreSubmitInfo vulkan_iface::SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
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

VkSubmitInfo2 vulkan_iface::SubmitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo)
{
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