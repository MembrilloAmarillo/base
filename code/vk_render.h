#ifndef _VK_RENDER_H_
#define _VK_RENDER_H_

#include <vulkan/vulkan.h>

#if __linux__
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#endif

#include "third-party/vk_mem_alloc.h"

#include <stdlib.h>
#include <stdio.h>

#include "types.h"

/* ----------------------------------------------------------------------------- */

const char* VALIDATION_LAYERS[] = { "VK_LAYER_KHRONOS_validation" };
const char* DEVICE_EXTENSIONS[] = {
    "VK_KHR_swapchain",
	"VK_KHR_dynamic_rendering",
    "VK_EXT_descriptor_indexing",
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
};

#define MAX_FRAMES_IN_FLIGHT 2

/* ---------- helper macros -------------------------------------------------- */

#define VK_CHECK(expr)                                                         \
do {                                                                       \
VkResult _res = (expr);                                                \
if (_res != VK_SUCCESS) {                                              \
fprintf(stderr, "[ERROR] %s:%d VK_CHECK failed (%d)\n",            \
__FILE__, __LINE__, _res);                                 \
exit(EXIT_FAILURE);                                                \
}                                                                      \
} while (0)

static inline uint32_t clamp_u32(uint32_t val, uint32_t min, uint32_t max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

typedef struct api_clipboard api_clipboard;
struct api_clipboard {
    Atom Clipboard;
    Atom Utf8;
    Atom Paste;
};

typedef struct api_window api_window;
struct api_window {
    F64          Width;
    F64          Height;
    VkSurfaceKHR Surface;
    Display     *Dpy;
    Window       Win;
    XIC          xic;
    api_clipboard  Clipboard;
};

///////////////////////////////////////////////////////////////////////////////
// Pipeline builder structs
//
typedef enum {
    PIPELINE_TYPE_GRAPHICS2D,
    PIPELINE_TYPE_GRAPHICS3D,
    PIPELINE_TYPE_COMPUTE,
    PIPELINE_TYPE_RAY_TRACING,
    PIPELINE_TYPE_COUNT,
} PipelineType;

typedef struct pipeline_builder pipeline_builder;
struct pipeline_builder {
    Stack_Allocator* Allocator;
    void*  ss_buffer;
    void*  pc_buffer;
    vector ShaderStages;
    vector PushConstants;
    VkPipelineInputAssemblyStateCreateInfo InputAssembly;
    VkPipelineRasterizationStateCreateInfo Rasterizer;
    VkPipelineColorBlendAttachmentState ColorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo Multisampling;
    VkPipelineDepthStencilStateCreateInfo DepthStencil;
    VkPipelineRenderingCreateInfo RenderInfo;
    VkPipelineLayout PipelineLayout;
    VkFormat ColorAttachmentFormat;
    VkDescriptorSetLayout* DescriptorLayout;
    u32 DescriptorLayoutCount;
    VkVertexInputAttributeDescription* AttributeDescriptions;
    size_t AttributeDescriptionCount;
    VkVertexInputBindingDescription* BindingDescriptions;
    size_t BindingDescriptionCount;
};

typedef struct {
    VkPipeline Pipeline;
    VkPipelineCache Cache;
    VkPipelineLayout Layout;
    VkDescriptorSet* Descriptors;
    size_t DescriptorCount;
    struct allocated_buffer* v_buffer;
    struct allocated_buffer* i_buffer;
} vk_pipeline;

///////////////////////////////////////////////////////////////////////////////
//
typedef struct queue_family_indices queue_family_indices;
struct queue_family_indices {
    u32 GraphicsAndCompute;
    u32 Presentation;
};

///////////////////////////////////////////////////////////////////////////////
//
typedef struct swapchain_support_details swapchain_support_details;
struct swapchain_support_details {
    VkSurfaceCapabilitiesKHR Capabilities;
    VkSurfaceFormatKHR      *Formats;
    VkPresentModeKHR        *PresentModes;
};

typedef struct swapchain swapchain;
struct swapchain {
    VkSwapchainKHR Swapchain;
    VkImage*       Images;
    u32            N_Images;
    VkImageView*   ImageViews;
    u32            N_ImageViews;
    VkFormat       Format;
    VkExtent2D     Extent;
    VkSurfaceCapabilitiesKHR  Capabilities;
    VkSurfaceFormatKHR*       SurfaceFormats;
    VkPresentModeKHR*         PresentModes;
};

///////////////////////////////////////////////////////////////////////////////
// Related to descriptors
typedef struct pool_size_ratio pool_size_ratio;
struct pool_size_ratio {
    VkDescriptorType Type;
    f32 Ratio;
};

typedef struct vk_descriptor_set vk_descriptor_set;
struct vk_descriptor_set {
    VkDescriptorSet       DescriptorSets;
    VkDescriptorSetLayout DescriptorSetLayout;
    vector                bindings; // type: VkDescriptorSetLayoutBinding
    
    void*                  BackBuffer;
    Stack_Allocator*       Allocator;
};

typedef struct descriptor_allocator_growable descriptor_allocator_growable;
struct descriptor_allocator_growable {
    vector Ratios;     // type: pool_size_ratio
    vector FullPools;  // type: VkDescriptorPool
    vector ReadyPools; // type: VkDescriptorPool
    
    void*                  RatiosBuffer;
    void*                  FullPoolsBuffer;
    void*                  ReadyPoolsBuffer;
    Stack_Allocator*       Allocator;
    
    i32 SetsPerPool;
};

typedef struct descriptor_writer descriptor_writer;
struct descriptor_writer {
    queue  ImageInfos;  // VkDescriptorImageInfo
    queue  BufferInfos; // VkDescriptorBufferInfo
    vector Writes;      // VkWriteDescriptorSet
    
    void*            ImageInfosBuffer;
    void*            BufferInfosBuffer;
    void*            WritesBuffer;
    Stack_Allocator* Allocator;
};

///////////////////////////////////////////////////////////////////////////////
//
typedef struct v_3d v_3d;
struct v_3d {
    vec3 Position;
    vec2 UV;
    vec3 Normal;
    vec4 Color;
};

typedef struct v_2d v_2d;
struct v_2d {
    vec2 LeftCorner;
    vec2 Size;
    vec2 UV;
    vec2 UVSize;
    vec4 Color;
    f32  CornerRadius;
};

///////////////////////////////////////////////////////////////////////////////
//
typedef struct allocated_buffer allocated_buffer;
struct allocated_buffer {
	VkBuffer          Buffer;
	VmaAllocation     Allocation;
	VmaAllocationInfo Info;
};

///////////////////////////////////////////////////////////////////////////////
//
typedef struct vk_image vk_image;
struct vk_image {
    u32 Width;
    u32 Height;
    VkExtent3D Extent;
    VkFormat Format;
    VkImageTiling Tiling;
    VkImageUsageFlags UsageFlags;
    VkMemoryPropertyFlags Properties;
    VkImage Image;
    VkDeviceMemory Memory;
    VkImageView ImageView;
    VkImageLayout Layout;
    VkSampler Sampler;
    VmaAllocation Alloc;
};

///////////////////////////////////////////////////////////////////////////////
//
typedef struct gpu_sync gpu_sync;
struct gpu_sync {
    VkSemaphore *ImageAvailable;
    VkSemaphore *RenderFinished;
    VkSemaphore *ComputeFinished;
    VkFence *InFlight;
    VkFence *ComputeInFlight;
};

///////////////////////////////////////////////////////////////////////////////
//
typedef struct vulkan_base vulkan_base;
struct vulkan_base{
    api_window           Window;
    VkInstance           Instance;
    
    VkDescriptorPool     GlobalDescriptorAllocator;
    
    /////////////////////////////////////////////
    // Device Settings
    VkDevice             Device;
    VkPhysicalDevice     PhysicalDevice;
    queue_family_indices FamilyIndices;
    VkQueue              GraphicsQueue;
    VkQueue              ComputeQueue;
    VkQueue              PresentationQueue;
    
    /////////////////////////////////////////////
    // Sempahores and fences
    gpu_sync Semaphores;
    
    /////////////////////////////////////////////
	// Immediate submite
    VkFence         ImmFence;
    VkCommandBuffer ImmCommandBuffer;
    VkCommandPool   ImmCommandPool;
    
    /////////////////////////////////////////////
    // Command buffer details
    VkCommandPool CommandPool[MAX_FRAMES_IN_FLIGHT];
    VkCommandBuffer CommandBuffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandBuffer ComputeCommandBuffers[MAX_FRAMES_IN_FLIGHT];
    
    /////////////////////////////////////////////
    // Swapchain details
    swapchain Swapchain;
    bool      FramebufferResized;
    u32       CurrentFrame;
    u32       SwapchainImageIdx;
    
    /////////////////////////////////////////////
    // Allocation
    Stack_Allocator Allocator;
    Stack_Allocator TempAllocator;
    Arena*          Arena;
    VmaAllocator    GPUAllocator;
    
    /////////////////////////////////////////////
    // Debugging
    VkDebugUtilsMessengerEXT DebugMessenger;
};

/** \brief Creates a new window with specified dimensions
 *
 *  Initializes and returns a window handle with the requested width and height.
 *  The window is configured for Vulkan rendering and appropriate event handling.
 *
 *  \param w Width of the window in pixels
 *  \param h Height of the window in pixels
 *  \return Handle to the created window
 */
internal api_window CreateWindow(F64 w, F64 h);

/** \brief Initializes the Vulkan subsystem
 *
 *  Sets up all required Vulkan components including instance, physical device,
 *  logical device, and essential resources. Must be called before any other
 *  Vulkan operations.
 *
 *  \return Initialized vulkan_base structure containing all Vulkan resources
 */
internal vulkan_base VulkanInit();

/** \brief Creates a Vulkan swapchain for rendering
 *
 *  Creates and configures a swapchain appropriate for the current surface,
 *  ensuring proper image count, format, and presentation mode.
 *
 *  \param base Pointer to the vulkan_base structure containing device and surface
 */
internal void CreateSwapchain(vulkan_base* base);

/** \brief Creates image views for swapchain images
 *
 *  Creates VkImageView objects for each image in the swapchain, allowing them
 *  to be used as framebuffers and render targets.
 *
 *  \param base Pointer to the vulkan_base structure containing swapchain images
 */
internal void CreateImageViews(vulkan_base* base);

/** \brief Initializes command pools and command buffers
 *
 *  Creates primary command pools and allocates command buffers for each frame
 *  in flight. Sets up appropriate command buffer usage patterns.
 *
 *  \param base Pointer to the vulkan_base structure
 */
internal void InitCommands(vulkan_base* base);

/** \brief Initializes synchronization primitives
 *
 *  Creates fences and semaphores needed for frame pacing and GPU-CPU synchronization.
 *  Configures appropriate signaling and waiting patterns.
 *
 *  \param base Pointer to the vulkan_base structure
 */
internal void InitSyncStructures(vulkan_base* base);

/** \brief Initializes descriptor sets and layouts
 *
 *  Creates descriptor set layouts and allocates descriptor sets from the pool.
 *  Configures bindings according to the application's requirements.
 *
 *  \param base Pointer to the vulkan_base structure
 */
internal void InitDescriptors(vulkan_base* base);

/** \brief Initializes a descriptor pool
 *
 *  Creates a descriptor pool with specified ratios for different descriptor types.
 *  Useful for custom descriptor pool configuration beyond default initialization.
 *
 *  \param base Pointer to the vulkan_base structure
 *  \param Pool Pointer to where the created VkDescriptorPool will be stored
 *  \param Device Vulkan device handle
 *  \param MaxSets Maximum number of descriptor sets the pool can allocate
 *  \param pool_ratios Array of pool size ratios for different descriptor types
 *  \param N_Pools Number of elements in the pool_ratios array
 */
internal void InitDescriptorPool(vulkan_base* base, VkDescriptorPool* Pool, VkDevice Device, u32 MaxSets, pool_size_ratio* pool_ratios, u32 N_Pools);

/** \brief Resets all descriptor sets in a descriptor pool
 *
 *  Resets a descriptor pool, making all descriptor sets allocated from it
 *  available for reuse. This is more efficient than destroying and recreating
 *  the pool when you need to update all descriptors.
 *
 *  \param allocator Pointer to the descriptor pool to reset
 *  \param device Vulkan device associated with the descriptor pool
 *
 *  \note After resetting, all descriptor sets from this pool become invalid
 *        and must be重新 allocated before use
 *  \note This operation does not free the underlying memory resources,
 *        it only marks the descriptor sets as available for reuse
 */
internal void ClearDescriptorPool(VkDescriptorPool* allocator, VkDevice device);

/** \brief Destroys a descriptor pool and frees its resources
 *
 *  Completely destroys a descriptor pool and all descriptor sets allocated
 *  from it, releasing all associated memory resources back to the system.
 *
 *  \param allocator Pointer to the descriptor pool to destroy
 *  \param device Vulkan device associated with the descriptor pool
 *
 *  \note This function should only be called when the descriptor pool is
 *        no longer needed and all descriptor sets from it are no longer in use
 *  \note After destruction, the descriptor pool handle becomes invalid and
 *        must not be used in any Vulkan commands
 */
internal void DestroyPool(VkDescriptorPool* allocator, VkDevice device);

/** \brief Creates a Vulkan buffer with associated memory allocation
 *
 *  Creates a Vulkan buffer object and allocates memory for it using the
 *  Vulkan Memory Allocator (VMA). The buffer is mapped into host memory
 *  if VMA_ALLOCATION_CREATE_MAPPED_BIT is specified.
 *
 *  \param allocator VMA memory allocator instance
 *  \param AllocSize Size of the buffer in bytes
 *  \param Usage Buffer usage flags (e.g., VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
 *  \param MemoryUsage Intended memory usage pattern (e.g., VMA_MEMORY_USAGE_GPU_ONLY)
 *  \return Structure containing the created buffer, its memory allocation, and allocation info
 *
 *  \note The function automatically sets VMA_ALLOCATION_CREATE_MAPPED_BIT,
 *        so the buffer memory is mapped and can be accessed directly from CPU
 *  \note The buffer uses exclusive sharing mode (not shared between queues)
 *  \note Uses VK_CHECK macro to validate the creation operation
 */
internal allocated_buffer CreateBuffer(VmaAllocator allocator, VkDeviceSize AllocSize,
                                       VkBufferUsageFlags Usage, VmaMemoryUsage MemoryUsage);


/** \brief Creates a VkFenceCreateInfo structure
 *
 *  Helper function to initialize a VkFenceCreateInfo structure with specified flags.
 *  Sets appropriate structure type and default values for other fields.
 *
 *  \param flags Creation flags for the fence (signaled state, etc.)
 *  \return Fully initialized VkFenceCreateInfo structure
 */
internal VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags);

