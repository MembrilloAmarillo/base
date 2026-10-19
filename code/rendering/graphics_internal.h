#ifndef GRAPHICS_INTERNAL_H
#define GRAPHICS_INTERNAL_H

#include "graphics.h"
#include <stdlib.h>
#include <string.h>

#define GRAPHICS_FRAMES 2U
#define GRAPHICS_MAX_SWAPCHAIN_IMAGES 16U
#define GRAPHICS_MAX_MEMORY_BLOCKS 32U
#define GRAPHICS_MAX_ALLOCATIONS 512U
#define GRAPHICS_MEMORY_BLOCK_SIZE (16U * 1024U * 1024U)
#define GRAPHICS_MAX_DESCRIPTOR_POOLS 16U
#define GRAPHICS_DESCRIPTOR_TYPES 6U
#define GRAPHICS_MAX_ENUMERATED 256U
#define GRAPHICS_MAX_DISCOVERY_PROPERTIES 4096U
#define GRAPHICS_MAX_TRACKED_IMAGES GRAPHICS_MAX_UPLOADS

typedef struct {
    VkDeviceMemory memory;
    VkDeviceSize size;
    void *mapped;
    uint32_t memory_type;
    uint32_t live_allocations;
    uint32_t first; /* offset-sorted allocation list, slot + 1 */
    bool image;
    bool dedicated;
} Graphics_Memory_Block_t;

typedef struct {
    VkDeviceSize offset;
    VkDeviceSize size;
    uint32_t block;
    uint32_t next;
} Graphics_Allocation_t;

typedef struct {
    VkDescriptorPool pool;
    uint32_t live_sets;
    uint32_t available[GRAPHICS_DESCRIPTOR_TYPES];
} Graphics_Descriptor_Pool_t;

struct Graphics_Device {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue queue;
    uint32_t queue_family;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory_properties;
    Graphics_Device_Desc_t enabled;
    bool presentation;
    bool lost;
    VkResult last_vk_result;
    VkPipelineCache pipeline_cache;
    uint32_t live_objects;
    uint32_t free_allocation;
    Graphics_Memory_Block_t memory_blocks[GRAPHICS_MAX_MEMORY_BLOCKS];
    Graphics_Allocation_t allocations[GRAPHICS_MAX_ALLOCATIONS];
    Graphics_Descriptor_Pool_t descriptor_pools[GRAPHICS_MAX_DESCRIPTOR_POOLS];
    Graphics_Stats_t stats;
};

struct Gpu_Binding_Layout {
    VkDevice owner;
    VkDescriptorSetLayout layout;
    uint32_t references;
    uint32_t count;
    Gpu_Binding_Desc_t bindings[GRAPHICS_MAX_BINDINGS];
    uint32_t budgets[GRAPHICS_DESCRIPTOR_TYPES];
};

typedef enum {
    GRAPHICS_COMMAND_IDLE,
    GRAPHICS_COMMAND_RECORDING,
    GRAPHICS_COMMAND_RENDERING,
    GRAPHICS_COMMAND_PENDING,
    GRAPHICS_COMMAND_FAILED
} Graphics_Command_Phase;

typedef struct {
    Gpu_Texture_t *texture;
    VkImageLayout layout;
} Graphics_Image_State_t;

struct Graphics_Command {
    Graphics_Device_t *device;
    VkCommandPool pool;
    VkCommandBuffer command;
    VkFence fence;
    Graphics_Command_Phase phase;
    bool frame_owned;
    bool state_valid;
    bool depth_enabled;
    bool stencil_enabled;
    const Gpu_Pipeline_t *pipeline;
    VkDescriptorSet sets[GRAPHICS_MAX_SET_LAYOUTS];
    VkFormat colors[GRAPHICS_MAX_COLOR_ATTACHMENTS];
    uint32_t color_count;
    VkFormat depth_format;
    VkFormat stencil_format;
    VkDeviceSize index_bytes;
    uint32_t index_stride;
    uint32_t image_count;
    Graphics_Image_State_t images[GRAPHICS_MAX_TRACKED_IMAGES];
};

