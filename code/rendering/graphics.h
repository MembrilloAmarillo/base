#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDL_Window SDL_Window;
typedef struct Graphics_Device Graphics_Device_t;
typedef struct Graphics_Swapchain Graphics_Swapchain_t;
typedef struct Graphics_Command Graphics_Command_t;
typedef struct Gpu_Binding_Layout Gpu_Binding_Layout_t;
typedef struct Gpu_Upload_Batch Gpu_Upload_Batch_t;

/** @defgroup graphics Graphics
 *  @brief Bounded, single-graphics-queue Vulkan 1.3 rendering API.
 *
 *  All calls for a device run on one thread. Initialize output pointers to NULL
 *  and owning value objects to zero. Do not copy owning objects. Creation is
 *  transactional: failure leaves the output unchanged. Destroy resources after
 *  their final GPU use, then destroy the device. References must stay alive from
 *  recording/staging until completion or cancellation. Native handles are borrowed.
 *  The caller owns SDL initialization and windows, including their lifetime.
 *  Vulkan structures/flags supplied by callers must satisfy Vulkan valid usage
 *  and the enabled device features. Errors distinguish invalid arguments,
 *  recording state, range, fixed capacity, allocation failure and device loss.
 *  @{
 */

#define GRAPHICS_MAX_COLOR_ATTACHMENTS 8U
#define GRAPHICS_MAX_BINDINGS 64U
#define GRAPHICS_MAX_SET_LAYOUTS 8U
#define GRAPHICS_MAX_PUSH_RANGES 2U
#define GRAPHICS_MAX_DESCRIPTOR_WRITES 64U
#define GRAPHICS_MAX_DESCRIPTOR_VALUES 128U
#define GRAPHICS_MAX_UPLOADS 128U

/** @brief API status; capacity exhaustion is distinct from allocation failure. */
typedef enum {
    Graphics_NONE,
    Graphics_ERROR_INVALID_ARGUMENT,
    Graphics_ERROR_STATE,
    Graphics_ERROR_RESOURCE,
    Graphics_NO_DEVICE,
    Graphics_NO_MEMORY,
    Graphics_NO_DEVICE_MEM,
    Graphics_NO_MAPPEABLE,
    Graphics_OUT_OF_RANGE,
    Graphics_CAPACITY,
    Graphics_UNSUPPORTED,
    Graphics_LAYER_NON_AVAILABLE,
    Graphics_EXTENSION_NON_AVAILABLE,
    Graphics_NO_WINDOW,
    Graphics_NO_SURFACE,
    Graphics_NO_SWAPCHAIN,
    Graphics_SWAPCHAIN_OUT_OF_DATE,
    Graphics_NO_COMMAND_BUFFER,
    Graphics_ERROR_QUEUE_SUBMIT,
    Graphics_ERROR_PRESENT,
    Graphics_ERROR_FENCE,
    Graphics_DEVICE_LOST,
    Graphics_BUSY
} Graphics_Result;

/** @brief Device creation settings. Zero initializes a headless, validation-free device. */
typedef struct {
    SDL_Window *window; /**< Optional borrowed window used to select presentation support. */
    bool validation;
    bool synchronization_validation;                     /**< Requires validation=true. */
    PFN_vkDebugUtilsMessengerCallbackEXT debug_callback; /**< NULL selects stderr logging. */
    void *debug_user;
    bool buffer_device_address;
    bool scalar_block_layout;
    bool sampler_anisotropy;
} Graphics_Device_Desc_t;

/** @brief Diagnostic counters, including algorithmic work and retained memory. */
typedef struct {
    uint64_t memory_allocations;
    uint64_t allocation_search_steps;
    uint64_t descriptor_pool_creations;
    uint64_t descriptor_layout_creations;
    uint64_t descriptor_update_calls;
    uint64_t upload_submissions;
    uint64_t uploaded_bytes;
    uint64_t upload_barrier_calls;
    uint64_t pipeline_binds;
    uint64_t dynamic_state_commands;
    uint64_t validation_errors;
    VkDeviceSize reserved_bytes;
} Graphics_Stats_t;