/** \brief Creates a VkSemaphoreCreateInfo structure
 *
 *  Helper function to initialize a VkSemaphoreCreateInfo structure with specified flags.
 *  Sets appropriate structure type and default values for other fields.
 *
 *  \param flags Creation flags for the semaphore
 *  \return Fully initialized VkSemaphoreCreateInfo structure
 */
internal VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags flags);

/** \brief Initializes a pipeline builder
 *
 *  Creates and initializes a pipeline_builder structure with pre-allocated
 *  space for shader stages. The builder is ready to be configured for pipeline creation.
 *
 *  \param n_shader_stages Number of shader stages to pre-allocate space for
 *  \param Allocator Memory allocator to use for dynamic allocations
 *  \return Initialized pipeline_builder structure
 */
pipeline_builder InitPipelineBuilder(size_t n_shader_stages, Stack_Allocator* Allocator);

/** \brief Resets a pipeline builder to default state
 *
 *  Clears all configuration from the pipeline builder, resetting all states
 *  to defaults. Frees any allocated resources within the builder.
 *
 *  \param builder Pointer to the pipeline_builder to reset
 */
void ClearPipelineBuilder(pipeline_builder* builder);

/** \brief Creates a shader stage configuration
 *
 *  Helper function to initialize a VkPipelineShaderStageCreateInfo structure
 *  for a specific shader module and entry point.
 *
 *  \param stage_flag Shader stage (e.g., VK_SHADER_STAGE_VERTEX_BIT)
 *  \param module Shader module containing the code
 *  \param entry Entry point name in the shader (typically "main")
 *  \return Fully initialized VkPipelineShaderStageCreateInfo structure
 */
VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage_flag, VkShaderModule module, const char* entry);

/** \brief Adds a push constant range to the pipeline
 *
 *  Configures the pipeline to use push constants for the specified shader stages
 *  with the given size. Multiple calls accumulate push constant ranges.
 *
 *  \param builder Pointer to the pipeline builder
 *  \param stage Shader stages that will use these push constants
 *  \param size Size of the push constant data in bytes
 */
void AddPushConstant(pipeline_builder* builder, VkShaderStageFlags stage, size_t size);

/** \brief Sets vertex and fragment shaders for the pipeline
 *
 *  Configures the pipeline builder with the specified vertex and fragment shaders.
 *  Replaces any previously set shaders.
 *
 *  \param builder Pointer to the pipeline builder
 *  \param vertex_shader Vertex shader module
 *  \param fragment_shader Fragment shader module
 */
void SetShaders(pipeline_builder* builder, VkShaderModule vertex_shader, VkShaderModule fragment_shader);

/** \brief Sets primitive topology for the pipeline
 *
 *  Configures how vertices are assembled into primitives (triangles, lines, etc.).
 *
 *  \param builder Pointer to the pipeline builder
 *  \param topology Primitive assembly topology (e.g., triangle list, line strip)
 */
void SetInputTopology(pipeline_builder* builder, VkPrimitiveTopology topology);

/** \brief Sets descriptor set layout for the pipeline
 *
 *  Configures the descriptor set layout that will be used by the pipeline.
 *
 *  \param builder Pointer to the pipeline builder
 *  \param layout Pointer to the descriptor set layout
 */
void SetDescriptorLayout(pipeline_builder* builder, VkDescriptorSetLayout* layout, u32 LayoutCount );

/** \brief Configures pipeline for no multisampling
 *
 *  Sets up the multisample state to use a single sample per pixel with no
 *  anti-aliasing or coverage effects.
 *
 *  \param builder Pointer to the pipeline builder
 */
void SetMultisamplingNone(pipeline_builder* builder);

/** \brief Disables depth testing in the pipeline
 *
 *  Configures the pipeline to ignore depth values, disabling both depth testing
 *  and depth writing. Useful for 2D rendering or transparent objects.
 *
 *  \param builder Pointer to the pipeline builder
 */
void DisableDepthTest(pipeline_builder* builder);

/** \brief Builds a complete graphics pipeline
 *
 *  Creates a Vulkan graphics pipeline from the configured pipeline builder.
 *  Handles all necessary validation and error reporting.
 *
 *  \param builder Pointer to the configured pipeline builder
 *  \param device Vulkan device to create the pipeline on
 *  \return The created VkPipeline, or VK_NULL_HANDLE on failure
 */
VkPipeline BuildPipeline(pipeline_builder* builder, VkDevice device);

/** \brief Sets vertex attribute descriptions
 *
 *  Configures how vertex data is interpreted by the vertex shader.
 *  Defines the format and location of each vertex attribute.
 *
 *  \param builder Pointer to the pipeline builder
 *  \param v Array of vertex attribute descriptions
 *  \param count Number of elements in the array
 */
void SetVertexInputAttributeDescription(pipeline_builder* builder, const VkVertexInputAttributeDescription* v, size_t count);

/** \brief Sets vertex binding descriptions
 *
 *  Configures how vertex data is fetched from buffers, including stride and
 *  input rate (per-vertex or per-instance).
 *
 *  \param builder Pointer to the pipeline builder
 *  \param v Array of vertex binding descriptions
 *  \param count Number of elements in the array
 */
void SetVertexInputBindingDescription(pipeline_builder* builder, const VkVertexInputBindingDescription* v, size_t count);

/** \brief Sets polygon rendering mode
 *
 *  Configures how polygons are rasterized (filled, wireframe, points).
 *
 *  \param builder Pointer to the pipeline builder
 *  \param polygon_mode Rasterization mode (fill, line, point)
 */
void SetPolygonMode(pipeline_builder* builder, VkPolygonMode polygon_mode);

/** \brief Sets face culling configuration
 *
 *  Configures which faces are culled (front, back, or none) and how front faces
 *  are determined (clockwise or counter-clockwise).
 *
 *  \param builder Pointer to the pipeline builder
 *  \param cull_mode Which faces to cull (VK_CULL_MODE_BACK_BIT, etc.)
 *  \param front_face How front faces are determined (VK_FRONT_FACE_CLOCKWISE, etc.)
 */
void SetCullMode(pipeline_builder* builder, VkCullModeFlags cull_mode, VkFrontFace front_face);

/** \brief Disables color blending
 *
 *  Configures the pipeline to write color values directly without blending.
 *  Color write mask is set to all channels.
 *
 *  \param builder Pointer to the pipeline builder
 */
void DisableBlending(pipeline_builder* builder);

/** \brief Enables additive blending
 *
 *  Configures the pipeline for additive blending (SRC_ALPHA, ONE).
 *  Commonly used for particle effects and light accumulation.
 *
 *  \param builder Pointer to the pipeline builder
 */
void EnableBlendingAdditive(pipeline_builder* builder);

/** \brief Enables standard alpha blending
 *
 *  Configures the pipeline for standard alpha blending (SRC_ALPHA, ONE_MINUS_SRC_ALPHA).
 *  Used for transparent objects where the background shows through.
 *
 *  \param builder Pointer to the pipeline builder
 */
void EnableBlendingAlphaBlend(pipeline_builder* builder);

/** \brief Sets depth attachment format
 *
 *  Configures the pipeline to use the specified format for depth attachments.
 *  Required for depth testing and writing.
 *
 *  \param builder Pointer to the pipeline builder
 *  \param format Vulkan format to use for depth (e.g., VK_FORMAT_D32_SFLOAT)
 */
void SetDepthFormat(pipeline_builder* builder, VkFormat format);

/** \brief Sets color attachment format
 *
 *  Configures the pipeline to use the specified format for color attachments.
 *  Must match the swapchain image format for presentation.
 *
 *  \param builder Pointer to the pipeline builder
 *  \param format Vulkan format to use for color (e.g., VK_FORMAT_B8G8R8A8_SRGB)
 */
void SetColorAttachmentFormat(pipeline_builder* builder, VkFormat format);

/** \brief Creates and adds a complete graphics pipeline
 *
 *  Loads shaders, builds pipeline layout, and creates a complete graphics pipeline.
 *  Handles shader module creation and cleanup internally.
 *
 *  \param base Pointer to the vulkan_base structure
 *  \param builder Pointer to the configured pipeline builder
 *  \param vert_path Path to the vertex shader file
 *  \param frag_path Path to the fragment shader file
 *  \return vk_pipeline structure containing the created pipeline and related resources
 */
vk_pipeline AddPipeline(struct vulkan_base* base, pipeline_builder* builder, const char* vert_path, const char* frag_path);

/** \brief Frees resources allocated by the pipeline builder
 *
 *  Releases all memory and resources associated with the pipeline builder.
 *  Should be called when the builder is no longer needed.
 *
 *  \param builder Pointer to the pipeline builder to destroy
 */
void DestroyPipelineBuilder(pipeline_builder* builder);

/**
 * @brief Opens and read a file containing a shader and loads it into a VkShaderModule
 *
 * @param filename  String containing the path to the file
 * @param device    Logical device
 * @param outModule Pointer to the VkShaderModule the user wants
 */
/** \brief Loads a shader module from a SPIR-V file
 *
 *  Reads a SPIR-V shader file and creates a Vulkan shader module.
 *  The caller is responsible for destroying the created shader module
 *  with vkDestroyShaderModule when no longer needed.
 *
 *  \param filename Path to the SPIR-V shader file
 *  \param device Vulkan device to create the shader module on
 *  \param[out] outModule Pointer to store the created shader module handle
 *  \return true if successful, false otherwise
 *
 *  \note The function uses memory-mapped I/O for efficient file reading
 */
internal bool LoadShaderModule(const char* filename, VkDevice device, VkShaderModule* outModule);

