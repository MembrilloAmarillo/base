#ifndef _RENDER_H_
#define _RENDER_H_

/**
	* @file render.h
	* @author Sascha Paez
	* @version 0.0.1
	* @brief An abstraction layer for the Vulkan graphics API.
	* @note This is an alpha version. The main goal is to simplify Vulkan's boilerplate
	* and provide a more intuitive, high-level rendering API.
	*/

// --- Core Types and Definitions ---

/**
	* @brief An opaque handle representing a rendering resource.
	* @details This is used to reference resources like buffers, textures, and pipelines
	* without exposing the underlying Vulkan implementation details.
	*/
typedef u64 R_Handle;

/** @brief A special value indicating an invalid or uninitialized resource handle. */
#define R_HANDLE_INVALID 0

/** @brief The maximum number of resource bindings that can be queued for a single draw call. */
#define MAX_PENDING_BINDINGS 16

// --- Vertex and Pipeline Structures ---

/**
	* @brief Enumerates the possible data formats for a vertex attribute.
	*/
typedef enum r_format {
    R_FORMAT_FLOAT,  /**< A single 32-bit float. */
    R_FORMAT_VEC2,   /**< A 2-component vector of 32-bit floats. */
    R_FORMAT_VEC3,   /**< A 3-component vector of 32-bit floats. */
    R_FORMAT_VEC4,   /**< A 4-component vector of 32-bit floats. */
    R_FORMAT_UNORM8, /**< A 4-component vector of 8-bit unsigned normalized integers (0-255 mapped to 0.0-1.0). */
    R_FORMAT_SRGB    /**< A 4-component sRGB color format. */
} r_format;

typedef enum r_buffer_type {
	R_BUFFER_TYPE_VERTEX,
	R_BUFFER_TYPE_INDEX,
	R_BUFFER_TYPE_UNIFORM,
	R_BUFFER_TYPE_STAGING
} r_buffer_type;

/**
	* @brief Describes a single attribute within a vertex structure.
	*/
typedef struct r_vertex_attribute {
    u32      Location; /**< The binding location in the vertex shader (e.g., `layout(location = 0)`). */
    r_format Format;   /**< The data format of the attribute. */
    u32      Offset;   /**< The byte offset of this attribute within the vertex struct. Use `offsetof()`. */
} r_vertex_attribute;

/**
	* @brief Provides a complete description of a vertex's memory layout.
	*/
typedef struct r_vertex_input_description {
    r_vertex_attribute* Attributes;     /**< An array of attribute descriptions. */
    u32                 AttributeCount; /**< The number of attributes in the array. */
    u32                 Stride;         /**< The total size, in bytes, of a single vertex struct. Use `sizeof()`. */
	VkVertexInputRate   Rate;
} r_vertex_input_description;

// --- Renderer State and Management ---

/**
	* @brief Holds information about a resource binding requested for an upcoming draw call.
	* @note This is an fn_internal structure used by the renderer to queue state changes.
	*/
typedef struct pending_binding {
    R_Handle         Handle;       /**< The handle of the resource to bind. */
    u32              BindingPoint; /**< The binding point in the shader (e.g., `layout(binding = 0)`). */
    VkDescriptorType Type;         /**< The Vulkan descriptor type (e.g., UBO, SSBO). */
	u64              Range;
} pending_binding;

typedef struct handler_list handler_list;
struct handler_list {
	R_Handle Handle;
	handler_list* Prev;
	handler_list* Next;
};

/**
	* @brief The main renderer context structure.
	* @details This structure encapsulates all the necessary state for the rendering abstraction layer,
	* including the Vulkan backend, resource managers, and frame-specific data.
	*/
typedef struct r_render r_render;
struct r_render {
    Stack_Allocator* Allocator;         /**< A general-purpose allocator for persistent resources. */
    Stack_Allocator  PerFrameAllocator; /**< An allocator that is reset at the beginning of each frame. */

    vulkan_base* VulkanBase;        /**< A pointer to the low-level Vulkan backend context. */
    VkDescriptorPool DescriptorPool;    /**< The global descriptor pool used for allocating descriptor sets. */

    hash_table  Textures;          /**< A hash table mapping R_Handles to fn_internal texture resources. */
    hash_table  Buffers;           /**< A hash table mapping R_Handles to fn_internal buffer resources. */
	hash_table  Pipelines;