/** @brief Memory placement intent. Host-visible allocations are coherent. */
typedef enum { GPU_MEMORY_UPLOAD, GPU_MEMORY_DEVICE, GPU_MEMORY_READBACK } Gpu_Alloc_Type_t;

/** @brief Buffer creation requirements. */
typedef struct {
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    Gpu_Alloc_Type_t memory;
} Gpu_Buffer_Desc_t;

/** @brief Owning buffer. Fields are read-only; mapped bytes may be written after synchronization.
 */
typedef struct {
    VkBuffer buffer;
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    void *mapped;
    VkDeviceAddress address;
    VkDevice owner;
    uint32_t allocation; /**< Private allocator token. */
} Gpu_Buffer_t;

/** @brief Single-layer 2D image; mip_levels=0 means one mip. Format support is queried on the
 * device. */
typedef struct {
    uint32_t width;
    uint32_t height;
    VkFormat format;
    VkImageUsageFlags usage;
    uint32_t mip_levels;
} Gpu_Texture_Desc_t;

/** @brief Image and full-range view. Samplers are independent objects.
 *  layout is the last successfully submitted whole-image layout. Keep the
 *  object's address stable through submission. Record and submit commands that
 *  touch an image before recording another command context that touches it.
 */
typedef struct {
    VkImage image;
    VkImageView view;
    VkFormat format;
    VkExtent3D extent;
    VkImageAspectFlags aspects;
    uint32_t mip_levels;
    VkImageLayout layout;
    VkImageUsageFlags usage;
    VkDevice owner;
    uint32_t allocation; /**< Zero for borrowed swapchain images. */
} Gpu_Texture_t;

/** @brief Independently owned sampler, shareable among image bindings. */
typedef struct {
    VkSampler sampler;
    VkDevice owner;
} Gpu_Sampler_t;

/** @brief Explicit layout binding number, type, visibility and array length. */
typedef struct {
    uint32_t binding;
    VkDescriptorType type;
    VkShaderStageFlags stages;
    uint32_t count;
} Gpu_Binding_Desc_t;

/** @brief Lightweight descriptor set referencing a shared layout. */
typedef struct {
    VkDescriptorSet set;
    VkDevice owner;
    Gpu_Binding_Layout_t *layout;
    uint32_t pool_index; /**< Private pool index. */
} Gpu_Binding_Set_t;

/** @brief Non-owning descriptor value; use buffer fields or image/sampler fields according to the
 * layout. */
typedef struct {
    const Gpu_Buffer_t *buffer;
    VkDeviceSize offset;
    VkDeviceSize size;
    const Gpu_Texture_t *texture;
    const Gpu_Sampler_t *sampler;
    VkImageLayout image_layout; /**< Expected layout at GPU use, not at descriptor update time. */
} Gpu_Binding_Value_t;

/** @brief One contiguous descriptor-array write. The batch is validated before any write is
 * applied. */
typedef struct {
    Gpu_Binding_Set_t *set;
    uint32_t binding;
    uint32_t first_element;
    uint32_t count;
    const Gpu_Binding_Value_t *values;
} Gpu_Binding_Write_t;

/** @brief Pipeline attachment format and blend configuration. */
typedef struct {
    VkFormat format;
    VkPipelineColorBlendAttachmentState blend;
} Gpu_Color_Target_t;

/** @brief Graphics pipeline description. NULL optional state uses defaults inside creation.
 *  SPIR-V lengths are word counts. Shader entry points default to "main".
 *  Topology defaults to triangle list; a NULL color array with count=0 is a
 *  depth-only pipeline. Vertex input is optional (vertex pulling is supported).
 */
typedef struct {
    const uint32_t *vertex_spirv;
    size_t vertex_words;
    const uint32_t *fragment_spirv;
    size_t fragment_words;
    const char *vertex_entry;
    const char *fragment_entry;
    const Gpu_Color_Target_t *colors;
    uint32_t color_count;
    VkFormat depth_format;
    VkFormat stencil_format;
    const VkPipelineRasterizationStateCreateInfo *rasterization;
    const VkPipelineVertexInputStateCreateInfo *vertex_input;
    const VkPrimitiveTopology *topology;
    Gpu_Binding_Layout_t *const *layouts;
    uint32_t layout_count;
    const VkPushConstantRange *push_ranges;
    uint32_t push_range_count;
} Gpu_Pipeline_Desc_t;