/** \brief Initializes a descriptor set builder
 *
 *  Sets up a descriptor set builder structure with pre-allocated space
 *  for descriptor bindings.
 *
 *  \param ds Pointer to the descriptor set structure to initialize
 *  \param max_descriptor_set_layout_binding Maximum number of bindings to pre-allocate
 *  \param Allocator Memory allocator to use for dynamic allocations
 */
internal void InitDescriptorSet(vk_descriptor_set* ds, u32 max_descriptor_set_layout_binding, Stack_Allocator* Allocator);

/** \brief Adds a binding to a descriptor set
 *
 *  Configures a new descriptor binding with the specified type.
 *  The binding is added to the end of the current bindings list.
 *
 *  \param ds Pointer to the descriptor set
 *  \param binding Binding point index
 *  \param type Descriptor type (e.g., VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
 *
 *  \note The stageFlags field is initialized to 0 and should be set later
 *        when building the descriptor set layout
 */
internal void AddBindingDescriptorSet(vk_descriptor_set* ds, u32 binding, VkDescriptorType type);

/** \brief Clears all bindings from a descriptor set
 *
 *  Resets the descriptor set builder to an empty state, removing all
 *  previously added bindings.
 *
 *  \param ds Pointer to the descriptor set to clear
 */
internal void ClearDescriptorSet(vk_descriptor_set* ds);

/** \brief Builds a descriptor set layout
 *
 *  Creates a Vulkan descriptor set layout from the configured bindings.
 *  The stageFlags for each binding are updated with the provided shader stages.
 *
 *  \param ds Pointer to the descriptor set builder
 *  \param device Vulkan device to create the layout on
 *  \param shader_stages Shader stages that will access the descriptors
 *  \param p_next Optional pNext chain for extended functionality
 *  \param flags Creation flags for the descriptor set layout
 *  \return Created VkDescriptorSetLayout handle
 */
internal VkDescriptorSetLayout BuildDescriptorSet(vk_descriptor_set* ds, VkDevice device, VkShaderStageFlags shader_stages, void* p_next, VkDescriptorSetLayoutCreateFlags flags);

/** \brief Allocates a descriptor set
 *
 *  Allocates a descriptor set from the specified descriptor pool.
 *
 *  \param Pool Pointer to the descriptor pool to allocate from
 *  \param device Vulkan device
 *  \param layout Descriptor set layout to use for the allocation
 *  \return Allocated VkDescriptorSet handle
 */
internal VkDescriptorSet DescriptorSetAllocate(VkDescriptorPool* Pool, VkDevice device, VkDescriptorSetLayout* layout);

/** \brief Initializes a descriptor writer
 *
 *  Sets up a descriptor writer for building descriptor writes in batches.
 *  Pre-allocates space for the specified capacity of descriptors.
 *
 *  \param capacity Maximum number of descriptors to support
 *  \param Allocator Memory allocator to use for dynamic allocations
 *  \return Initialized descriptor_writer structure
 */
internal descriptor_writer DescriptorWriterInit(u32 capacity, Stack_Allocator* Allocator);

/** \brief Clears a descriptor writer
 *
 *  Resets the descriptor writer to an empty state, removing all pending writes.
 *  Does not free the underlying memory allocations.
 *
 *  \param dw Pointer to the descriptor writer to clear
 */
internal void DescriptorWriterClear(descriptor_writer* dw);

/** \brief Adds an image descriptor write operation
 *
 *  Configures a descriptor write operation for an image resource.
 *  The write operation is queued and will be applied when UpdateDescriptorSet is called.
 *
 *  \param dw Pointer to the descriptor writer
 *  \param binding Binding point to write to
 *  \param Image Image view to bind
 *  \param Sample Sampler to use (can be VK_NULL_HANDLE)
 *  \param Layout Image layout the shader will access the image in
 *  \param Type Descriptor type (e.g., VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
 */
internal void WriteImage(descriptor_writer* dw, i32 binding, VkImageView Image, VkSampler Sample, VkImageLayout Layout, VkDescriptorType Type);

/** \brief Adds a buffer descriptor write operation
 *
 *  Configures a descriptor write operation for a buffer resource.
 *  The write operation is queued and will be applied when UpdateDescriptorSet is called.
 *
 *  \param dw Pointer to the descriptor writer
 *  \param Binding Binding point to write to
 *  \param Buffer Buffer to bind
 *  \param Size Size of the buffer region to bind (in bytes)
 *  \param Offset Offset into the buffer (in bytes)
 *  \param Type Descriptor type (e.g., VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
 */
internal void WriteBuffer(descriptor_writer* dw, i32 Binding, VkBuffer Buffer, i32 Size, i32 Offset, VkDescriptorType Type);

/** \brief Updates a descriptor set with all queued writes
 *
 *  Submits all pending descriptor writes to the specified descriptor set.
 *  After this call, the descriptor set contains the configured resources.
 *
 *  \param dw Pointer to the descriptor writer containing the writes
 *  \param Device Vulkan device
 *  \param Set Descriptor set to update
 */
internal void UpdateDescriptorSet(descriptor_writer* dw, VkDevice Device, VkDescriptorSet Set);

/** \brief Creates a VkImageCreateInfo structure with common parameters
 *
 *  Helper function to initialize a VkImageCreateInfo structure for 2D images
 *  with optimal tiling and default configuration.
 *
 *  \param Format Image format
 *  \param Flags Usage flags for the image
 *  \param Extent Image dimensions (width, height, depth)
 *  \return Fully initialized VkImageCreateInfo structure
 */
internal VkImageCreateInfo ImageCreateInfo(VkFormat Format, VkImageUsageFlags Flags, VkExtent3D Extent);

/** \brief Creates a VkImageViewCreateInfo structure
 *
 *  Helper function to initialize a VkImageViewCreateInfo structure for
 *  2D images with the specified aspect flags.
 *
 *  \param Format Image view format
 *  \param image Image to create the view for
 *  \param flags Aspect flags (e.g., VK_IMAGE_ASPECT_COLOR_BIT)
 *  \return Fully initialized VkImageViewCreateInfo structure
 */
internal VkImageViewCreateInfo ImageViewCreateInfo(VkFormat Format, VkImage image, VkImageAspectFlags flags);

/** \brief Creates a default image with associated resources
 *
 *  Creates a Vulkan image with the specified properties, allocates memory
 *  for it, and creates an image view. Handles mipmapping if requested.
 *
 *  \param base Pointer to the vulkan_base structure
 *  \param Size Image dimensions
 *  \param Format Image format
 *  \param Usage Image usage flags
 *  \param is_mipmapped Whether to generate mipmaps
 *  \return vk_image structure containing the created image resources
 *
 *  \note The ImageView field in the returned structure must point to
 *        externally allocated memory that will store the image view handle
 */
internal vk_image CreateImageDefault(vulkan_base* base, VkExtent3D Size, VkFormat Format, VkImageUsageFlags Usage, bool is_mipmapped);
internal vk_image CreateImageData(vulkan_base* base, void* data, VkExtent3D Size, VkFormat Format, VkImageUsageFlags Usage, bool is_mipmapped);

/** \brief Creates a VkCommandBufferBeginInfo structure
 *
 *  Helper function to initialize a VkCommandBufferBeginInfo structure
 *  with the specified usage flags.
 *
 *  \param flags Command buffer usage flags
 *  \return Fully initialized VkCommandBufferBeginInfo structure
 */
internal VkCommandBufferBeginInfo CommandBufferBeginInfo(VkCommandBufferUsageFlags flags);

/** \brief Begins an immediate submit command buffer
 *
 *  Prepares the immediate submit command buffer for recording by resetting
 *  the associated fence and command buffer.
 *
 *  \param base Pointer to the vulkan_base structure
 *  \return Command buffer ready for recording
 *
 *  \note This is part of a pattern for single-use command buffers that
 *        are submitted immediately after recording
 */
internal VkCommandBuffer ImmediateSubmitBegin(vulkan_base* base);

/** \brief Creates a VkCommandBufferSubmitInfo structure
 *
 *  Helper function to initialize a VkCommandBufferSubmitInfo structure
 *  for command buffer submission.
 *
 *  \param cmd Command buffer to submit
 *  \return Fully initialized VkCommandBufferSubmitInfo structure
 */
internal VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer cmd);

/** \brief Creates a VkSubmitInfo2 structure
 *
 *  Helper function to initialize a VkSubmitInfo2 structure for command
 *  submission with optional semaphore signaling.
 *
 *  \param cmd Command buffer submission info
 *  \param signalSemaphoreInfo Semaphore info for signaling (can be NULL)
 *  \param waitSemaphoreInfo Semaphore info for waiting (can be NULL)
 *  \return Fully initialized VkSubmitInfo2 structure
 */
internal VkSubmitInfo2 SubmitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo);

/** \brief Completes and submits an immediate command buffer
 *
 *  Ends recording of the command buffer, submits it to the queue,
 *  and waits for completion using the associated fence.
 *
 *  \param base Pointer to the vulkan_base structure
 *  \param cmd Command buffer to submit
 *
 *  \note This function blocks until the command buffer execution completes
 */
internal void ImmediateSubmitEnd(vulkan_base* base, VkCommandBuffer cmd);

/** \brief Transition an image's layout using Vulkan 1.3 synchronization model
 *
 *  Creates and executes an image memory barrier to transition an image between layouts.
 *  This is the full parameter version supporting custom synchronization flags.
 *
 *  \param cmd Command buffer to record the barrier into
 *  \param image Image to transition
 *  \param currentLayout Current layout of the image
 *  \param newLayout Target layout for the image
 *  \param srcStageMask Pipeline stages that must complete before the barrier
 *  \param srcAccessMask Memory accesses that must complete before the barrier
 *  \param dstStageMask Pipeline stages that must wait for the barrier
 *  \param dstAccessMask Memory accesses that must wait for the barrier
 */
internal void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout,
                              VkImageLayout newLayout, VkPipelineStageFlags2 srcStageMask,
                              VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask,
                              VkAccessFlags2 dstAccessMask);

#define TransitionImageDefault(cmd, image, currentLayout, newLayout) (TransitionImage(cmd, image, currentLayout, newLayout, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT))

/**
 * \brief Copies image content from one image to another using blitting
 *
 * Performs an image copy operation using vkCmdBlitImage2, which allows for
 * scaling and format conversion during the copy. The function sets up the
 * necessary blit regions and executes the command in the provided command buffer.
 *
 * \param cmd Command buffer to record the blit operation into
 * \param source Source image to copy from (must be in TRANSFER_SRC_OPTIMAL layout)
 * \param destination Destination image to copy to (must be in TRANSFER_DST_OPTIMAL layout)
 * \param srcSize Dimensions of the source image region
 * \param dstSize Dimensions of the destination image region
 *
 * \note Both images must have been transitioned to the appropriate layouts
 *       (TRANSFER_SRC_OPTIMAL for source, TRANSFER_DST_OPTIMAL for destination)
 *       before calling this function.
 * \note This function only handles color aspect images with a single mip level
 *       and array layer.
 * \note The blit operation uses linear filtering for any required scaling.
 */
void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

/** \brief Recreates the swapchain and associated resources
 *
 *  Destroys the current swapchain and image views, then recreates them
 *  with the current window dimensions. This function should be called
 *  when the window is resized or when the swapchain becomes out of date.
 *
 *  \param base Pointer to the vulkan_base structure containing Vulkan resources
 *
 *  \note This function blocks until the device becomes idle, so it should
 *        not be called frequently during normal rendering.
 *  \note After recreation, the framebuffer resized flag is cleared.
 */
internal void RecreateSwapchain(vulkan_base* base);

/** \brief Prepares the next frame for rendering
 *
 *  Waits for the previous frame to complete, resets synchronization objects,
 *  and acquires the next image from the swapchain. Handles cases where
 *  the swapchain needs to be recreated due to resizing or other conditions.
 *
 *  \param base Pointer to the vulkan_base structure
 *  \return true if the swapchain was recreated, false otherwise
 *
 *  \note This function may return true without an error if the swapchain
 *        was recreated due to being suboptimal (VK_SUBOPTIMAL_KHR).
 *  \note If VK_ERROR_OUT_OF_DATE_KHR is returned, the swapchain must be
 *        recreated before proceeding with rendering.
 */
internal bool PrepareFrame(vulkan_base* base);

