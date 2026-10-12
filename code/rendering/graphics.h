#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMALL_VECTOR_CAPACITY 64

typedef struct {
	uint8_t   *data;
	size_t      len;
	size_t capacity;
} Small_Vector_t;

static inline Small_Vector_t _small_vector_alloc(uint8_t *data, size_t capacity) {
	Small_Vector_t vector = {data, 0, capacity};
	return vector;
}

static inline void _small_vector_push(Small_Vector_t *vector, uint8_t* data, size_t size) {
	if(vector->len >= vector->capacity) {
		assert(false && "Vector already full");
	}
	memcpy(vector->data + vector->len, data, size);
	vector->len += 1;
}

#define small_vector_push(vec, data, type) _small_vector_push(vec, data, sizeof(type))
#define small_vector_alloc(data, cap, type) _small_vector_alloc(data, cap * sizeof(type))

typedef enum {
  Graphics_NONE,
  Graphics_NO_DEVICE,
  Graphics_NO_MEMORY,
  Graphics_LAYER_NON_AVAILABLE,
  Graphics_EXTENSION_NON_AVAILABLE,
  Graphics_NO_LIBRARY,
  Graphics_NO_PRESENTATION_FAMILY,
  Graphics_NO_WINDOW,
  Graphics_NO_SURFACE,
  Graphics_NO_CAPABILITIES,
  Graphics_NO_SWAPCHAIN,
  Graphics_NO_DEPTH_FORMAT,
  Graphics_NO_DEVICE_MEM,
  Graphics_NO_IMAGE_VIEW,
  Graphics_NO_BUFFER,
  Graphics_NO_SEMAPHORE,
  Graphics_NO_FENCE,
  Graphics_NO_COMMAND_BUFFER,
  Graphics_ERROR_FENCE,
  Graphics_ERROR_QUEUE_SUBMIT,
  Graphics_ERROR_PRESENT,
  Graphics_SWAPCHAIN_OUT_OF_DATE,
  Graphics_NO_SAMPLER,
  Graphics_ERROR_INVALID_ARGUMENT,
  Graphics_ERROR_RESOURCE,
  Graphics_NO_MAPPEABLE,
  Graphics_OUT_OF_RANGE,
} Graphics_Result;

#define MAX_FRAMES_IN_FLIGHT 2
#define MAX_COLOR_ATTACHMENTS 8
#define SWAPCHAIN_FORMAT VK_FORMAT_B8G8R8A8_SRGB
#define DEPTH_FORMAT     VK_FORMAT_D32_SFLOAT_S8_UINT

