#ifndef VULKAN_IMPL_H
#define VULKAN_IMPL_H

#include <stdio.h>
#include <string.h>

// TODO: ERASE AND REPLACE
//
#include <cstdlib>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h> 

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../types/types.h"
#include "../math/math.h"
#include "../memory/memory.h" 

#define VK_CHECK(x)                                                    \
do {                                                                   \
	VkResult err = x;                                                  \
        if (err) {                                                     \
			printf("Detected Vulkan error: %s", string_VkResult(err)); \
			abort();                                                   \
        }                                                              \
} while (0)

#define MAX_FRAMES_IN_FLIGHT 2

class window {
public:
    i32          Width;
    i32          Height;
	GLFWwindow*  Window { nullptr };
    VkSurfaceKHR Surface;
    Vec2         ScalingFactor;
    bool         FocusedWindow;
};

struct queue_family_indices {
	u32 GraphicsAndCompute;
	u32 Presentation;
};

struct swapchain_support_details
{
	VkSurfaceCapabilitiesKHR Capabilities;
	VkSurfaceFormatKHR*      Formats;
	VkPresentModeKHR*        PresentModes;
	u32 FormatCount;
	u32 PresentModeCount;
};

class vk_device {
public:
	VkPhysicalDevice PhysicalDevice;
	VkDevice LogicalDevice;
	VkQueue GraphicsQueue;
	VkQueue ComputeQueue;
	VkQueue PresentationQueue;
	queue_family_indices FamilyIndices;
};

class vk_swapchain {
public:
	VkSwapchainKHR 	         Swapchain;
	VkImage*       	         Images { nullptr };
	u32            	         N_Images;
	VkImageView*   	         ImageViews { nullptr };
	u32            	         N_ImageViews;
	VkFormat                 Format;
	VkExtent2D               Extent;
	VkSurfaceCapabilitiesKHR Capabilities;
	VkSurfaceFormatKHR*      SurfaceFormats { nullptr };
	VkPresentModeKHR*        PresentModes { nullptr };
};

class vk_pipeline {
public:
	VkPipelineLayout Layout;
	VkPipeline Pipeline;

	enum PipelineType {
		GRAPHICS2D,
		GRAPHICS3D,
		COMPUTE,
		RAY_TRACING,
		PipelineType_COUNT
	};
};

class vk_descriptor_set {
public:
	VkDescriptorSet DescriptorSets[MAX_FRAMES_IN_FLIGHT];
	VkDescriptorSetLayout DescriptorSetLayout;
};

class vk_semaphore {
public:
	VkSemaphore ImageAvailable [MAX_FRAMES_IN_FLIGHT];
	VkSemaphore RenderFinished [MAX_FRAMES_IN_FLIGHT];
	VkSemaphore ComputeFinished [MAX_FRAMES_IN_FLIGHT];
	VkFence     InFlight [MAX_FRAMES_IN_FLIGHT];
	VkFence     ComputerInFlight [MAX_FRAMES_IN_FLIGHT];
};

class vertex3d {
public:
	Vec3 Pos;
	Vec3 Color;
	Vec3 TexCoord;
};

class vertex2d {
public:
	Vec2 P0; // left corner
	Vec2 P1; // bottom right corner
	Vec4 Color00;
	Vec4 Color01;
	Vec4 Color10;
	Vec4 Color11;
	Vec2 TexCoordP0;
	Vec2 TexCoordP1;
	f32 CornerRadius;
	f32 EdgeSoftness;
	f32 BorderThickness;
};

class vk_image {
public:
	u32 Width;
	u32 Height;
	VkFormat Format;
	VkImageTiling Tiling;
	VkImageUsageFlags UsageFlags;
	VkMemoryPropertyFlags Properties;
	VkImage Image;
	VkDeviceMemory Memory;
	VkImageView ImageView;
	VkImageLayout Layout;
	VkSampler Sampler;
};

class vk_buffer {
public:
	VkBuffer              Buffer;
	VkDeviceMemory        DeviceMemory;
	void*                 MappedMemory { nullptr };
	VkDeviceSize          Size;
	VkDeviceSize          MaxSize;
	VkMemoryPropertyFlags Properties;
	VkBufferUsageFlags    UsageFlags;
	u32                   InstanceCount;
	bool                  ToDestroy;
};