/** \brief Begins command buffer recording for a new frame
 *
 *  Resets and begins recording a command buffer for the current frame.
 *  Configures the command buffer for one-time submission.
 *
 *  \param base Pointer to the vulkan_base structure
 *  \return Command buffer ready for recording commands
 *
 *  \note The returned command buffer is already in the recording state
 *        and should have rendering commands recorded into it.
 *  \note This function assumes that PrepareFrame has already been called
 *        successfully for the current frame.
 */
internal VkCommandBuffer BeginRender(vulkan_base* base);

/** \brief Creates a VkSemaphoreSubmitInfo structure
 *
 *  Helper function to initialize a semaphore submission info structure
 *  for use with vkQueueSubmit2.
 *
 *  \param stageMask Pipeline stage where the semaphore will be used
 *  \param semaphore Semaphore to submit
 *  \return Fully initialized VkSemaphoreSubmitInfo structure
 *
 *  \note The function sets default values for deviceIndex (0) and value (1)
 */
internal VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);

/** \brief Completes rendering and presents the frame
 *
 *  Ends command buffer recording, submits it for execution, and presents
 *  the rendered image to the window. Handles swapchain recreation if needed.
 *
 *  \param base Pointer to the vulkan_base structure
 *  \param cmd Command buffer that was used for rendering
 *  \return true if the swapchain was recreated, false otherwise
 *
 *  \note This function handles the complete presentation sequence:
 *        - Ending command buffer recording
 *        - Submitting the command buffer with proper synchronization
 *        - Presenting the rendered image
 *        - Handling swapchain recreation if necessary
 *  \note The CurrentFrame index is updated at the end of this function
 *        to rotate through the frame resources.
 */
internal bool EndRender(vulkan_base* base, VkCommandBuffer cmd);

#endif // _VK_RENDER_H_

#ifdef VK_RENDER_IMPL

///////////////////////////////////////////////////////////////////////////////
//
VkBool32
debugCallback(VkDebugUtilsMessageSeverityFlagsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void* pUserData)
{
    printf("validation layer: %s\n", pCallbackData->pMessage);
    
    return VK_FALSE;
}

///////////////////////////////////////////////////////////////////////////////
//
internal void
populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT* createInfo) {
	createInfo->sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	createInfo->messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo->pfnUserCallback = debugCallback;
	createInfo->pUserData       = NULL;
}