/** @brief Owning pipeline and immutable validation metadata. */
typedef struct {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDevice owner;
    Gpu_Binding_Layout_t *layouts[GRAPHICS_MAX_SET_LAYOUTS];
    uint32_t layout_count;
    VkPushConstantRange push_ranges[GRAPHICS_MAX_PUSH_RANGES];
    uint32_t push_range_count;
    VkFormat colors[GRAPHICS_MAX_COLOR_ATTACHMENTS];
    uint32_t color_count;
    VkFormat depth_format;
    VkFormat stencil_format;
} Gpu_Pipeline_t;

/** @brief Rendering attachment. Full image view; rendering is restricted to mip 0. */
typedef struct {
    Gpu_Texture_t *texture;
    VkAttachmentLoadOp load;
    VkAttachmentStoreOp store;
    VkClearValue clear;
} Graphics_Attachment_t;

/** @brief Explicit dynamic-rendering scope. Depth and stencil may reference the same image. */
typedef struct {
    VkRect2D area;
    uint32_t color_count;
    const Graphics_Attachment_t *colors;
    const Graphics_Attachment_t *depth;
    const Graphics_Attachment_t *stencil;
} Graphics_Render_Desc_t;

/** @brief Tightly packed upload to one mip rectangle. Zero extent selects the complete mip.
 *  final_layout must be SHADER_READ_ONLY_OPTIMAL or GENERAL and be consistent
 *  across uploads of one image in a batch. Consumer masks must match later use.
 */
typedef struct {
    uint32_t mip_level;
    VkOffset2D offset;
    VkExtent2D extent;
    VkImageLayout final_layout;
    VkPipelineStageFlags2 consumer_stages;
    VkAccessFlags2 consumer_access;
} Gpu_Texture_Upload_t;

/** @brief Describe a result with a static string; unknown values return "unknown". */
const char *graphics_result_string(Graphics_Result result);
/** @brief Create a device. desc and out are required; *out must be NULL. Failure cleans up
 * completely. */
Graphics_Result graphics_init(const Graphics_Device_Desc_t *desc, Graphics_Device_t **out);
/** @brief Destroy an idle device after all owned objects. NULL *device is accepted; live objects
 * return ERROR_RESOURCE. */
Graphics_Result graphics_shutdown(Graphics_Device_t **device);
/** @brief Wait for the graphics queue/device. Returns DEVICE_LOST distinctly. */
Graphics_Result graphics_wait_idle(Graphics_Device_t *device);
/** @brief Borrow the logical device for native Vulkan interoperability; NULL yields VK_NULL_HANDLE.
 */
VkDevice graphics_native_device(const Graphics_Device_t *device);
/** @brief Borrow cached physical-device properties; NULL device yields NULL. */
const VkPhysicalDeviceProperties *graphics_device_properties(const Graphics_Device_t *device);
/** @brief Snapshot diagnostic counters; NULL device yields zero counters. */
Graphics_Stats_t graphics_stats(const Graphics_Device_t *device);
/** @brief Last Vulkan failure recorded on this device, or VK_SUCCESS. */
VkResult graphics_last_vk_result(const Graphics_Device_t *device);

/** @brief Create presentation resources for a borrowed SDL Vulkan window; the device must support
 * presentation. */
Graphics_Result graphics_create_swapchain(Graphics_Device_t *device,
                                          SDL_Window *window,
                                          Graphics_Swapchain_t **out);
/** @brief Wait and destroy presentation resources. An active frame must first be ended or
 * cancelled. */
Graphics_Result graphics_destroy_swapchain(Graphics_Swapchain_t **swapchain);
/** @brief Rebuild presentation resources. Returns OUT_OF_DATE while minimized; retry after
 * restoring the window. */