    R_Handle         CurrentPipeline;      /**< The handle of the graphics pipeline currently bound for rendering. */
	R_Handle         CurrentVertexBuffer;
	R_Handle         CurrentIndexBuffer;
	pending_binding  PendingBindings[MAX_PENDING_BINDINGS]; /**< An array of resource bindings to be applied before the next draw call. */
    u32              PendingBindingCount;  /**< The number of currently pending resource bindings. */

    VkCommandBuffer  CurrentCommandBuffer; /**< The primary command buffer for the current frame in flight. */

	bool SetScreenToClear;
	VkClearValue ClearColorScreen;

	bool SetExternalResize;
};

// --- Lifecycle and Frame Management ---

/**
	* @brief Initializes the rendering context.
	* @param Render A pointer to the r_render structure to initialize.
	* @param base A pointer to an already initialized vulkan_base backend.
	* @param Allocator A pointer to a memory allocator for the renderer's use.
	*/
fn_internal void R_RenderInit(r_render* Render, vulkan_base* base, Stack_Allocator* Allocator);

/**
	* @brief Begins a new rendering frame.
	* @details This must be called before any drawing or binding commands. It handles synchronization
	* with the GPU and prepares command buffers for recording.
	* @param Render A pointer to the initialized renderer context.
	*/
fn_internal void R_Begin(r_render* Render);

fn_internal void R_ClearScreen(r_render* Render, vec4 Color);

fn_internal void R_BeginRenderPass(r_render* Render);

fn_internal void R_RenderPassEnd(r_render* Render);
/**
	* @brief Ends the current rendering frame.
	* @details This submits all recorded commands to the GPU for execution and presents the
	* final image to the screen.
	* @param Render A pointer to the initialized renderer context.
	*/
fn_internal void R_RenderEnd(r_render* Render);

// --- Resource Creation and Management ---

fn_internal VkDescriptorSetLayout R_CreateDescriptorSetLayout(
	r_render* Render, 
	u32 NSets, 
	VkDescriptorType* Types, 
	VkShaderStageFlagBits StageFlags
);

/**
	* @brief Creates a new graphics pipeline.
	* @param Render A pointer to the renderer context.
	* @param VertPath The file path to the compiled SPIR-V vertex shader.
	* @param FragPath The file path to the compiled SPIR-V fragment shader.
	* @param Description A pointer to a description of the vertex layout required by this pipeline.
	* @param DescriptorSetLayout The layout expected on the pipeline
	* @return An `R_Handle` to the newly created pipeline. Returns `R_HANDLE_INVALID` on failure.
	*/
fn_internal R_Handle R_CreatePipelineEx(
	r_render* Render, 
	const char* Id,
	const char* VertPath, 
	const char* FragPath, 
	r_vertex_input_description* Description,
	VkPrimitiveTopology Topology,
	VkDescriptorSetLayout* DescriptorSetLayout, 
	u32 LayoutCount
);

#define R_CreatePipeline(Render, Id, VertPath, FragPath, Description, DescriptorLayout, LayoutCount) \
R_CreatePipelineEx(Render, Id, VertPath, FragPath, Description, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, DescriptorLayout, LayoutCount)

#define R_CreatePipelineLine(Render, Id, VertPath, FragPath, Description, DescriptorLayout, LayoutCount) \
R_CreatePipelineEx(Render, Id, VertPath, FragPath, Description, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, DescriptorLayout, LayoutCount)

#define R_CreatePipelinePoint(Render, Id, VertPath, FragPath, Description, DescriptorLayout, LayoutCount) \
R_CreatePipelineEx(Render, Id, VertPath, FragPath, Description, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, DescriptorLayout, LayoutCount)


fn_internal R_Handle R_CreateComputePipeline(
	r_render* Render, 
	const char* Id,
	const char* ComputeShaderPath,
	VkDescriptorSetLayout* DescriptorSetLayout, 
	u32 LayoutCount
);

/**
	* @brief Creates a new GPU buffer.
	* @param Render A pointer to the renderer context.
	* @param Size The total size of the buffer in bytes.
	* @param Type The intended usage of the buffer (e.g., vertex, index, uniform).
	* @return An `R_Handle` to the newly created buffer.
	*/
fn_internal R_Handle R_CreateBuffer(r_render* Render, const char* BufferId, u64 Size, r_buffer_type Type);