typedef enum {
	FORMAT_R8_SRGB               = VK_FORMAT_R8_SRGB,
	FORMAT_RG8_SRGB              = VK_FORMAT_R8G8_SRGB,
	FORMAT_RGBA8_SRGB            = VK_FORMAT_R8G8B8A8_SRGB,
	FORMAT_BGRA8_SRGB            = VK_FORMAT_B8G8R8A8_SRGB,

	FORMAT_RGBA4_UNORM           = VK_FORMAT_R4G4B4A4_UNORM_PACK16,
	FORMAT_R5G5B5A1_UNORM        = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
	FORMAT_R5G6B5_UNORM          = VK_FORMAT_R5G6B5_UNORM_PACK16,

	FORMAT_R8_UNORM              = VK_FORMAT_R8_UNORM,
	FORMAT_RG8_UNORM             = VK_FORMAT_R8G8_UNORM,
	FORMAT_RGBA8_UNORM           = VK_FORMAT_R8G8B8A8_UNORM,
	FORMAT_BGRA8_UNORM           = VK_FORMAT_B8G8R8A8_UNORM,
	FORMAT_R16_UNORM             = VK_FORMAT_R16_UNORM,
	FORMAT_RG16_UNORM            = VK_FORMAT_R16G16_UNORM,
	FORMAT_RGBA16_UNORM          = VK_FORMAT_R16G16B16A16_UNORM,

	FORMAT_R8_UINT               = VK_FORMAT_R8_UINT,
	FORMAT_RG8_UINT              = VK_FORMAT_R8G8_UINT,
	FORMAT_RGBA8_UINT            = VK_FORMAT_R8G8B8A8_UINT,
	FORMAT_BGRA8_UINT            = VK_FORMAT_B8G8R8A8_UINT,
	FORMAT_R16_UINT              = VK_FORMAT_R16_UINT,
	FORMAT_RG16_UINT             = VK_FORMAT_R16G16_UINT,
	FORMAT_RGBA16_UINT           = VK_FORMAT_R16G16B16A16_UINT,
	FORMAT_R32_UINT              = VK_FORMAT_R32_UINT,
	FORMAT_RG32_UINT             = VK_FORMAT_R32G32_UINT,
	FORMAT_RGB32_UINT            = VK_FORMAT_R32G32B32_UINT,
	FORMAT_RGBA32_UINT           = VK_FORMAT_R32G32B32A32_UINT,

	FORMAT_R16_FLOAT             = VK_FORMAT_R16_SFLOAT,
	FORMAT_RG16_FLOAT            = VK_FORMAT_R16G16_SFLOAT,
	FORMAT_RGBA16_FLOAT          = VK_FORMAT_R16G16B16A16_SFLOAT,
	FORMAT_R32_FLOAT             = VK_FORMAT_R32_SFLOAT,
	FORMAT_RG32_FLOAT            = VK_FORMAT_R32G32_SFLOAT,
	FORMAT_RGB32_FLOAT           = VK_FORMAT_R32G32B32_SFLOAT,
	FORMAT_RGBA32_FLOAT          = VK_FORMAT_R32G32B32A32_SFLOAT,

	FORMAT_RGB10A2_UNORM         = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
	FORMAT_RG11B10_FLOAT         = VK_FORMAT_B10G11R11_UFLOAT_PACK32,

	FORMAT_D16_UNORM             = VK_FORMAT_D16_UNORM,
	FORMAT_D24_UNORM_S8_UINT     = VK_FORMAT_D24_UNORM_S8_UINT,
	FORMAT_D32_FLOAT             = VK_FORMAT_D32_SFLOAT,
	FORMAT_S8_UINT               = VK_FORMAT_S8_UINT,
	FORMAT_D32_FLOAT_S8_UINT     = VK_FORMAT_D32_SFLOAT_S8_UINT,

	FORMAT_EAC_RG                = VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
	FORMAT_ASTC_4X4_SRGB         = VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
	FORMAT_ASTC_4X4_UNORM        = VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
	FORMAT_BC3_SRGB              = VK_FORMAT_BC3_SRGB_BLOCK,
	FORMAT_BC3_UNORM             = VK_FORMAT_BC3_UNORM_BLOCK,
	FORMAT_BC5_RG                = VK_FORMAT_BC5_UNORM_BLOCK,
	FORMAT_BC7_SRGB              = VK_FORMAT_BC7_SRGB_BLOCK,
	FORMAT_BC7_UNORM             = VK_FORMAT_BC7_UNORM_BLOCK,

	FORMAT_UNDEFINED             = VK_FORMAT_UNDEFINED,
} Format_t;

// --------------------------------------------------------------- //

typedef enum {
	CULL_MODE_NONE              = VK_CULL_MODE_NONE,
	CULL_MODE_CLOCKWISE         = VK_CULL_MODE_FRONT_BIT,
	CULL_MODE_COUNTER_CLOCKWISE = VK_CULL_MODE_BACK_BIT,
} Cull_Mode_t;

// --------------------------------------------------------------- //

typedef enum {
	BLEND_FACTOR_ZERO                        = VK_BLEND_FACTOR_ZERO,
	BLEND_FACTOR_ONE                         = VK_BLEND_FACTOR_ONE,
	BLEND_FACTOR_SOURCE_COLOR                = VK_BLEND_FACTOR_SRC_COLOR,
	BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR      = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
	BLEND_FACTOR_DESTINATION_COLOR           = VK_BLEND_FACTOR_DST_COLOR,
	BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
	BLEND_FACTOR_SOURCE_ALPHA                = VK_BLEND_FACTOR_SRC_ALPHA,
	BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA      = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	BLEND_FACTOR_DESTINATION_ALPHA           = VK_BLEND_FACTOR_DST_ALPHA,
	BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
	BLEND_FACTOR_SOURCE_ALPHA_SATURATE       = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
} Blend_Factor_t;

// --------------------------------------------------------------- //

typedef enum {
	BLEND_OP_ADD              = VK_BLEND_OP_ADD,
	BLEND_OP_SUBTRACT         = VK_BLEND_OP_SUBTRACT,
	BLEND_OP_REVERSE_SUBTRACT = VK_BLEND_OP_REVERSE_SUBTRACT,
	BLEND_OP_MINIMUM          = VK_BLEND_OP_MIN,
	BLEND_OP_MAXIMUM          = VK_BLEND_OP_MAX,
} Blend_Op_t;

// --------------------------------------------------------------- //

typedef struct {
	Blend_Factor_t source;
	Blend_Factor_t destination;
	Blend_Op_t operation;
} Blend_Component_State_t;

// --------------------------------------------------------------- //

typedef struct {
	bool enabled;
	Blend_Component_State_t color;
	Blend_Component_State_t alpha;
} Blend_State_t;

// --------------------------------------------------------------- //