///////////////////////////////////////////////////////////////////////////////
//
internal queue_family_indices
FindQueueFamilies(vulkan_base* base, VkPhysicalDevice Device) {
    queue_family_indices Indices;
    Indices.GraphicsAndCompute = UINT32_MAX;
    Indices.Presentation       = UINT32_MAX;
    
    u32 QueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, 0);
    
    VkQueueFamilyProperties *Families = stack_push(&base->Allocator, VkQueueFamilyProperties, QueueFamilyCount );
    
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, Families);
    for( u32 i = 0; i < QueueFamilyCount; i += 1 ) {
        VkQueueFamilyProperties Family = Families[i];
        if((Family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (Family.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            Indices.GraphicsAndCompute = i;
        }
        
        VkBool32 present_support = false;
        if( vkGetPhysicalDeviceSurfaceSupportKHR(Device, i, base->Window.Surface, &present_support) != VK_SUCCESS ) {
            fprintf(stderr, "[ERROR] vkGetPhysicalDeviceSurfaceSupportKHR was not successfull, %d (%s)\n", __LINE__, __FILE__);
            exit(-1);
        }
        
        if( present_support ) {
            Indices.Presentation = i;
        }
        if( Indices.GraphicsAndCompute != UINT32_MAX && Indices.Presentation != UINT32_MAX) {
            printf("[INFO] Found adequate queue families\n");
            //stack_free(&base->Allocator, Families);
            return Indices;
        }
    }
    
    //stack_free(&base->Allocator, Families);
    return Indices;
}

///////////////////////////////////////////////////////////////////////////////
//
internal bool
CheckValidationLayerSupport(vulkan_base* base) {
    u32 LayerCount = 0;
    VkLayerProperties* Layers;
    
    vkEnumerateInstanceLayerProperties(&LayerCount, 0);
    
    Layers = stack_push(&base->Allocator, VkLayerProperties, LayerCount);
    vkEnumerateInstanceLayerProperties(&LayerCount, Layers);
    
    for( u32 i = 0; i < ArrayCount(VALIDATION_LAYERS); i += 1) {
        const char* name = VALIDATION_LAYERS[i];
        bool LayerFound = false;
        for( u32 j = 0; j < LayerCount && !LayerFound; j += 1 ) {
            const char* layer_name = (const char*)Layers[i].layerName;
            if( strcmp(layer_name, name) == 0 ) {
                LayerFound = true;
            }
        }
        if(!LayerFound) {
            fprintf(stderr, "ERROR: validation layer %s not available\n", name);
            return false;
        }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////
//
internal bool
CheckDeviceExtensionSupport( vulkan_base* base, VkPhysicalDevice Device ) {
    u32 ExtensionCount = 0;
    VkExtensionProperties* Extensions = 0;
    bool ExtensionFound = false;
    
    vkEnumerateDeviceExtensionProperties(Device, 0, &ExtensionCount, 0);
    Extensions = stack_push(&base->Allocator, VkExtensionProperties, ExtensionCount);
    vkEnumerateDeviceExtensionProperties(Device, 0, &ExtensionCount, Extensions);
    
    printf("[INFO] Device extensions:\n");
    for( u32 j = 0; j < ExtensionCount; j += 1 ) {
        printf("\t%s\n", Extensions[j].extensionName);
    }
    
    for( u32 i = 0; i < ArrayCount(DEVICE_EXTENSIONS); i += 1 ) {
        const char* extension = DEVICE_EXTENSIONS[i];
        ExtensionFound = false;
        printf("[INFO] Searching for extension: %s\n", extension);
        for( u32 j = 0; j < ExtensionCount; j += 1 ) {
            if( strcmp(extension, Extensions[j].extensionName) == 0 ) {
                printf("[INFO] Found device extension %s\n", extension);
				ExtensionFound = true;
            }
        }
        if( !ExtensionFound ) {
            //stack_free(&base->Allocator, Extensions);
            return false;
        }
    }
    
    //stack_free(&base->Allocator, Extensions);
    return true;
}

///////////////////////////////////////////////////////////////////////////////
//
internal swapchain_support_details
QuerySwapchainSupport(vulkan_base* base, VkPhysicalDevice Device )
{
    swapchain_support_details Details = {0};
    u32 FormatCount = 0;
    u32 PresentModeCount = 0;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device, base->Window.Surface, &Details.Capabilities);
    vkGetPhysicalDeviceSurfaceFormatsKHR(Device, base->Window.Surface, &FormatCount, 0);
    
    if( FormatCount != 0 ) {
        Details.Formats = stack_push(&base->Allocator, VkSurfaceFormatKHR, FormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(Device, base->Window.Surface, &FormatCount, Details.Formats);
    }
    
    vkGetPhysicalDeviceSurfacePresentModesKHR(Device, base->Window.Surface, &PresentModeCount, 0);
    if( PresentModeCount != 0 ) {
        Details.PresentModes = stack_push(&base->Allocator, VkPresentModeKHR, FormatCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(Device, base->Window.Surface, &PresentModeCount, Details.PresentModes);
    }
    
    return Details;
}

///////////////////////////////////////////////////////////////////////////////
//
internal bool
IsSuitableDevice(vulkan_base* base, VkPhysicalDevice Device) {
    queue_family_indices Indices = FindQueueFamilies(base, Device);
	bool extensions_supported = CheckDeviceExtensionSupport(base, Device);
	bool swapchain_adequate = false;
    
	if( extensions_supported ) {
		swapchain_support_details swapchain_support = QuerySwapchainSupport(base, Device);
		swapchain_adequate = ArrayCount(swapchain_support.Formats) > 0 && ArrayCount(swapchain_support.PresentModes) > 0;
        
		//stack_free( &base->Allocator, swapchain_support.Formats      );
		//stack_free( &base->Allocator, swapchain_support.PresentModes );
	}
    
	return Indices.GraphicsAndCompute != UINT32_MAX && Indices.Presentation != UINT32_MAX && extensions_supported && swapchain_adequate;
}

///////////////////////////////////////////////////////////////////////////////
//
internal api_window
CreateWindow(F64 w, F64 h) {
    api_window app;
    
    app.Dpy = XOpenDisplay(NULL);
    if (!app.Dpy) { fprintf(stderr, "Cannot open display\n"); exit(1); }
    
    const int screen = DefaultScreen(app.Dpy);
    app.Clipboard.Clipboard = XInternAtom(app.Dpy, "CLIPBOARD", false);
    app.Clipboard.Utf8      = XInternAtom(app.Dpy, "UTF8_STRING", false);
    app.Clipboard.Paste     = XInternAtom(app.Dpy, "X_PASTE_PROPERTY", False);
    
    Atom wm_delete_window = XInternAtom(app.Dpy, "WM_DELETE_WINDOW", False);
    
    Window win = XCreateSimpleWindow(
                                     app.Dpy, RootWindow(app.Dpy, screen),
                                     0, 0, w, h, 0,
                                     BlackPixel(app.Dpy, screen),
                                     WhitePixel(app.Dpy, screen)
                                     );
    
    XSelectInput(app.Dpy, win,
                 ExposureMask     |
                 PointerMotionMask|
                 ButtonPressMask  |
                 ButtonReleaseMask|
                 KeyPressMask     |
                 KeyReleaseMask   |
                 FocusChangeMask
                 );
    
    app.Width = w;
    app.Height = h;
    XMapWindow(app.Dpy, win);
    XStoreName(app.Dpy, win, "GFX Context");
    XFlush(app.Dpy);
    
    app.Win = win;
    
    XIM xim = XOpenIM(app.Dpy, NULL, NULL, NULL);
    if (!xim) {
        /* Fallback: no IME support */
        fprintf(stderr, "[ERROR] No IME Support\n");
    }
    
    /* 4. Create an XIC for each window that needs text input */
    XIC xic = XCreateIC(xim,
                        XNInputStyle,   XIMPreeditNothing | XIMStatusNothing,
                        XNClientWindow, win,
                        XNFocusWindow,  win,
                        NULL);
    
    app.xic = xic;
    
    XSetWMProtocols(app.Dpy, app.Win, &wm_delete_window, 1);
    
    return app;
}

///////////////////////////////////////////////////////////////////////////////
//
internal vulkan_base
VulkanInit() {
    vulkan_base Base = {0};
    
    Base.Arena = ArenaAllocDefault();
    void* data   = PushArray(Base.Arena, void, gigabyte(1));
    void* TempData = PushArray(Base.Arena, void, mebibyte(256));
    stack_init(&Base.Allocator, data, gigabyte(1));
    stack_init(&Base.TempAllocator, TempData, mebibyte(256));
    VkDebugUtilsMessengerCreateInfoEXT DebugCreateInfo = {};
    char** ExtensionNames = NULL;
    u32 ExtensionCount = 0;
    
    Base.Window = CreateWindow(1200, 1200);
    
    VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "GUI";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;
    
    {
        uint32_t count = 0;
        vkEnumerateInstanceLayerProperties(&count, NULL);
        if (!count) {
            puts("No layers found — SDK not installed or VK_LAYER_PATH missing.");
            exit(1);
        }
        
        VkLayerProperties props[count];
        vkEnumerateInstanceLayerProperties(&count, props);
        
        puts("Available layers:");
        for (uint32_t i = 0; i < count; ++i)
            printf("  %2u  %s  (spec v%u)\n",
                   i, props[i].layerName, props[i].specVersion);
    }
    
    {
        vkEnumerateInstanceExtensionProperties(NULL, &ExtensionCount, NULL);
        
        VkExtensionProperties props[ExtensionCount];
        
        /* 2. Get data */
        vkEnumerateInstanceExtensionProperties(NULL, &ExtensionCount, props);
        
        for (uint32_t i = 0; i < ExtensionCount; ++i)
            printf(" %2u  %-46s  v%u\n", i, props[i].extensionName, props[i].specVersion);
        
#ifndef NDEBUG
        ExtensionCount += 1;
#endif
        
        // TODO: Change to own custom memory allocator
        //
        ExtensionNames = stack_push(&Base.Allocator, char*, ExtensionCount);
        if( ExtensionNames == NULL ) {
            fprintf(stderr, "[ERROR] Could not allocate memory %d %d", __LINE__, __FUNCTION__);
        }
        for(u32 i = 0; i < ExtensionCount - 1; i += 1) {
            ExtensionNames[i] = stack_push(&Base.Allocator, char, strlen(props[i].extensionName) + 1);
            if( ExtensionNames[i] == NULL ) {
                fprintf(stderr, "[ERROR] Could not allocate memory %d %d", __LINE__, __FUNCTION__);
            }
            strcpy(ExtensionNames[i], props[i].extensionName);
        }
        
#ifndef NDEBUG
        ExtensionNames[ExtensionCount-1] = stack_push(&Base.Allocator, char, strlen("VK_EXT_debug_utils") + 1);
        strcpy(ExtensionNames[ExtensionCount-1], "VK_EXT_debug_utils");
#endif
        
        VkInstanceCreateInfo InstanceInfo = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = NULL,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = 1,
            .ppEnabledLayerNames = VALIDATION_LAYERS,
            .enabledExtensionCount = ExtensionCount,
            .ppEnabledExtensionNames = ExtensionNames
        };
        
        
#ifndef NDEBUG
        populate_debug_messenger_create_info(&DebugCreateInfo);
        InstanceInfo.pNext = &DebugCreateInfo;   // <- add this
        InstanceInfo.enabledLayerCount = 1;
#else
        InstanceInfo.enabledLayerCount = 0;
        InstanceInfo.pNext = NULL;
#endif
        
        VK_CHECK(vkCreateInstance(&InstanceInfo, 0, &Base.Instance));
    }
    
    VkXlibSurfaceCreateInfoKHR ci = {};
    ci.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    ci.dpy    = Base.Window.Dpy;
    ci.window = Base.Window.Win;
    
    VkSurfaceKHR surface;
    if (vkCreateXlibSurfaceKHR(Base.Instance, &ci, NULL, &surface) != VK_SUCCESS) {
        fprintf(stderr, "vkCreateXlibSurfaceKHR failed\n");
        exit(EXIT_FAILURE);
    }
    
    Base.Window.Surface = surface;
    
#ifndef NDEBUG
    PFN_vkCreateDebugUtilsMessengerEXT pfnCreateDebugUtilsMessengerEXT;
    pfnCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(Base.Instance, "vkCreateDebugUtilsMessengerEXT");
    
    if (!pfnCreateDebugUtilsMessengerEXT) {
        fprintf(stderr, "Could not load vkCreateDebugUtilsMessengerEXT\n");
        exit(EXIT_FAILURE);
    }
    
    if( pfnCreateDebugUtilsMessengerEXT(Base.Instance, &DebugCreateInfo, 0, &Base.DebugMessenger) != VK_SUCCESS) {
        fprintf(stderr, "[ERROR] Could not create debug utils\n");
        exit(-1);
    }
#endif
    
    ///////////////////////////////////////////////////////////////////////////
    // SETUP PHYSICAL DEVICE
    //
    {
        queue_family_indices qfi;
        u32 DeviceCount = 0;
        vkEnumeratePhysicalDevices(Base.Instance, &DeviceCount, 0);
        if( DeviceCount == 0 ) {
            fprintf(stderr, "[ERROR] No device with vulkan support found\n");
            exit(-1);
        }
        VkPhysicalDevice* Devices = stack_push(&Base.Allocator, VkPhysicalDevice, DeviceCount);
        vkEnumeratePhysicalDevices(Base.Instance, &DeviceCount, Devices);
        
        for( u32 i = 0; i < DeviceCount; i += 1 ) {
            VkPhysicalDeviceProperties properties ;
            vkGetPhysicalDeviceProperties(Devices[i], &properties);
            
            printf("Device Name: %s\n", properties.deviceName);
            if( IsSuitableDevice(&Base, Devices[i]) ) {
                Base.PhysicalDevice = Devices[i];
                printf("[INFO] Selected GPU: %s\n", properties.deviceName);
                break;
            }
        }
        
        if( Base.PhysicalDevice == VK_NULL_HANDLE ) {
	        fprintf(stderr, "[ERROR] Could not find a suitable GPU!\n");
            exit(-1);
	    }
        //stack_free(&Base.Allocator, Devices);
    }
    
    //////////////////////////////////////////////////////////////////////////
    // SETUP LOGICAL DEVICE
    //
    queue_family_indices qfi = FindQueueFamilies(&Base, Base.PhysicalDevice);
    Base.FamilyIndices = qfi;
    
    f32 queue_priority = 1.0;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = qfi.GraphicsAndCompute;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    
    // Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = true;
    features13.synchronization2 = true;
    features13.pNext = 0;
    
    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;
    features12.pNext = &features13;
    
    // Core Vulkan features
    VkPhysicalDeviceFeatures2 device_features = {};
    device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features.features.samplerAnisotropy = true;
    device_features.features.fillModeNonSolid = true;
    device_features.pNext = &features12;
    
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pQueueCreateInfos = &queue_create_info;
    create_info.queueCreateInfoCount = 1;
    create_info.pEnabledFeatures = 0;
    create_info.enabledExtensionCount = 4;
    create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS;
    create_info.pNext = &device_features;
    
#ifdef NDEBUG // In case we are not in debug, we do not set layers
    create_info.enabledLayerCount = 0;
#endif
    
    vkCreateDevice(Base.PhysicalDevice, &create_info, 0, &Base.Device);
    vkGetDeviceQueue(Base.Device, Base.FamilyIndices.GraphicsAndCompute, 0, &Base.GraphicsQueue);
    vkGetDeviceQueue(Base.Device, Base.FamilyIndices.GraphicsAndCompute, 0, &Base.ComputeQueue);
    vkGetDeviceQueue(Base.Device, Base.FamilyIndices.Presentation, 0, &Base.PresentationQueue);
    
    ///////////////////////////////////////////////////////////////////////////
    // Setup vulkan memory allocator
    //
    static VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    
    VmaAllocatorCreateInfo AllocatorInfo = {};
    AllocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    AllocatorInfo.physicalDevice   = Base.PhysicalDevice;
    AllocatorInfo.device           = Base.Device;
    AllocatorInfo.instance         = Base.Instance;
    AllocatorInfo.flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    //AllocatorInfo.pVulkanFunctions = &vulkanFunctions;
    
    VK_CHECK( vmaCreateAllocator(&AllocatorInfo, &Base.GPUAllocator) );
    
    ///////////////////////////////////////////////////////////////////////////
    // Swapchain creation
    //
    CreateSwapchain(&Base);
    
    ///////////////////////////////////////////////////////////////////////////
    // Image Views creation
    //
    CreateImageViews(&Base);
    
    ///////////////////////////////////////////////////////////////////////////
    // Init command buffers creation
    //
    InitCommands(&Base);
    
    ///////////////////////////////////////////////////////////////////////////
    // Init sync structures
    //
    InitSyncStructures(&Base);
    
    ///////////////////////////////////////////////////////////////////////////
    // Init descriptors
    //
    InitDescriptors(&Base);
    
    ///////////////////////////////////////////////////////////////////////////
    // Free memory from temporal data
    //
    for( int i = ExtensionCount - 1 ; i >= 0; i -= 1 ) {
        //stack_free(&Base.Allocator, ExtensionNames[i]);
    }
    //stack_free(&Base.Allocator, ExtensionNames);
    
    return Base;
}

///////////////////////////////////////////////////////////////////////////////
// clamp helper function
//
internal u32
u32_clamp(u32 value, u32 min, u32 max) {
    if( value < min ) {
        value = min;
    } else if ( value > max ) {
        value = max;
    }
    
    return value;
}

///////////////////////////////////////////////////////////////////////////////
// Vulkan Swapchain creation
//
internal void
CreateSwapchain(vulkan_base* base) {
    swapchain_support_details Support = QuerySwapchainSupport(base, base->PhysicalDevice);
    U32 ImageCount = Support.Capabilities.minImageCount + 1;
    
    VkSurfaceFormatKHR SurfaceFormat;
    VkPresentModeKHR   PresentMode;
    VkExtent2D         Extent;
    
    bool VerticalBlank = false;
    
    if( Support.Capabilities.maxImageCount > 0 && ImageCount > Support.Capabilities.maxImageCount ) {
        ImageCount = Support.Capabilities.maxImageCount;
    }
    
    for(u32 i = 0; i < ArrayCount(Support.Formats); i += 1) {
        VkSurfaceFormatKHR format = Support.Formats[i];
        if( format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
            SurfaceFormat = format;
            break;
        }
    }
    
    for(u32 i = 0; i < ArrayCount(Support.PresentModes); i += 1) {
        VkPresentModeKHR mode = Support.PresentModes[i];
        if( mode == VK_PRESENT_MODE_MAILBOX_KHR ) {
            VerticalBlank = true;
            PresentMode = mode;
        }
    }
    
    if( !VerticalBlank ) {
        PresentMode = VK_PRESENT_MODE_FIFO_KHR;
    }
    
    if( Support.Capabilities.currentExtent.width != UINT32_MAX ) {
        Extent = Support.Capabilities.currentExtent;
    } else {
        U32 width = 0, height = 0, snum = 0;
        snum   = DefaultScreen(base->Window.Dpy);
        width  = DisplayWidth(base->Window.Dpy, snum);
        height = DisplayHeight(base->Window.Dpy, snum);
        
        Extent = (VkExtent2D){ .width = width, .height = height };
        
        Extent.width  = u32_clamp(Extent.width, Support.Capabilities.minImageExtent.width, Support.Capabilities.maxImageExtent.width);
        Extent.height = u32_clamp(Extent.height, Support.Capabilities.minImageExtent.height, Support.Capabilities.maxImageExtent.height);
    }
    VkSwapchainCreateInfoKHR CreateInfo = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = base->Window.Surface,
        .minImageCount    = ImageCount,
        .imageFormat      = SurfaceFormat.format,
        .imageColorSpace  = SurfaceFormat.colorSpace,
        .imageExtent      = Extent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform     = Support.Capabilities.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = PresentMode,
        .clipped          = true,
    };
    
    queue_family_indices indices = FindQueueFamilies(base, base->PhysicalDevice);
    u32 qFamilyIndices[] = {indices.GraphicsAndCompute, indices.Presentation};
    
    if( indices.GraphicsAndCompute != indices.Presentation ) {
        CreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        CreateInfo.queueFamilyIndexCount = 2;
        CreateInfo.pQueueFamilyIndices   = qFamilyIndices;
    } else {
        CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    VK_CHECK(vkCreateSwapchainKHR(base->Device, &CreateInfo, 0, &base->Swapchain.Swapchain));
    VK_CHECK(vkGetSwapchainImagesKHR(base->Device, base->Swapchain.Swapchain, &base->Swapchain.N_Images, 0));
    
    if(base->Swapchain.Images == 0) {
        base->Swapchain.Images = stack_push(&base->Allocator, VkImage, base->Swapchain.N_Images);
    }
    
    VK_CHECK(vkGetSwapchainImagesKHR(base->Device, base->Swapchain.Swapchain, &base->Swapchain.N_Images, base->Swapchain.Images));
    
    base->Swapchain.Format = SurfaceFormat.format;
    base->Swapchain.Extent = Extent;
    base->Swapchain.Capabilities = Support.Capabilities;
    
}

///////////////////////////////////////////////////////////////////////////////
// ImageCreateInfo helper function
//
internal VkImageCreateInfo
ImageCreateInfo(VkFormat Format, VkImageUsageFlags UsageFlags, VkExtent3D Extent) {
    VkImageCreateInfo info = {};
    info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext         = 0;
    info.imageType     = VK_IMAGE_TYPE_2D;
    info.format        = Format;
    info.extent        = Extent;
    info.mipLevels     = 1;
    info.arrayLayers   = 1;
    // No MSAA by default (1 sample per pixel)
    info.samples       = VK_SAMPLE_COUNT_1_BIT;
    // Optimal tiling for GPU access
    info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    info.usage         = UsageFlags;
    
    return info;
}

///////////////////////////////////////////////////////////////////////////////
// Creates image views for swapchain
//
internal void
CreateImageViews(vulkan_base* base) {
    base->Swapchain.N_ImageViews = base->Swapchain.N_Images;
    
    if( base->Swapchain.ImageViews == 0 ) {
        base->Swapchain.ImageViews = stack_push(&base->Allocator, VkImageView, base->Swapchain.N_ImageViews);
    }
    
    for( u32 i = 0; i < base->Swapchain.N_ImageViews; i += 1 ) {
        VkImageViewCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        CreateInfo.image = base->Swapchain.Images[i];
        CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        CreateInfo.format   = base->Swapchain.Format;
        CreateInfo.components = (VkComponentMapping){VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        CreateInfo.subresourceRange = (VkImageSubresourceRange){
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
        };
        
        VK_CHECK(vkCreateImageView(base->Device, &CreateInfo, 0, &base->Swapchain.ImageViews[i]));
    }
}

///////////////////////////////////////////////////////////////////////////////
// Command pool helper functions
//
internal VkCommandPoolCreateInfo
CommandPoolCreateInfo(u32 queueFamilyIndex, VkCommandPoolCreateFlags flags /*= 0*/) {
    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = 0;
    info.queueFamilyIndex = queueFamilyIndex;
    info.flags = flags;
    
    return info;
}

internal VkCommandBufferAllocateInfo
CommandBufferAllocateInfo( VkCommandPool pool, u32 count ) {
    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = 0;
    
    info.commandPool = pool;
    info.commandBufferCount = count;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    
    return info;
}


///////////////////////////////////////////////////////////////////////////////
// Creates the command buffers for the main rendering
//
internal void
InitCommands(vulkan_base* base) {
    VkCommandPoolCreateInfo CommandPoolInfo = CommandPoolCreateInfo(base->FamilyIndices.GraphicsAndCompute, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    
    for( u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i += 1 ) {
        VK_CHECK(vkCreateCommandPool(base->Device, &CommandPoolInfo, 0, &base->CommandPool[i]));
        VkCommandBufferAllocateInfo cmdAllocInfo = CommandBufferAllocateInfo(base->CommandPool[i], 1);
        
        VK_CHECK(vkAllocateCommandBuffers(base->Device, &cmdAllocInfo, &base->CommandBuffers[i]));
    }
    
    VK_CHECK(vkCreateCommandPool(base->Device, &CommandPoolInfo, 0, &base->ImmCommandPool));
	// allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo = CommandBufferAllocateInfo(base->ImmCommandPool, 1);
    
    VK_CHECK(vkAllocateCommandBuffers(base->Device, &cmdAllocInfo, &base->ImmCommandBuffer));
}

///////////////////////////////////////////////////////////////////////////////
// Inits semaphores and fences
//

// -----------------------------------------------------------------------------
internal VkSemaphoreCreateInfo
SemaphoreCreateInfo( VkSemaphoreCreateFlags flags) {
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = 0;
    info.flags = flags;
    
    return info;
}

// ------------------------------------------------------------------
internal VkFenceCreateInfo
FenceCreateInfo(VkFenceCreateFlags flags) {
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = 0;
    
    info.flags = flags;
    
    return info;
}

internal void
InitSyncStructures(vulkan_base* base) {
    // create syncronization structures
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first
    // frame
    VkFenceCreateInfo fenceCreateInfo = FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkFenceCreateFlags flags = {0};
    VkSemaphoreCreateInfo semaphoreCreateInfo = SemaphoreCreateInfo(flags);
    
    base->Semaphores.ImageAvailable  = stack_push(&base->Allocator, VkSemaphore, MAX_FRAMES_IN_FLIGHT);
    base->Semaphores.RenderFinished  = stack_push(&base->Allocator, VkSemaphore, base->Swapchain.N_Images);
    base->Semaphores.ComputeFinished = stack_push(&base->Allocator, VkSemaphore, MAX_FRAMES_IN_FLIGHT);
    base->Semaphores.InFlight        = stack_push(&base->Allocator, VkFence, MAX_FRAMES_IN_FLIGHT);
    base->Semaphores.ComputeInFlight = stack_push(&base->Allocator, VkFence, MAX_FRAMES_IN_FLIGHT);
    
    for( u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i += 1) {
        VK_CHECK(vkCreateFence(base->Device, &fenceCreateInfo, 0, &base->Semaphores.InFlight[i]));
        VK_CHECK(vkCreateSemaphore(base->Device, &semaphoreCreateInfo, 0, &base->Semaphores.ImageAvailable[i]));
        VK_CHECK(vkCreateFence(base->Device, &fenceCreateInfo, 0, &base->Semaphores.ComputeInFlight[i]));
        VK_CHECK(vkCreateSemaphore(base->Device, &semaphoreCreateInfo, 0, &base->Semaphores.ComputeFinished[i]));
    }
    
    for( u32 i = 0; i < base->Swapchain.N_Images; i += 1 ) {
        VK_CHECK(vkCreateSemaphore(base->Device, &semaphoreCreateInfo, 0, &base->Semaphores.RenderFinished[i]));
    }
    
    // Immediate submit fences
    //
    VK_CHECK(vkCreateFence(base->Device, &fenceCreateInfo, 0, &base->ImmFence));
}

///////////////////////////////////////////////////////////////////////////////
// Init descriptors
//
internal void
InitDescriptors(vulkan_base* base) {
    pool_size_ratio* sizes = stack_push( &base->Allocator,  pool_size_ratio,  3);
	sizes[0] = (pool_size_ratio){VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 8};
	sizes[1] = (pool_size_ratio){VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8};
	sizes[2] = (pool_size_ratio){VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8};
    
	InitDescriptorPool(base, &base->GlobalDescriptorAllocator, base->Device, 24, sizes, 3);
}

///////////////////////////////////////////////////////////////////////////////
// Init descriptor pool
//
internal void
InitDescriptorPool(vulkan_base* base, VkDescriptorPool* Pool, VkDevice Device, u32 MaxSets, pool_size_ratio* pool_ratios, u32 N_Pools ) {
    VkDescriptorPoolSize* PoolSizes = stack_push(&base->Allocator, VkDescriptorPoolSize, N_Pools);
    
    for(u32 i = 0; i < N_Pools; i += 1) {
        pool_size_ratio* ratio = &pool_ratios[i];
        VkDescriptorPoolSize pool_size = {0};
        pool_size.type = ratio->Type;
        pool_size.descriptorCount = (u32)(ratio->Ratio * (f32)MaxSets);
        PoolSizes[i] = pool_size;
    }
    
    VkDescriptorPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = (VkDescriptorPoolCreateFlags){0};
    pool_info.maxSets = MaxSets;
    pool_info.poolSizeCount = N_Pools;
    pool_info.pPoolSizes = PoolSizes;
    
    vkCreateDescriptorPool(Device, &pool_info, 0, Pool);
}

internal void
ClearDescriptorPool(VkDescriptorPool* allocator, VkDevice device) {
    vkResetDescriptorPool(device, *allocator, (VkDescriptorPoolResetFlags){0});
}

internal void
DestroyPool(VkDescriptorPool* allocator, VkDevice device) {
    vkDestroyDescriptorPool(device, *allocator, 0);
}

internal  allocated_buffer
CreateBuffer(VmaAllocator allocator, VkDeviceSize AllocSize, VkBufferUsageFlags Usage, VmaMemoryUsage MemoryUsage)
{
    allocated_buffer out = {0};
    
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .size  = AllocSize,
        .usage = Usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    
    VmaAllocationCreateInfo vmaInfo = {
        .usage = MemoryUsage,
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
    };
    
    VK_CHECK(vmaCreateBuffer(allocator,
                             &bufferInfo,
                             &vmaInfo,
                             &out.Buffer,
                             &out.Allocation,
                             &out.Info));
    
    return out;
}

internal void
DestroyBuffer(VmaAllocator* allocator, allocated_buffer* buf)
{
    vmaDestroyBuffer(*allocator, buf->Buffer, buf->Allocation);
}


pipeline_builder InitPipelineBuilder(size_t n_shader_stages, Stack_Allocator* Allocator) {
    pipeline_builder builder = {0};
    
    builder.ss_buffer = stack_push(Allocator, VkPipelineShaderStageCreateInfo, n_shader_stages);
    builder.pc_buffer = stack_push(Allocator, VkPushConstantRange, n_shader_stages);
    builder.Allocator = Allocator;
    
    builder.ShaderStages  = VectorNew(builder.ss_buffer, 0, n_shader_stages, VkPipelineShaderStageCreateInfo);
    builder.PushConstants = VectorNew(builder.pc_buffer, 0, n_shader_stages, VkPushConstantRange);
    
    ClearPipelineBuilder(&builder);
    
    return builder;
}

void ClearPipelineBuilder(pipeline_builder* builder) {
    // Clear all structs back to 0 with their correct sType
    
    builder->InputAssembly = (VkPipelineInputAssemblyStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    };
    
    builder->Rasterizer = (VkPipelineRasterizationStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    };
    
    builder->ColorBlendAttachment = (VkPipelineColorBlendAttachmentState){0};
    
    builder->Multisampling = (VkPipelineMultisampleStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    };
    
    builder->DepthStencil = (VkPipelineDepthStencilStateCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };
    
    builder->RenderInfo = (VkPipelineRenderingCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    };
    
    // Clear shader stages array
    builder->ShaderStages.len = 0;
}

void
DestroyPipelineBuilder(pipeline_builder* builder) {
    //stack_free(builder->Allocator, builder->ss_buffer);
    //stack_free(builder->Allocator, builder->ss_buffer);
}

VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(
                                                              VkShaderStageFlagBits stage_flag,
                                                              VkShaderModule module,
                                                              const char* entry) {
    
    VkPipelineShaderStageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .stage = stage_flag,
        .module = module,
        .pName = entry,
        .pSpecializationInfo = NULL
    };
    
    return info;
}

void AddPushConstant(pipeline_builder* builder, VkShaderStageFlags stage, size_t size) {
    VkPushConstantRange push_constant = {
        .stageFlags = stage,
        .offset = 0,
        .size = (uint32_t)size
    };
    VectorAppend(&builder->PushConstants, &push_constant);
}

void SetShaders(pipeline_builder* builder, VkShaderModule vertex_shader, VkShaderModule fragment_shader) {
    builder->ShaderStages.len = 0;
    
    VkPipelineShaderStageCreateInfo psci = PipelineShaderStageCreateInfo(
                                                                         VK_SHADER_STAGE_VERTEX_BIT,
                                                                         vertex_shader,
                                                                         "main");
    VectorAppend(&builder->ShaderStages, &psci);
    
    psci = PipelineShaderStageCreateInfo(
                                         VK_SHADER_STAGE_FRAGMENT_BIT,
                                         fragment_shader,
                                         "main");
    VectorAppend(&builder->ShaderStages, &psci);
}

void SetInputTopology(pipeline_builder* builder,
                      VkPrimitiveTopology topology) {
    builder->InputAssembly.topology = topology;
    builder->InputAssembly.primitiveRestartEnable = VK_FALSE;
}

void SetDescriptorLayout(pipeline_builder* builder,
                         VkDescriptorSetLayout* layout, u32 LayoutCount ) {
    builder->DescriptorLayout = layout;
    builder->DescriptorLayoutCount = LayoutCount;
}

void SetMultisamplingNone(pipeline_builder* builder) {
    builder->Multisampling.sampleShadingEnable = VK_FALSE;
    builder->Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    builder->Multisampling.minSampleShading = 1.0f;
    builder->Multisampling.pSampleMask = NULL;
    builder->Multisampling.alphaToCoverageEnable = VK_FALSE;
    builder->Multisampling.alphaToOneEnable = VK_FALSE;
}

void DisableDepthTest(pipeline_builder* builder) {
    builder->DepthStencil.depthTestEnable = VK_FALSE;
    builder->DepthStencil.depthWriteEnable = VK_FALSE;
    builder->DepthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    builder->DepthStencil.depthBoundsTestEnable = VK_FALSE;
    builder->DepthStencil.stencilTestEnable = VK_FALSE;
    builder->DepthStencil.minDepthBounds = 0.0f;
    builder->DepthStencil.maxDepthBounds = 1.0f;
}

VkPipeline BuildPipeline(pipeline_builder* builder, VkDevice device) {
    // Make viewport state from our stored viewport and scissor
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };
    
    // Setup color blending
    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &builder->ColorBlendAttachment
    };
    
    // Vertex input state
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };
    
    if (builder->AttributeDescriptionCount > 0) {
        vertex_input_info.vertexAttributeDescriptionCount =
        (u32)builder->AttributeDescriptionCount;
        vertex_input_info.pVertexAttributeDescriptions =
            builder->AttributeDescriptions;
        vertex_input_info.vertexBindingDescriptionCount =
        (u32)builder->BindingDescriptionCount;
        vertex_input_info.pVertexBindingDescriptions =
            builder->BindingDescriptions;
    }
    
    // Build the actual pipeline
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &builder->RenderInfo,
        .stageCount = builder->ShaderStages.len,
        .pStages = builder->ShaderStages.data,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &builder->InputAssembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &builder->Rasterizer,
        .pMultisampleState = &builder->Multisampling,
        .pColorBlendState = &color_blending,
        .pDepthStencilState = &builder->DepthStencil,
        .layout = builder->PipelineLayout
    };
    
    // Dynamic states
    VkDynamicState dynamic_states[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    
    VkPipelineDynamicStateCreateInfo dynamic_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pDynamicStates = dynamic_states,
        .dynamicStateCount = 2
    };
    
    pipeline_info.pDynamicState = &dynamic_info;
    
    // Create the pipeline
    VkPipeline new_pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                  &pipeline_info, NULL, &new_pipeline) != VK_SUCCESS) {
        fprintf(stderr, "[ERROR] Failed to create pipeline\n");
        return VK_NULL_HANDLE;
    }
    
    return new_pipeline;
}