typedef struct {
    VkSwapchainKHR handle;
    VkFormat format;
    VkExtent2D extent;
    uint32_t count;
    Gpu_Texture_t images[GRAPHICS_MAX_SWAPCHAIN_IMAGES];
    VkSemaphore complete[GRAPHICS_MAX_SWAPCHAIN_IMAGES];
} Graphics_Swapchain_Images_t;

struct Graphics_Swapchain {
    Graphics_Device_t *device;
    SDL_Window *window;
    VkSurfaceKHR surface;
    Graphics_Swapchain_Images_t images;
    Graphics_Command_t *frames[GRAPHICS_FRAMES];
    VkSemaphore acquired[GRAPHICS_FRAMES];
    bool acquire_pending[GRAPHICS_FRAMES];
    uint32_t frame;
    uint32_t image;
    bool active;
    bool needs_rebuild;
};

typedef struct {
    Gpu_Buffer_t *buffer;
    Gpu_Texture_t *texture;
    VkDeviceSize staging_offset;
    VkDeviceSize offset;
    size_t size;
    Gpu_Texture_Upload_t region;
    VkPipelineStageFlags2 stages;
    VkAccessFlags2 access;
} Graphics_Upload_Record_t;

struct Gpu_Upload_Batch {
    Graphics_Device_t *device;
    Gpu_Buffer_t staging;
    Graphics_Command_t *command;
    VkDeviceSize used;
    uint32_t count;
    bool recording;
    Graphics_Upload_Record_t records[GRAPHICS_MAX_UPLOADS];
    VkBufferMemoryBarrier2 buffer_barriers[GRAPHICS_MAX_UPLOADS];
    VkImageMemoryBarrier2 image_barriers[GRAPHICS_MAX_UPLOADS];
#ifdef GRAPHICS_BENCHMARK
    VkQueryPool profile_queries;
#endif
};

Graphics_Result
graphics_vk_result(Graphics_Device_t *device, VkResult result, Graphics_Result fallback);
void graphics_device_cleanup(Graphics_Device_t *device);
Graphics_Result
graphics_command_submit(Graphics_Command_t *command, VkSemaphore wait, VkSemaphore signal);
Graphics_Result graphics_command_begin(Graphics_Command_t *command);
Graphics_Result graphics_command_reset(Graphics_Command_t *command);
void graphics_command_defaults(Graphics_Command_t *command);
bool graphics_command_recording(const Graphics_Command_t *command);
VkImageLayout graphics_command_image_layout(const Graphics_Command_t *command,
                                            const Gpu_Texture_t *texture);
Graphics_Result
graphics_track_image(Graphics_Command_t *command, Gpu_Texture_t *texture, VkImageLayout layout);
void graphics_image_barrier(VkCommandBuffer command,
                            Gpu_Texture_t *texture,
                            VkImageLayout old_layout,
                            VkImageLayout new_layout,
                            VkPipelineStageFlags2 stages,
                            VkAccessFlags2 access);
Graphics_Result graphics_memory_acquire(Graphics_Device_t *device,
                                        const VkMemoryRequirements *requirements,
                                        Gpu_Alloc_Type_t memory,
                                        VkBuffer buffer,
                                        VkImage image,
                                        bool dedicated,
                                        uint32_t *allocation);
void graphics_memory_release(Graphics_Device_t *device, uint32_t allocation);
uint32_t graphics_format_bytes(VkFormat format);
VkImageAspectFlags graphics_format_aspects(VkFormat format);
uint32_t graphics_descriptor_type_index(VkDescriptorType type);
Graphics_Result graphics_check_draw(Graphics_Command_t *command);

#ifdef GRAPHICS_TESTING
typedef enum {
    GRAPHICS_FAIL_NONE,
    GRAPHICS_FAIL_INSTANCE,
    GRAPHICS_FAIL_DEVICE,
    GRAPHICS_FAIL_SWAPCHAIN,
    GRAPHICS_FAIL_VIEW,
    GRAPHICS_FAIL_BEGIN,
    GRAPHICS_FAIL_SUBMIT
} Graphics_Test_Fault;
void graphics_test_fault(Graphics_Test_Fault fault);
bool graphics_test_failure(Graphics_Test_Fault fault);
#endif

#endif