typedef struct {
	Format_t format;
	Blend_State_t blend;
	uint8_t write_mask;
} Color_Target_Desc_t;

Color_Target_Desc_t graphics_color_target_desc_default();

// --------------------------------------------------------------- //

typedef struct {
	Cull_Mode_t cull;
	float depth_bias_constant;
	float depth_bias_clamp;
	float depth_bias_slope;
} Rasterization_State_t;

Rasterization_State_t graphics_rasterization_state_default();

typedef struct {
	VkDevice device;
	VkPhysicalDevice physical_device;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkSurfaceKHR surface;

	uint32_t queue_family_count;
	uint32_t queue_family;

	SDL_Window *window;

	VkSwapchainKHR swapchain;
	VkExtent2D swapchain_extent;
	VkImage swapchain_images[SMALL_VECTOR_CAPACITY];
	VkImageView swapchain_img_views[SMALL_VECTOR_CAPACITY];
	uint32_t swapchain_image_count;

	VkImage depth_image;
	VkImageView depth_img_view;
	VkDeviceMemory depth_img_mem;
	VkFormat depth_format;

	VkFence fences[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore image_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore render_complete_semaphores[SMALL_VECTOR_CAPACITY];

	VkCommandPool command_pool;
	VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];

	uint32_t frame_index;
	uint32_t image_index;

	// Loaded Functions
	//
	PFN_vkCmdPushDataEXT vkCmdPushDataEXT_fn;
	// Optional conventional layout for devices without descriptor heaps.
	VkPipelineLayout pipeline_layout;
} Graphics_Device_t;

typedef struct {
	VkBuffer buffer;
	VkDeviceSize size;
	VkMemoryPropertyFlags type;
	VkBufferUsageFlags usage;
	VkDeviceMemory memory;
    void* mapped;
    VkDeviceAddress address;
} Gpu_Buffer_t;

typedef struct {
	VkDescriptorPool pool;
	VkDescriptorSetLayout layout;
	VkDescriptorSet set;
} Gpu_Resource_t;

typedef struct {
	VkImage        image;
	VkImageView    view;
	VkDeviceMemory memory;

	VkFormat format;
    VkExtent3D extent;
    VkImageAspectFlags barrier_aspects;
    uint32_t mip_levels;
    uint32_t array_layers;
    VkImageLayout layout;
} Gpu_Texture_t;

typedef struct {
	size_t offset;
	size_t size;
} Gpu_Range_t;

typedef struct {
	VkPipeline       pipeline;
	VkPipelineLayout layout;
	Gpu_Resource_t  *resource;
} Gpu_Pipeline_t;

typedef enum {
	CPU_VISIBLE,
	GPU_ONLY,
	CACHED, // For VK_MEMORY_PROPERTY_HOST_CACHED_BIT
} Gpu_Alloc_Type_t;

typedef struct {
	// SPIR-V sizes are counts of uint32_t words, not bytes.
	const uint32_t              *first_stage_spirv;
	size_t                       first_stage_spirv_size;
	const uint32_t              *fragment_spirv;
	size_t                       fragment_spirv_size;
	const Color_Target_Desc_t   *color_targets;          /**< If null, defaults */
	size_t                       color_target_count;
	VkFormat                     depth_format;
	VkFormat                     stencil_format;
	const Rasterization_State_t *rasterization_state;    /**< If null, defaults */
	const char                  *first_stage_entry;      /**< if null, defaults to "main" */
	const char                  *fragment_entry;         /**< if null, defaults to "main" */
	bool                         mesh;
	bool                         set_layout;
	// if set_layout, this is mandatory
	Gpu_Resource_t              *resources;
	size_t                       n_resources;
	Gpu_Range_t                 *ranges;   /**< This can be optional, is for push constants**/
	size_t                       n_ranges;
} Gpu_PSO_Desc_t;

extern Color_Target_Desc_t g_default_color_target[1];

extern Rasterization_State_t g_default_rasterization_state;

Gpu_PSO_Desc_t _graphics_pso_desc(Gpu_PSO_Desc_t desc);
    
#ifdef __cplusplus
#define graphics_pso_desc(...) _graphics_pso_desc(Gpu_PSO_Desc_t{ __VA_ARGS__ })
#else
#define graphics_pso_desc(...) _graphics_pso_desc((Gpu_PSO_Desc_t){ __VA_ARGS__ })
#endif

static inline VkDeviceAddress graphics_get_buffer_address(Gpu_Buffer_t* buffer) {return buffer->address;}

Graphics_Result graphics_get_win_size(Graphics_Device_t *device, int *w_out, int *h_out);

Graphics_Result graphics_load_functions();
Graphics_Result graphics_init(Graphics_Device_t *tDevice);