void SetVertexInputAttributeDescription(pipeline_builder* builder,
                                        const VkVertexInputAttributeDescription* v,
                                        size_t count) {
    builder->AttributeDescriptions = malloc(count * sizeof(VkVertexInputAttributeDescription));
    memcpy(builder->AttributeDescriptions, v, count * sizeof(VkVertexInputAttributeDescription));
    builder->AttributeDescriptionCount = count;
}

void SetVertexInputBindingDescription(pipeline_builder* builder,
                                      const VkVertexInputBindingDescription* v,
                                      size_t count) {
    builder->BindingDescriptions = malloc(count * sizeof(VkVertexInputBindingDescription));
    memcpy(builder->BindingDescriptions, v, count * sizeof(VkVertexInputBindingDescription));
    builder->BindingDescriptionCount = count;
}

void SetPolygonMode(pipeline_builder* builder, VkPolygonMode polygon_mode) {
    builder->Rasterizer.polygonMode = polygon_mode;
    builder->Rasterizer.lineWidth = 1.0f;
}

void SetCullMode(pipeline_builder* builder,
                 VkCullModeFlags cull_mode,
                 VkFrontFace front_face) {
    builder->Rasterizer.cullMode = cull_mode;
    builder->Rasterizer.frontFace = front_face;
}