struct uniform_buffer_ui {
	f32 Time;
	f32 DeltaTime;
	f32 Width;
	f32 Height;
	f32 AtlasWidth;
	f32 AtlasHeight;
};

struct batch_2d {
	vertex2d* Vertices { nullptr };
	u32* Indices { nullptr };
	u32* Instances { nullptr };
};

class buffer_batch_group {
public:
	vk_buffer* Buffer { nullptr };
	u32 N_Batches;
	u32 CurrentBatchIdx;
};

class vulkan_iface {
    public:
	// ----- Global variables ---------------
	//
    global inline const char* VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
    global inline  const char* DEVICE_EXTENSIONS[] =  {
        "VK_KHR_swapchain",
    	"VK_KHR_dynamic_rendering",
    	"VK_EXT_descriptor_indexing"
    };

	// -------- General ---------------------------------------------
	//
	VkInstance         Instance;
	window             Window;
	vk_device          Device;
	vk_swapchain       Swapchain;
	vk_pipeline*       Pipelines;
	VkDescriptorPool   DescriptorPool;
	vk_descriptor_set* Descriptors;
	vk_semaphore       Semaphores;

	// ----- Compute pipeline ---------------------------------------
	//
	VkBuffer* ShaderStorage;
	VkDeviceMemory* ShaderMemory;

	// ----- Uniform Buffers ----------------------------------------
	//
	vk_buffer UniformBufferUI[MAX_FRAMES_IN_FLIGHT];
	uniform_buffer_ui CurrentUniformBufferUI;

	// ----- Vulkan command buffers ---------------------------------
	//
	VkCommandPool   CommandPool[MAX_FRAMES_IN_FLIGHT];
	VkCommandBuffer CommandBuffers[MAX_FRAMES_IN_FLIGHT];
	VkCommandBuffer ComputeCommandBuffers[MAX_FRAMES_IN_FLIGHT];

	// ----- 2D Buffers ---------------------------------------------
	//
	buffer_batch_group BufferBatch2D;
	buffer_batch_group BufferBatch2D_Destroy[MAX_FRAMES_IN_FLIGHT];

	// ----- Depth Image -------------------------------------------
	//
	vk_image DepthImage;

	// ----- Our Texture Images -------------------------------------
	//
	vk_image* TextureImage;

	// ----- For frames ---------------------------------------------
	//
	u32  CurrentFrame       { 0  };
	f64  LastTimeFrame      { 0.0f  };
	bool FramebufferResized { false };
	f64  LastTime           { 0.0f  };

	// ----- Debug Uitilities ---------------------------------------
	// 
	VkDebugUtilsMessengerEXT DebugMessenger;


	// ----- Main Functions -----------------------------------------
	//
	vulkan_iface( const char* window_name );
	~vulkan_iface() {}
	vulkan_iface(vulkan_iface& v) = delete;
	vulkan_iface(vulkan_iface&& v) = delete;

	void CreateSwapchain();
	void CreateImageViews();
	void InitCommands();
	void InitSyncStructures();
	void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

	void BeginDrawing();

    private:
	Arena* RenderArena;
	Arena* TempArena;

	VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer cmd);
	VkSemaphoreSubmitInfo SemaphoreSubmitInfo( VkPipelineStageFlags2 stageMask, VkSemaphore semaphore );
	VkSubmitInfo2 SubmitInfo( VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo );
	VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags /*= 0*/);

	VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags /*= 0*/);

	VkCommandPoolCreateInfo CommandPoolCreateInfo(u32 queueFamilyIndex, VkCommandPoolCreateFlags flags /*= 0*/);
	
	VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool pool, uint32_t count /*= 1*/);
	
	swapchain_support_details QuerySwapChainSupport(VkPhysicalDevice Device);
	
	bool CheckDeviceExtensionSupport(VkPhysicalDevice Device);
	
	queue_family_indices FindQueueFamilies(VkPhysicalDevice& device);
	
	bool IsSuitableDevice(VkPhysicalDevice& device);
	
	bool CheckValidationLayerSupport();
	
	char** GetRequiredExtensions(uint32_t* ExtensionCount);
};

#endif // VULKAN_IMPL_H