Graphics_Result graphics_create_pso(
	Graphics_Device_t *device,
	Gpu_Pipeline_t *pso_out,
	Gpu_PSO_Desc_t *desc
);

Graphics_Result graphics_draw(VkCommandBuffer cmd, uint32_t n_vertices, uint32_t n_instances);
Graphics_Result graphics_bind_pso(Graphics_Device_t *device, VkPipeline pipeline);
Graphics_Result graphics_bind_pso_layout(Graphics_Device_t *device, VkPipeline pipeline, VkPipelineLayout layout);
Graphics_Result graphics_bind_compute();

Graphics_Result graphics_alloc(Graphics_Device_t *device, Gpu_Buffer_t *buffer, VkDeviceSize size, Gpu_Alloc_Type_t alloc_type);
Graphics_Result graphics_memcpy(Graphics_Device_t *device, Gpu_Buffer_t *buffer_dst, void *data, Gpu_Range_t range);
Graphics_Result graphics_copy(Graphics_Device_t *device, Gpu_Buffer_t *buffer_dst, Gpu_Buffer_t *buffer_src, Gpu_Range_t range, VkCommandBuffer cmd);
Graphics_Result graphics_push_constant(Graphics_Device_t *device, const void *data, size_t size);
Graphics_Result graphics_push_uniform_buffer();

Graphics_Result graphics_create_sampler(Graphics_Device_t *device, VkSampler *sampler_out);
Graphics_Result graphics_create_resources(Graphics_Device_t *device, uint32_t n_descriptors, Gpu_Resource_t *resource_out);
Graphics_Result graphics_update_resources(Graphics_Device_t *device, VkWriteDescriptorSet descriptor_set);

Gpu_Texture_t graphics_create_texture(Graphics_Device_t *device, int width, int height);
/* Upload a complete RGBA8_UINT texture created by graphics_create_texture().
 * data_size must be width * height * 4 bytes, with tightly packed rows.
 * Call outside frame recording, on the graphics thread. Waits for completion
 * and leaves the texture in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
 * Do not change the texture's layout externally between uploads. */
Graphics_Result graphics_texture_data(Graphics_Device_t *device, Gpu_Texture_t *texture, const void *data, size_t data_size);
VkImageCreateInfo graphics_img_create_info(VkFormat format, VkExtent3D extent, VkSampleCountFlags sample_count, VkImageTiling tiling, VkImageUsageFlags usage);
VkDeviceMemory graphics_img_alloc(Graphics_Device_t *tDevice, VkImage image);
VkImageView graphics_img_view_create(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect);

/*< Command buffers */
Graphics_Result graphics_create_commands(Graphics_Device_t *device, uint32_t queue_family, VkCommandPool *command_pool_out, VkCommandBuffer command_buffers_out[MAX_FRAMES_IN_FLIGHT]);
Graphics_Result graphics_single_time_command_buffer(Graphics_Device_t *tDevice, VkCommandPool command_pool, VkCommandBuffer *command_buffer_out);
Graphics_Result graphics_begin_command_buffer(VkCommandBuffer command_buffer);
Graphics_Result graphics_end_command_buffer(VkCommandBuffer command_buffer);

#define graphics_single_cmd(device, cmd_pool, cmd_buffer) \
	for(int _i_ = ((graphics_single_time_command_buffer(device, cmd_pool, &cmd_buffer)), (graphics_begin_command_buffer(cmd_buffer)), 0); !_i_; _i_ += 1, (graphics_end_command_buffer(cmd_buffer)))

/*< sync functions */
Graphics_Result graphics_new_semaphore(Graphics_Device_t *device, VkSemaphore *semaphore_out);
Graphics_Result graphics_new_fence(Graphics_Device_t *device, VkFence *fence_out);
void graphics_issue_image_barrier(
	VkCommandBuffer command_buffer,
	VkImage image,
	VkPipelineStageFlags2 src_stage,
	VkAccessFlags2 src_access,
	VkPipelineStageFlags2 dst_stage,
	VkAccessFlags2 dst_access,
	VkImageLayout old_layout,
	VkImageLayout new_layout
);
void graphics_issue_memory_barrier(VkCommandBuffer command_buffer, VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access);

Graphics_Result graphics_start_frame(Graphics_Device_t *device);
Graphics_Result graphics_end_frame(Graphics_Device_t *device);

Graphics_Result graphics_begin_render(Graphics_Device_t *device);
Graphics_Result graphics_end_render(Graphics_Device_t *device);
Graphics_Result graphics_push_viewport(Graphics_Device_t *device);

Graphics_Result graphics_update_swapchain(Graphics_Device_t *device);

#ifdef __cplusplus
}
#endif

#endif // _GRAPHICS_H_