Graphics_Result graphics_update_swapchain(Graphics_Swapchain_t *swapchain);
/** @brief Query negotiated surface format; NULL yields UNDEFINED. */
VkFormat graphics_swapchain_format(const Graphics_Swapchain_t *swapchain);
/** @brief Query current extent; NULL yields zero extent. */
VkExtent2D graphics_swapchain_extent(const Graphics_Swapchain_t *swapchain);
/** @brief Borrow the acquired color target during an active frame. Never destroy this texture. */
Gpu_Texture_t *graphics_swapchain_target(Graphics_Swapchain_t *swapchain);
/** @brief Acquire and begin a frame. Rejects a second start; *out must be NULL. */
Graphics_Result graphics_start_frame(Graphics_Swapchain_t *swapchain, Graphics_Command_t **out);
/** @brief Submit/present a frame after closing all rendering scopes. Command pointers remain
 * borrowed. */
Graphics_Result graphics_end_frame(Graphics_Swapchain_t *swapchain);
/** @brief Discard recorded frame work, then release the acquired image through an empty
 * presentation. */
Graphics_Result graphics_cancel_frame(Graphics_Swapchain_t *swapchain);

/** @brief Create a reusable independent command context; *out must be NULL. */
Graphics_Result graphics_create_command(Graphics_Device_t *device, Graphics_Command_t **out);
/** @brief Begin recording after completion/cancellation. Resets dynamic defaults: tests off, writes
 * on, LESS, ALWAYS/KEEP. */
Graphics_Result graphics_begin_command(Graphics_Command_t *command);
/** @brief Submit recording asynchronously. Rendering must be closed. Commits recorded image layouts
 * on success. */
Graphics_Result graphics_submit_command(Graphics_Command_t *command);
/** @brief Wait for a submitted command. No pending work is a successful no-op. */
Graphics_Result graphics_wait_command(Graphics_Command_t *command);
/** @brief Nonblocking completion query. Returns BUSY while submitted work is pending. */
Graphics_Result graphics_poll_command(Graphics_Command_t *command);
/** @brief Discard unsubmitted recording and its layout changes. Pending work returns BUSY. */
Graphics_Result graphics_cancel_command(Graphics_Command_t *command);
/** @brief Wait and destroy an independently created context; frame contexts cannot be destroyed
 * here. */
Graphics_Result graphics_destroy_command(Graphics_Command_t **command);
/** @brief Borrow the recording Vulkan command buffer. Native callers must preserve render scopes
 * and resource lifetimes. */
VkCommandBuffer graphics_native_command(const Graphics_Command_t *command);
/** @brief Invalidate native-modified pipeline/descriptor/index/dynamic state; rebind and set state
 * before further draws. */
void graphics_invalidate_state(Graphics_Command_t *command);
/** @brief Transition the whole image outside rendering. Changes are committed only on successful
 * submission. */
Graphics_Result graphics_transition_texture(Graphics_Command_t *command,
                                            Gpu_Texture_t *texture,
                                            VkImageLayout layout,
                                            VkPipelineStageFlags2 stages,
                                            VkAccessFlags2 access);
/** @brief Record independent-offset buffer copy outside rendering, with destination-scoped
 * synchronization. */
Graphics_Result graphics_copy_buffer(Graphics_Command_t *command,
                                     const Gpu_Buffer_t *src,
                                     VkDeviceSize src_offset,
                                     const Gpu_Buffer_t *dst,
                                     VkDeviceSize dst_offset,
                                     VkDeviceSize size);

/** @brief Begin rendering and transition its attachments. Sets viewport/scissor to the render area.
 */
Graphics_Result graphics_begin_render(Graphics_Command_t *command,
                                      const Graphics_Render_Desc_t *desc);
/** @brief Close the active rendering scope. Invalid call order returns ERROR_STATE. */
Graphics_Result graphics_end_render(Graphics_Command_t *command);
/** @brief Set one explicit viewport and scissor, validated against device limits. */
Graphics_Result graphics_set_viewport(Graphics_Command_t *command,
                                      const VkViewport *viewport,
                                      const VkRect2D *scissor);
/** @brief Bind a pipeline from the same device. Repeated bindings are cached; dynamic state is
 * preserved. */
Graphics_Result graphics_bind_pipeline(Graphics_Command_t *command, const Gpu_Pipeline_t *pipeline);
/** @brief Bind a set matching the pipeline's layout at set_index. */
Graphics_Result graphics_bind_resources(Graphics_Command_t *command,
                                        uint32_t set_index,
                                        const Gpu_Binding_Set_t *set);
/** @brief Push aligned bytes inside the bound pipeline's declared range for every requested stage.
 */