void DisableBlending(pipeline_builder* builder) {
    builder->ColorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    builder->ColorBlendAttachment.blendEnable = VK_FALSE;
}

void EnableBlendingAdditive(pipeline_builder* builder) {
    builder->ColorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    builder->ColorBlendAttachment.blendEnable = VK_TRUE;
    builder->ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    builder->ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    builder->ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    builder->ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    builder->ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    builder->ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void EnableBlendingAlphaBlend(pipeline_builder* builder) {
    builder->ColorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    builder->ColorBlendAttachment.blendEnable = VK_TRUE;
    builder->ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    builder->ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    builder->ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    builder->ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    builder->ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    builder->ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void SetDepthFormat(pipeline_builder* builder, VkFormat format) {
    builder->RenderInfo.depthAttachmentFormat = format;
}

void SetColorAttachmentFormat(pipeline_builder* builder, VkFormat format) {
    builder->ColorAttachmentFormat = format;
    builder->RenderInfo.colorAttachmentCount = 1;
    builder->RenderInfo.pColorAttachmentFormats = &builder->ColorAttachmentFormat;
}

internal bool
LoadShaderModule(const char* filename, VkDevice device, VkShaderModule* outModule)
{
    f_file f = OpenFile(filename, RDONLY);
    
    i32 FileSize = FileLength(&f);
    
    void* data = mmap(NULL, FileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, f.Fd, 0);
    SetFileData(&f, data);
    i32 r_len = FileRead(&f);
    
    if( r_len == -1 ) {
        fprintf( stderr, "[ERROR] Could not read file %s\n", filename);
        fprintf( stderr, "       %s\n", strerror(errno) );
        return false;
    }
    
    VkShaderModuleCreateInfo CreateInfo = {0};
	CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	CreateInfo.pNext = 0;
	CreateInfo.codeSize = FileSize;
	CreateInfo.pCode    = data;
    
	VK_CHECK(vkCreateShaderModule(device, &CreateInfo, 0, outModule));
    
    munmap(data, FileSize);
    CloseFile(&f);
    
	return true;
}

vk_pipeline AddPipeline(vulkan_base* base, pipeline_builder* builder, const char* vert_path, const char* frag_path) {
    VkShaderModule triangleVertShader = VK_NULL_HANDLE;
    if (!LoadShaderModule(vert_path, base->Device, &triangleVertShader)) {
        fprintf(stderr, "Failed to load vertex shader: %s\n", vert_path);
        exit(EXIT_FAILURE);
    }
    
    VkShaderModule triangleFragShader = VK_NULL_HANDLE;
    if (!LoadShaderModule(frag_path, base->Device, &triangleFragShader)) {
        fprintf(stderr, "Failed to load fragment shader: %s\n", frag_path);
        vkDestroyShaderModule(base->Device, triangleVertShader, NULL);
        exit(EXIT_FAILURE);
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = builder->DescriptorLayout ? 1 : 0,
        .pSetLayouts = builder->DescriptorLayout,
        .pushConstantRangeCount = (uint32_t)builder->PushConstants.len,
        .pPushConstantRanges = builder->PushConstants.data
    };
    
    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(base->Device, &layout_info, NULL, &pipeline_layout));
    
    builder->PipelineLayout = pipeline_layout;
    SetShaders(builder, triangleVertShader, triangleFragShader);
    SetColorAttachmentFormat(builder, base->Swapchain.Format);
    
    VkPipeline b_pipeline = BuildPipeline(builder, base->Device);
    
    vk_pipeline pipeline = {0};
    pipeline.Pipeline = b_pipeline;
    pipeline.Layout = pipeline_layout;
    
    vkDestroyShaderModule(base->Device, triangleVertShader, NULL);
    vkDestroyShaderModule(base->Device, triangleFragShader, NULL);
    
    return pipeline;
}

internal void
InitDescriptorSet(vk_descriptor_set* ds, u32 max_descriptor_set_layout_binding, Stack_Allocator* Allocator) {
    ds->Allocator  = Allocator;
    ds->BackBuffer = stack_push(ds->Allocator, VkDescriptorSetLayoutBinding, max_descriptor_set_layout_binding);
    ds->bindings   = VectorNew(ds->BackBuffer, 0, max_descriptor_set_layout_binding, VkDescriptorSetLayoutBinding);
}

internal void
AddBindingDescriptorSet(vk_descriptor_set* ds, u32 binding, VkDescriptorType type) {
    VkDescriptorSetLayoutBinding new_bind = {0};
    new_bind.binding = binding;
    new_bind.descriptorCount = 1;
    new_bind.descriptorType = type;
    new_bind.stageFlags = 0;
    
    if( ds->bindings.data == NULL ) {
        fprintf(stderr, "[ERROR] Cannot bind, vector is not inited\n");
        //os.exit(1)
    }
    
    VectorAppend(&ds->bindings, &new_bind);
}

internal void
ClearDescriptorSet(vk_descriptor_set* ds) {
    VectorClear(&ds->bindings);
}

internal VkDescriptorSetLayout
BuildDescriptorSet(vk_descriptor_set* ds, VkDevice device, VkShaderStageFlags shader_stages, void* p_next, VkDescriptorSetLayoutCreateFlags flags) {
    for( u32 idx = 0; idx < ds->bindings.len; idx += 1 ) {
        VkDescriptorSetLayoutBinding* binding = VectorGet(&ds->bindings, idx);
        binding->stageFlags += shader_stages;
    }
    
    VkDescriptorSetLayoutCreateInfo Info = {0};
    Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    Info.pNext = p_next;
    Info.pBindings = ds->bindings.data;
    Info.bindingCount = ds->bindings.len;
    Info.flags = flags;
    
    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &Info, 0, &set));
    
    return set;
}

internal VkDescriptorSet
DescriptorSetAllocate(VkDescriptorPool* Pool, VkDevice device, VkDescriptorSetLayout* layout) {
    VkDescriptorSetAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = 0;
    alloc_info.descriptorPool = *Pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = layout;
    
    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &ds));
    
    return ds;
}

internal descriptor_writer
DescriptorWriterInit(u32 capacity, Stack_Allocator* Allocator) {
    descriptor_writer Writer = {0};
    
    Writer.Allocator = Allocator;
    Writer.ImageInfosBuffer  = stack_push(Allocator, VkDescriptorImageInfo, capacity);
    Writer.BufferInfosBuffer = stack_push(Allocator, VkDescriptorBufferInfo, capacity);
    Writer.WritesBuffer      = stack_push(Allocator, VkWriteDescriptorSet, capacity);
    
    Writer.ImageInfos  = QueueInit(Writer.ImageInfosBuffer,  0, capacity, sizeof(VkDescriptorImageInfo));
    Writer.BufferInfos = QueueInit(Writer.BufferInfosBuffer, 0, capacity, sizeof(VkDescriptorBufferInfo));
    Writer.Writes      = VectorNew(Writer.WritesBuffer, 0, capacity, VkWriteDescriptorSet);
    
    return Writer;
}

internal void
DescriptorWriterClear(descriptor_writer* dw) {
    VectorClear(&dw->Writes);
    QueueClear(&dw->BufferInfos);
    QueueClear(&dw->ImageInfos);
}

internal void
WriteImage(descriptor_writer* dw, i32 binding, VkImageView Image, VkSampler Sample, VkImageLayout Layout, VkDescriptorType Type) {
    VkDescriptorImageInfo Info = {0};
    Info.sampler = Sample;
    Info.imageView = Image;
    Info.imageLayout = Layout;
    
    QueuePush(&dw->ImageInfos, &Info);
    
    VkDescriptorImageInfo* WriteInfo = QueueGetFront(&dw->ImageInfos, VkDescriptorImageInfo);
    
    VkWriteDescriptorSet Write = {0};
    Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    Write.dstBinding = binding;
    Write.descriptorCount = 1;
    Write.descriptorType  = Type;
    Write.pImageInfo      = WriteInfo;
    
    VectorAppend(&dw->Writes, &Write);
}

internal void
WriteBuffer(descriptor_writer* dw, i32 Binding, VkBuffer Buffer, i32 Size, i32 Offset, VkDescriptorType Type) {
    VkDescriptorBufferInfo BufferInfo = {0};
    BufferInfo.buffer = Buffer;
    BufferInfo.offset = Offset;
    BufferInfo.range  = Size;
    
    QueuePush(&dw->BufferInfos, &BufferInfo);
    VkDescriptorBufferInfo* Info = QueueGetFront(&dw->BufferInfos, VkDescriptorBufferInfo);
    
    VkWriteDescriptorSet Write = {0};
    Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    Write.dstBinding = Binding;
    Write.descriptorCount = 1;
    Write.descriptorType  = Type;
    Write.pBufferInfo     = Info;
    
    VectorAppend(&dw->Writes, &Write);
}

internal void
UpdateDescriptorSet(descriptor_writer* dw, VkDevice Device, VkDescriptorSet Set) {
    for( u32 i = 0; i < dw->Writes.len; i += 1 ) {
        VkWriteDescriptorSet* Write = VectorGet(&dw->Writes, i);
        Write->dstSet = Set;
    }
    
    vkUpdateDescriptorSets( Device, dw->Writes.len, dw->Writes.data, 0, 0 );
}

internal VkImageViewCreateInfo
ImageViewCreateInfo(VkFormat Format, VkImage image, VkImageAspectFlags flags) {
    VkImageViewCreateInfo info = {0};
    info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext    = 0;
    
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.image    = image;
    info.format   = Format;
    
    info.subresourceRange.baseMipLevel   = 0;
    info.subresourceRange.levelCount     = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount     = 1;
    info.subresourceRange.aspectMask     = flags;
    
    return info;
}