/**
	* @brief Destroys a previously created buffer and releases its resources.
	* @param Render A pointer to the renderer context.
	* @param Handle The handle of the buffer to destroy.
	*/
fn_internal void R_DestroyBuffer(r_render* Render, const char* Id);

// --- State Binding and Drawing Commands ---

/**
	* @brief Updates a uniform buffer with new data and binds it to a shader location.
	* @param Render A pointer to the renderer context.
	* @param BufferHandle The handle of the uniform buffer to update.
	* @param Binding The binding point in the shader.
	* @param Data A pointer to the raw data to be copied into the buffer.
	* @param DataSize The size of the data to copy.
	*/
fn_internal void R_UpdateUniformBuffer(r_render* Render, const char* Id, u32 Binding, void* Data, size_t DataSize);

/**
	* @brief Binds a storage buffer (SSBO) to a shader location for read-only access.
	* @param Render A pointer to the renderer context.
	* @param BufferHandle The handle of the storage buffer to bind.
	* @param Binding The binding point in the shader.
	*/
fn_internal void R_BindStorageBuffer(r_render* Render, R_Handle BufferHandle, u32 Binding);

/**
	* @brief Pushes a new texture into the render graph
	* @param Render  A pointer to the renderer context.
	* @param Id      The id of the texture to bind.
	* @param Image   A pointer to the image texture.
*/
fn_internal R_Handle R_PushTexture(r_render* Render, const char* Id, vk_image* Image);

/**
	* @brief Binds a texture and its sampler to a shader location.
	* @param Render A pointer to the renderer context.
	* @param TextureHandle The handle of the texture to bind.
	* @param Binding The binding point in the shader.
	*/
fn_internal void R_BindTexture(r_render* Render, const char* Id, u32 Binding, VkDescriptorType Type );

fn_internal void R_BindVertexBuffer(r_render* Render, R_Handle Handle);

fn_internal void R_BindIndexBuffer(r_render* Render, R_Handle Handle);

fn_internal void R_SetPipeline(r_render* Render, R_Handle Handle);

/**
	* @brief Records a non-indexed drawing command.
	* @param Render A pointer to the renderer context.
	* @param VertexCount The number of vertices to draw.
	* @param InstanceCount The number of instances to draw.
	*/
void R_Draw(r_render* Render, u32 VertexCount, u32 InstanceCount);

/**
	* @brief Records an indexed drawing command.
	* @param Render A pointer to the renderer context.
	* @param IndexCount The number of indices to draw.
	* @param InstanceCount The number of instances to draw.
	*/
fn_internal void R_DrawIndexed(r_render* Render, u32 IndexCount, u32 InstanceCount);

fn_internal void R_SendDataToBuffer(r_render* Render, R_Handle Buffer, void* Data, u64 Size, u64 offset);

fn_internal void R_CopyStageToBuffer(r_render* Render, R_Handle StageBuffer, R_Handle Buffer, VkBufferCopy Copy);

// --- Internal Helper Functions ---

/**
	* @fn_internal
	* @brief Converts an `r_format` enum to its corresponding `VkFormat`.
	* @param format The `r_format` to convert.
	* @return The equivalent `VkFormat`.
	*/
fn_internal VkFormat R_FormatToVkFormat(r_format format);

/**
* @brief Despacha un pipeline de cómputo.
* @param Render El contexto del renderer.
	* @param PipelineHandle El handle del pipeline de cómputo a usar.
	* @param GroupCountX El número de grupos de trabajo a despachar en X.
	* @param GroupCountY El número de grupos de trabajo a despachar en Y.
	* @param GroupCountZ El número de grupos de trabajo a despachar en Z.
	*/
fn_internal void R_DispatchCompute(r_render* Render, R_Handle PipelineHandle, u32 GroupCountX, u32 GroupCountY, u32 GroupCountZ);

/**
* @brief Inserta una barrera de memoria para sincronizar cómputo y gráfico.
* @details Asegura que las escrituras del compute shader (Compute)
	* sean visibles para las lecturas del vertex/fragment shader (Graphics).
	*/
fn_internal void R_AddComputeToGraphicsBarrier(r_render* Render);

fn_internal void R_AddVertexCopyToBarrier(r_render* Render);

fn_internal void R_SendImageToSwapchain(r_render* Render, R_Handle Image);

fn_internal void R_SendCopyToGpu(r_render* Render);

#endif // _RENDER_H_