Graphics_Result graphics_push_constants(Graphics_Command_t *command,
                                        VkShaderStageFlags stages,
                                        uint32_t offset,
                                        uint32_t size,
                                        const void *data);
/** @brief Toggle depth testing while recording. Writes/compare are unchanged. */
Graphics_Result graphics_set_depth_test_enabled(Graphics_Command_t *command, bool enabled);
/** @brief Configure depth writes and compare operation. Defaults are true and LESS. */
Graphics_Result
graphics_set_depth_state(Graphics_Command_t *command, bool write, VkCompareOp compare);
/** @brief Toggle stencil testing while recording. Default is false. */
Graphics_Result graphics_set_stencil_test_enabled(Graphics_Command_t *command, bool enabled);
/** @brief Set front/back stencil operations, masks and reference. Default is ALWAYS/KEEP, full
 * masks, reference zero. */
Graphics_Result graphics_set_stencil_state(Graphics_Command_t *command,
                                           const VkStencilOpState *front,
                                           const VkStencilOpState *back);
/** @brief Bind one vertex buffer for conventional vertex input. */
Graphics_Result graphics_bind_vertex_buffer(Graphics_Command_t *command,
                                            uint32_t binding,
                                            const Gpu_Buffer_t *buffer,
                                            VkDeviceSize offset);
/** @brief Bind a UINT16 or UINT32 index buffer at an aligned offset. */
Graphics_Result graphics_bind_index_buffer(Graphics_Command_t *command,
                                           const Gpu_Buffer_t *buffer,
                                           VkDeviceSize offset,
                                           VkIndexType type);
/** @brief Draw inside rendering with a compatible pipeline and bound resources. */
Graphics_Result graphics_draw(Graphics_Command_t *command,
                              uint32_t vertices,
                              uint32_t instances,
                              uint32_t first_vertex,
                              uint32_t first_instance);
/** @brief Indexed draw; validates the requested index range against the bound buffer. */
Graphics_Result graphics_draw_indexed(Graphics_Command_t *command,
                                      uint32_t indices,
                                      uint32_t instances,
                                      uint32_t first_index,
                                      int32_t vertex_offset,
                                      uint32_t first_instance);

/** @brief Return an opaque, all-channel-write color target for the supplied format. */
Gpu_Color_Target_t graphics_color_target_default(VkFormat format);
/** @brief Create a conventional graphics pipeline; desc is borrowed only during this call. */
Graphics_Result graphics_create_pipeline(Graphics_Device_t *device,
                                         const Gpu_Pipeline_Desc_t *desc,
                                         Gpu_Pipeline_t *out);
/** @brief Destroy after final GPU use and release references to binding layouts. */
Graphics_Result graphics_destroy_pipeline(Graphics_Device_t *device, Gpu_Pipeline_t *pipeline);
/** @brief Create a shared immutable layout from explicit binding numbers. *out must be NULL. */
Graphics_Result graphics_create_binding_layout(Graphics_Device_t *device,
                                               const Gpu_Binding_Desc_t *bindings,
                                               uint32_t count,
                                               Gpu_Binding_Layout_t **out);
/** @brief Destroy an unreferenced layout; live sets/pipelines return ERROR_RESOURCE. */
Graphics_Result graphics_destroy_binding_layout(Graphics_Device_t *device,
                                                Gpu_Binding_Layout_t **layout);
/** @brief Allocate a lightweight descriptor set from a shared pool. */
Graphics_Result graphics_create_resources(Graphics_Device_t *device,
                                          Gpu_Binding_Layout_t *layout,
                                          Gpu_Binding_Set_t *out);
/** @brief Free a set after final GPU use. Does not destroy its shared layout or referenced
 * resources. */
Graphics_Result graphics_destroy_resources(Graphics_Device_t *device, Gpu_Binding_Set_t *set);
/** @brief Apply up to 64 binding writes/128 values in one Vulkan update. Synchronize with GPU use
 * first. */
Graphics_Result graphics_update_resources(Graphics_Device_t *device,
                                          const Gpu_Binding_Write_t *writes,
                                          uint32_t count);