internal vk_image
CreateImageDefault(vulkan_base* base, VkExtent3D Size, VkFormat Format, VkImageUsageFlags Usage, bool is_mipmapped)
{
    vk_image new_image = {0};
    new_image.Format = Format;
    new_image.Extent = Size;
    VkImageCreateInfo ImgInfo = ImageCreateInfo(Format, Usage, Size);
    
    if( is_mipmapped ) {
        ImgInfo.mipLevels = (u32)(floor(log2(Max(Size.width, Size.height))));
    }
    
    VmaAllocationCreateInfo AllocInfo = {0};
    AllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    AllocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    
    VK_CHECK(vmaCreateImage(base->GPUAllocator, &ImgInfo, &AllocInfo, &new_image.Image, &new_image.Alloc, 0));
    
    VkImageAspectFlags AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    
    if( Format == VK_FORMAT_D32_SFLOAT ) {
        AspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    
    VkImageViewCreateInfo ViewInfo = ImageViewCreateInfo(Format, new_image.Image, AspectFlags);
    ViewInfo.subresourceRange.levelCount = ImgInfo.mipLevels;
    
    VK_CHECK(vkCreateImageView(base->Device, &ViewInfo, 0, &new_image.ImageView));
    
    return new_image;
}

/**
 * @author Sascha Paez
 * @todo   Maybe it should be good to directly pass as an argument the data_size, we do not assume that is
 *         related to an rgba format, thus we multiply data_size * 4;
 */
internal vk_image
CreateImageData(vulkan_base* base, void* data, VkExtent3D Size, VkFormat Format, VkImageUsageFlags Usage, bool is_mipmapped)
{
    size_t data_size = Size.depth * Size.width * Size.height * sizeof(u8);
    
	allocated_buffer upload_buffer = CreateBuffer(
                                                  base->GPUAllocator,
                                                  data_size,
                                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                  VMA_MEMORY_USAGE_CPU_ONLY
                                                  );
    
	memcpy(upload_buffer.Info.pMappedData, data, data_size);
    
	vk_image new_image = CreateImageDefault(base, Size, Format, Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT, is_mipmapped);
    
	VkCommandBuffer cmd = ImmediateSubmitBegin(base);
	{
		TransitionImageDefault(
                               cmd,
                               new_image.Image,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                               );
        
		VkBufferImageCopy copyRegion = {0};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource = (VkImageSubresourceLayers){
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        copyRegion.imageExtent = Size;
        
		vkCmdCopyBufferToImage(
                               cmd,
                               upload_buffer.Buffer,
                               new_image.Image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &copyRegion
                               );
        
		TransitionImageDefault(
                               cmd,
                               new_image.Image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                               );
	}
	ImmediateSubmitEnd(base, cmd);
    
	DestroyBuffer(&base->GPUAllocator, &upload_buffer);
    
	new_image.Width = Size.width;
	new_image.Height = Size.height;
    
	return new_image;
}

internal VkCommandBufferBeginInfo
CommandBufferBeginInfo ( VkCommandBufferUsageFlags flags )
{
	VkCommandBufferBeginInfo info = (VkCommandBufferBeginInfo){
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = 0,
		.pInheritanceInfo = 0,
		.flags = flags,
	};
    
	return info;
}

internal VkCommandBuffer
ImmediateSubmitBegin(vulkan_base* base) {
    VK_CHECK(vkResetFences(base->Device, 1, &base->ImmFence));
    VK_CHECK(vkResetCommandBuffer(base->ImmCommandBuffer, 0));
    
    VkCommandBuffer cmd = base->ImmCommandBuffer;
    
    VkCommandBufferBeginInfo CmdBeginInfo = CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    
    VK_CHECK(vkBeginCommandBuffer(cmd, &CmdBeginInfo));
    
    return cmd;
}

internal VkCommandBufferSubmitInfo
CommandBufferSubmitInfo(VkCommandBuffer cmd)
{
	VkCommandBufferSubmitInfo info = {0};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	info.pNext = 0;
	info.commandBuffer = cmd;
	info.deviceMask = 0;
    
	return info;
}

internal VkSubmitInfo2
SubmitInfo( VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo *waitSemaphoreInfo) {
    
    VkSubmitInfo2 info = {0};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    info.pNext = 0;
    
    info.waitSemaphoreInfoCount = waitSemaphoreInfo == NULL ? 0 : 1;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;
    
    info.signalSemaphoreInfoCount = signalSemaphoreInfo == NULL ? 0 : 1;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;
    
    info.commandBufferInfoCount = 1;
    info.pCommandBufferInfos = cmd;
    
    return info;
}

internal void
ImmediateSubmitEnd(vulkan_base* base, VkCommandBuffer cmd) {
    VK_CHECK(vkEndCommandBuffer(cmd));
    VkCommandBufferSubmitInfo CmdInfo = CommandBufferSubmitInfo(cmd);
    VkSubmitInfo2 Submit = SubmitInfo(&CmdInfo, 0, 0);
    
    VK_CHECK(vkQueueSubmit2(base->GraphicsQueue, 1, &Submit, base->ImmFence));
	VK_CHECK(vkWaitForFences(base->Device, 1, &base->ImmFence, true, 9999999999));
    
}

internal
void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout,
                     VkImageLayout newLayout, VkPipelineStageFlags2 srcStageMask,
                     VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask,
                     VkAccessFlags2 dstAccessMask)
{
    VkImageAspectFlags aspectMask;
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    
    VkImageMemoryBarrier2 imageBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = NULL,
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = currentLayout,
        .newLayout = newLayout,
        .image = image,
        .subresourceRange = {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS
        }
    };
    
    VkDependencyInfo depInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = NULL,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageBarrier
    };
    
    vkCmdPipelineBarrier2(cmd, &depInfo);
}

void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination,
                      VkExtent2D srcSize, VkExtent2D dstSize)
{
    VkImageBlit2 blitRegion = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = NULL,
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .srcOffsets = {
            {0, 0, 0},
            {(int32_t)srcSize.width, (int32_t)srcSize.height, 1}
        },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .dstOffsets = {
            {0, 0, 0},
            {(int32_t)dstSize.width, (int32_t)dstSize.height, 1}
        }
    };
    
    VkBlitImageInfo2 blitInfo = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = NULL,
        .srcImage = source,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = destination,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &blitRegion,
        .filter = VK_FILTER_LINEAR
    };
    
    vkCmdBlitImage2(cmd, &blitInfo);
}

internal void
RecreateSwapchain(vulkan_base* base) {
    vkDeviceWaitIdle(base->Device);
    
    vkDestroySwapchainKHR(base->Device, base->Swapchain.Swapchain, 0);
    for( u32 i = 0; i < base->Swapchain.N_ImageViews; i += 1 ) {
        vkDestroyImageView(base->Device, base->Swapchain.ImageViews[i], 0);
    }
    
    U32 width = 0, height = 0, snum = 0;
    snum   = DefaultScreen(base->Window.Dpy);
    width  = DisplayWidth(base->Window.Dpy, snum);
    height = DisplayHeight(base->Window.Dpy, snum);
    
    base->Window.Width = width;
    base->Window.Height = height;
    
    CreateSwapchain(base);
    CreateImageViews(base);
    
    ClearDescriptorPool(&base->GlobalDescriptorAllocator, base->Device);
    //InitDescriptors(base);
    
    base->FramebufferResized = false;
}

internal bool
PrepareFrame(vulkan_base* base) {
    u32 FrameIdx = base->CurrentFrame;
    
    VK_CHECK(vkWaitForFences(base->Device, 1, &base->Semaphores.InFlight[FrameIdx], true, 1000000000));
	VK_CHECK(vkResetFences(base->Device, 1, &base->Semaphores.InFlight[FrameIdx]));
    
    VkResult result = vkAcquireNextImageKHR(
                                            base->Device,
                                            base->Swapchain.Swapchain,
                                            1000000000,
                                            base->Semaphores.ImageAvailable[FrameIdx],
                                            0,
                                            &base->SwapchainImageIdx
                                            );
    
	if( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// Resize swapchain
			RecreateSwapchain(base);
			return true;
		}
	} else {
		VK_CHECK(result);
	}
	return false;
}

internal VkCommandBuffer
BeginRender(vulkan_base* base)
{
    u32 FrameIdx = base->CurrentFrame;
    
	VkCommandBufferBeginInfo cmdBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = 0,
		.pInheritanceInfo = 0,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
    
	VkCommandBuffer cmd = base->CommandBuffers[FrameIdx];
    
	VK_CHECK(vkResetCommandBuffer(cmd, 0));
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
    
	return cmd;
}

internal VkSemaphoreSubmitInfo
SemaphoreSubmitInfo( VkPipelineStageFlags2 stageMask, VkSemaphore semaphore) {
    VkSemaphoreSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    submitInfo.pNext = 0;
    submitInfo.semaphore = semaphore;
    submitInfo.stageMask = stageMask;
    submitInfo.deviceIndex = 0;
    submitInfo.value = 1;
    
    return submitInfo;
}

internal bool
EndRender( vulkan_base* base, VkCommandBuffer cmd ) {
    
	bool ResizeSwapchain = false;
	u32 FrameIdx = base->CurrentFrame;
	u32 SwapchainImageIdx = base->SwapchainImageIdx;
    
	VK_CHECK(vkEndCommandBuffer(cmd));
    
	VkCommandBufferSubmitInfo cmdInfo = CommandBufferSubmitInfo( cmd );
    
	VkSemaphoreSubmitInfo waitInfo   = SemaphoreSubmitInfo(
                                                           VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                           base->Semaphores.ImageAvailable[FrameIdx]
                                                           );
	VkSemaphoreSubmitInfo signalInfo = SemaphoreSubmitInfo(
                                                           VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                                           base->Semaphores.RenderFinished[SwapchainImageIdx]
                                                           );
    
	VkSubmitInfo2 submit = SubmitInfo(&cmdInfo, &signalInfo, &waitInfo);
    
	VK_CHECK(vkQueueSubmit2(
                            base->GraphicsQueue,
                            1,
                            &submit,
                            base->Semaphores.InFlight[FrameIdx])
             );
    
	// prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that,
	// as its necessary that drawing commands have finished before the image is
	// displayed to the user
	//
	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = 0,
		.pSwapchains = &base->Swapchain.Swapchain,
		.swapchainCount = 1,
		.pWaitSemaphores = &base->Semaphores.RenderFinished[SwapchainImageIdx],
		.waitSemaphoreCount = 1,
		.pImageIndices = &base->SwapchainImageIdx
	};
    
	VkResult result = vkQueuePresentKHR(base->PresentationQueue, &presentInfo);
    
	if( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || base->FramebufferResized) {
		base->FramebufferResized = false;
		// Resize swapchain
		ResizeSwapchain = true;
		RecreateSwapchain(base);
        
		if( result == VK_ERROR_OUT_OF_DATE_KHR ) {
			return ResizeSwapchain;
		}
	} else {
		VK_CHECK(result);
	}
    
	base->CurrentFrame = (base->CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    
	return ResizeSwapchain;
}

internal VkRenderingAttachmentInfo
AttachmentInfo( VkImageView View, VkClearValue *Clear, VkImageLayout Layout ) {
    VkRenderingAttachmentInfo colorAttachment = {0};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.pNext = 0;
    
    colorAttachment.imageView = View;
    colorAttachment.imageLayout = Layout;
    colorAttachment.loadOp = Clear != NULL ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    if (Clear != NULL)  {
        colorAttachment.clearValue = *Clear;
    }
    
    return colorAttachment;
}

// ------------------------------------------------------------------

internal VkRenderingInfo
RenderingInfo(VkExtent2D Extent, VkRenderingAttachmentInfo *ColorInfo, VkRenderingAttachmentInfo* DepthInfo)  {
    VkRenderingInfo renderInfo = {0};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = 0;
    
    renderInfo.renderArea = (VkRect2D){(VkOffset2D){0, 0}, Extent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = ColorInfo;
    renderInfo.pDepthAttachment = DepthInfo;
    renderInfo.pStencilAttachment = NULL;
    
    return renderInfo;
}

#endif