/** @brief Create a buffer with explicit usage; GPU capacity exhaustion returns CAPACITY. */
Graphics_Result
graphics_create_buffer(Graphics_Device_t *device, const Gpu_Buffer_Desc_t *desc, Gpu_Buffer_t *out);
/** @brief Destroy a buffer after final GPU use. A zero object is accepted. */
Graphics_Result graphics_destroy_buffer(Graphics_Device_t *device, Gpu_Buffer_t *buffer);
/** @brief Copy CPU bytes into coherent mapped storage; caller synchronizes prior GPU access. */
Graphics_Result graphics_write_buffer(Graphics_Device_t *device,
                                      Gpu_Buffer_t *buffer,
                                      VkDeviceSize offset,
                                      const void *data,
                                      size_t size);
/** @brief Create a device-supported 2D image, including floating-point and depth/stencil formats.
 */
Graphics_Result graphics_create_texture(Graphics_Device_t *device,
                                        const Gpu_Texture_Desc_t *desc,
                                        Gpu_Texture_t *out);
/** @brief Destroy an owned image after final GPU use. Borrowed swapchain targets are rejected. */
Graphics_Result graphics_destroy_texture(Graphics_Device_t *device, Gpu_Texture_t *texture);
/** @brief Create a sampler. NULL desc selects linear/clamp with anisotropy disabled; desc->pNext
 * must be NULL. */
Graphics_Result graphics_create_sampler(Graphics_Device_t *device,
                                        const VkSamplerCreateInfo *desc,
                                        Gpu_Sampler_t *out);
/** @brief Destroy a sampler after final GPU use; does not affect textures. */
Graphics_Result graphics_destroy_sampler(Graphics_Device_t *device, Gpu_Sampler_t *sampler);

/** @brief Create reusable staging storage for up to GRAPHICS_MAX_UPLOADS records. */
Graphics_Result graphics_create_upload_batch(Graphics_Device_t *device,
                                             VkDeviceSize capacity,
                                             Gpu_Upload_Batch_t **out);
/** @brief Begin staging after prior completion. Returns BUSY for pending work. */
Graphics_Result graphics_begin_uploads(Gpu_Upload_Batch_t *batch);
/** @brief Copy source bytes now and queue a destination buffer update. Consumer masks describe
 * later GPU/host use. */
Graphics_Result graphics_upload_buffer(Gpu_Upload_Batch_t *batch,
                                       Gpu_Buffer_t *buffer,
                                       VkDeviceSize offset,
                                       const void *data,
                                       size_t size,
                                       VkPipelineStageFlags2 consumer_stages,
                                       VkAccessFlags2 consumer_access);
/** @brief Stage a color mip rectangle. NULL region selects mip 0, sampled-read final state. Failure
 * consumes no staging space. */
Graphics_Result graphics_upload_texture(Gpu_Upload_Batch_t *batch,
                                        Gpu_Texture_t *texture,
                                        const Gpu_Texture_Upload_t *region,
                                        const void *data,
                                        size_t size);
/** @brief Record grouped resource barriers and copies, then submit asynchronously on the graphics
 * queue. */
Graphics_Result graphics_submit_uploads(Gpu_Upload_Batch_t *batch);
/** @brief Wait for upload completion before reusing staging or destroying destinations. */
Graphics_Result graphics_wait_uploads(Gpu_Upload_Batch_t *batch);
/** @brief Nonblocking completion query; rotate multiple batches when this returns BUSY. */
Graphics_Result graphics_poll_uploads(Gpu_Upload_Batch_t *batch);
/** @brief Discard staged uploads. Pending work returns BUSY. Image layouts remain unchanged. */
Graphics_Result graphics_cancel_uploads(Gpu_Upload_Batch_t *batch);
/** @brief Wait for completion and destroy batch storage. NULL *batch is accepted. */
Graphics_Result graphics_destroy_upload_batch(Gpu_Upload_Batch_t **batch);
/** @brief Blocking complete-mip-0 upload using temporary staging. Prefer reusable batches for
 * repeated uploads. */
Graphics_Result graphics_upload_texture_wait(Graphics_Device_t *device,
                                             Gpu_Texture_t *texture,
                                             const void *data,
                                             size_t size);

/** @} */
#ifdef __cplusplus
}
#endif
#endif
