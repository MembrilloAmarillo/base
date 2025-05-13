#ifndef VULKAN_IMPL_H
#define VULKAN_IMPL_H

#include <stdio.h>
#include <string.h>

// TODO: ERASE AND REPLACE
//
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// third party include
//
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_vulkan.h"
#include "imgui/imgui.h"

#include "../math/math.h"
#include "../memory/memory.h"
#include "../types/types.h"
#include "../vector/vector.h"

#define VK_CHECK(x)                                                            \
do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
        printf("Detected Vulkan error: %s", string_VkResult(err));               \
        abort();                                                                 \
    }                                                                          \
} while (0)

#define MAX_FRAMES_IN_FLIGHT 2

#define U32_MAX UINT32_MAX

class window {
    public:
    I32 Width;
    I32 Height;
    GLFWwindow *Window{nullptr};
    VkSurfaceKHR Surface;
    Vec2 ScalingFactor;
    bool FocusedWindow;
};

struct queue_family_indices {
    U32 GraphicsAndCompute;
    U32 Presentation;
};

struct swapchain_support_details {
    VkSurfaceCapabilitiesKHR Capabilities;
    VkSurfaceFormatKHR *Formats;
    VkPresentModeKHR *PresentModes;
    U32 FormatCount;
    U32 PresentModeCount;
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
    VkSwapchainKHR Swapchain;
    VkImage *Images{nullptr};
    U32 N_Images;
    VkImageView *ImageViews{nullptr};
    U32 N_ImageViews;
    VkFormat Format;
    VkExtent2D Extent;
    VkSurfaceCapabilitiesKHR Capabilities;
    VkSurfaceFormatKHR *SurfaceFormats{nullptr};
    VkPresentModeKHR *PresentModes{nullptr};
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
    VkDescriptorSet DescriptorSets;
    VkDescriptorSetLayout DescriptorSetLayout;

    vector<VkDescriptorSetLayoutBinding> bindings;

    void Init(Arena *vecArena, U32 MaxDescriptorSetLayoutBinding) {
        bindings.Init(vecArena, MaxDescriptorSetLayoutBinding);
    }

    void AddBinding(U32 binding, VkDescriptorType type) {
        VkDescriptorSetLayoutBinding newbind{};
        newbind.binding = binding;
        newbind.descriptorCount = 1;
        newbind.descriptorType = type;

        if (bindings.GetCapacity() == 0) {
            printf("[ERROR] Cannot bind, vector is not inited");
            exit(1);
        }

        bindings.PushBack(newbind);
    }

    void Clear() { bindings.Clear(); }

    VkDescriptorSetLayout Build(VkDevice Device, VkShaderStageFlags ShaderStages,
        void *pNext = nullptr,
        VkDescriptorSetLayoutCreateFlags Flags = 0) {
            for (U32 idx = 0; idx < bindings.GetLength(); idx += 1) {
                bindings.At(idx).stageFlags |= ShaderStages;
            }

            VkDescriptorSetLayoutCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            info.pNext = pNext;

            info.pBindings = bindings.GetData();
            info.bindingCount = bindings.GetLength();
            info.flags = Flags;

            VkDescriptorSetLayout set;
            VK_CHECK(vkCreateDescriptorSetLayout(Device, &info, nullptr, &set));

            return set;
        }
};

class vk_descriptor_allocator {
    public:
    struct pool_size_ratio {
        VkDescriptorType Type;
        F32 Ratio;
    };
    VkDescriptorPool Pool;

    void InitPool(Arena *arena, VkDevice Device, U32 MaxSets,
        vector<pool_size_ratio> &PoolRatios) {

            vector<VkDescriptorPoolSize> PoolSizes(arena, PoolRatios.GetCapacity());

            for (U32 it = 0; it < PoolRatios.GetLength(); it += 1) {
                pool_size_ratio &Ratio = PoolRatios.At(it);
                PoolSizes.PushBack(VkDescriptorPoolSize{
                    .type = Ratio.Type, .descriptorCount = U32(Ratio.Ratio * MaxSets)});
            }

            VkDescriptorPoolCreateInfo pool_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pool_info.flags = 0;
            pool_info.maxSets = MaxSets;
            pool_info.poolSizeCount = PoolSizes.GetCapacity();
            pool_info.pPoolSizes = PoolSizes.GetData();

            vkCreateDescriptorPool(Device, &pool_info, nullptr, &Pool);
        }
        void ClearDescriptors(VkDevice Device) {
            vkResetDescriptorPool(Device, Pool, 0);
        }
        void DestroyPool(VkDevice Device) {
            vkDestroyDescriptorPool(Device, Pool, nullptr);
        }

        VkDescriptorSet Allocate(VkDevice Device, VkDescriptorSetLayout Layout) {
            VkDescriptorSetAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocInfo.pNext = nullptr;
            allocInfo.descriptorPool = Pool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &Layout;

            VkDescriptorSet ds;
            VK_CHECK(vkAllocateDescriptorSets(Device, &allocInfo, &ds));

            return ds;
        }
};

class vk_semaphore {
    public:
    //VkSemaphore ImageAvailable[MAX_FRAMES_IN_FLIGHT];
    //VkSemaphore RenderFinished[MAX_FRAMES_IN_FLIGHT];
    //VkSemaphore ComputeFinished[MAX_FRAMES_IN_FLIGHT];
    //VkFence InFlight[MAX_FRAMES_IN_FLIGHT];
    //VkFence ComputerInFlight[MAX_FRAMES_IN_FLIGHT];
    vector<VkSemaphore> ImageAvailable;
    vector<VkSemaphore> RenderFinished;
    vector<VkSemaphore> ComputeFinished;
    vector<VkFence>     InFlight;
    vector<VkFence>     ComputeInFlight;
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
    F32 CornerRadius;
    F32 EdgeSoftness;
    F32 BorderThickness;
};

class vk_image {
    public:
    U32 Width;
    U32 Height;
    VkFormat Format;
    VkImageTiling Tiling;
    VkImageUsageFlags UsageFlags;
    VkMemoryPropertyFlags Properties;
    VkImage Image;
    VkDeviceMemory Memory;
    VmaAllocation Alloc;
    VkImageView ImageView;
    VkImageLayout Layout;
    VkSampler Sampler;
};

class vk_buffer {
    public:
    VkBuffer Buffer;
    VkDeviceMemory DeviceMemory;
    void *MappedMemory{nullptr};
    VmaAllocation Allocation;
    VmaAllocationInfo AllocationInfo;
    VkDeviceSize Size;
    VkDeviceSize MaxSize;
    VkMemoryPropertyFlags Properties;
    VkBufferUsageFlags UsageFlags;
    U32 InstanceCount;
    bool ToDestroy;
};

struct gpu_mesh_buffer {
    vk_buffer IndexBuffer;
    vk_buffer VertexBuffer;
    VkDeviceAddress VertexBufferAddress;
};

class vk_pipeline_builder {
    public:
    vector<VkPipelineShaderStageCreateInfo> ShaderStages;

    VkPipelineInputAssemblyStateCreateInfo InputAssembly;
    VkPipelineRasterizationStateCreateInfo Rasterizer;
    VkPipelineColorBlendAttachmentState ColorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo Multisampling;
    VkPipelineDepthStencilStateCreateInfo DepthStencil;
    VkPipelineRenderingCreateInfo RenderInfo;
    VkPipelineLayout PipelineLayout;
    VkFormat ColorAttachmentformat;

    vk_pipeline_builder(Arena *arena, U32 NShaderStages) {
        ShaderStages.Init(arena, NShaderStages);
        Clear();
    }

    void Clear() {
        // clear all of the structs we need back to 0 with their correct stype

        InputAssembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};

        Rasterizer = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};

        ColorBlendAttachment = {};

        Multisampling = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};

        PipelineLayout = {};

        DepthStencil = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

        RenderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};

        ShaderStages.Clear();
    }

    global VkPipelineShaderStageCreateInfo
    PipelineShaderStageCreateInfo(VkShaderStageFlagBits StageFlag,
        VkShaderModule Module, const char *entry) {

            VkPipelineShaderStageCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            info.pNext = nullptr;
            // shader stage
            info.stage = StageFlag;
            // module containing the code for this shader stage
            info.module = Module;
            // the entry point of the shader
            info.pName = entry;

            return info;
        }

        VkPipeline BuildPipeline(VkDevice Device);

        void SetInputTopology(VkPrimitiveTopology topology);

        void SetPolygonMode(VkPolygonMode mode) {
            Rasterizer.polygonMode = mode;
            Rasterizer.lineWidth = 1.f;
        }

        void SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace) {
            Rasterizer.cullMode = cullMode;
            Rasterizer.frontFace = frontFace;
        }

        void DisableBlending() {
            // default write mask
            ColorBlendAttachment.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            // no blending
            ColorBlendAttachment.blendEnable = VK_FALSE;
        }

        void EnableBlendingAdditive() {
            ColorBlendAttachment.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            ColorBlendAttachment.blendEnable = VK_TRUE;
            ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        void EnableBlendingAlphablend() {
            ColorBlendAttachment.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            ColorBlendAttachment.blendEnable = VK_TRUE;
            ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            ColorBlendAttachment.dstColorBlendFactor =
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        void SetDepthFormat(VkFormat format) {
            RenderInfo.depthAttachmentFormat = format;
        }

        void SetMultisamplingNone();

        void DisableDepthTest();

        // This are used privately because the format color is from the DrawImage
        // format we use to draw, and the and to set shaders and build the modules
        // is done internally
        //
        void SetShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);

        void SetColorAttachmentFormat(VkFormat format) {
            ColorAttachmentformat = format;
            // connect the format to the renderInfo  structure
            RenderInfo.colorAttachmentCount = 1;
            RenderInfo.pColorAttachmentFormats = &ColorAttachmentformat;
        }
};

struct uniform_buffer_ui {
    F32 Time;
    F32 DeltaTime;
    F32 Width;
    F32 Height;
    F32 AtlasWidth;
    F32 AtlasHeight;
};

struct batch_2d {
    vector<vertex2d> Vertices;
    U32 *Indices{nullptr};
    U32 *Instances{nullptr};
};

class buffer_batch_group {
    public:
    vector<vk_buffer> Buffer;
    U32 N_Batches;
    U32 CurrentBatchIdx;
};

class vulkan_iface {
    public:
    // ----- Global variables ---------------
    //
    global inline const char *VALIDATION_LAYERS[] = {
        "VK_LAYER_KHRONOS_validation"};
    global inline const char *DEVICE_EXTENSIONS[] = {
        "VK_KHR_swapchain", "VK_KHR_dynamic_rendering",
        "VK_EXT_descriptor_indexing",
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME};

    // -------- General ---------------------------------------------
    //
    VkInstance Instance;
    window Window;
    vk_device Device;
    vk_swapchain Swapchain;
    vector<vk_pipeline> Pipelines;
    vk_descriptor_allocator GlobalDescriptorAllocator;
    VkDescriptorSet DrawImageDescriptors;
    VkDescriptorSetLayout DrawImageDescriptorLayout;
    vk_semaphore Semaphores;

    // -------- Memory Handling -------------------------------------
    //
    Arena *RenderArena;
    Arena *TempArena;
    VmaAllocator GPUAllocator;

    // ----- Immediate Submite --------------------------------------
    //
    VkFence ImmFence;
    VkCommandBuffer ImmCommandBuffer;

    // ----- Compute pipeline ---------------------------------------
    //
    VkBuffer *ShaderStorage;
    VkDeviceMemory *ShaderMemory;

    VkPipeline BackgroundComputePipeline;
    VkPipelineLayout BackgroundComputePipelineLayout;

    // ----- Testing Triangle pipelines -----------------------------
    //
    VkPipeline TrianglePipeline;
    VkPipelineLayout TrianglePipelineLayout;

    // ----- Uniform Buffers ----------------------------------------
    //
    vk_buffer UniformBufferUI[MAX_FRAMES_IN_FLIGHT];
    uniform_buffer_ui CurrentUniformBufferUI;

    // ----- Vulkan command buffers ---------------------------------
    //
    VkCommandPool CommandPool[MAX_FRAMES_IN_FLIGHT];
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
    vk_image *TextureImage;
    U32 N_TextureImages{0};

    // ----- For frames ---------------------------------------------
    //
    U64 CurrentFrame{0};
    F64 LastTimeFrame{0.0f};
    bool FramebufferResized{false};
    F64 LastTime{0.0f};

    // ----- Debug Uitilities ---------------------------------------
    //
    VkDebugUtilsMessengerEXT DebugMessenger;

    // ----- Main Functions -----------------------------------------
    //
    vulkan_iface() = delete;
    vulkan_iface(const char *window_name);

    ~vulkan_iface() {}

    vulkan_iface(vulkan_iface &v) = delete;

    vulkan_iface(vulkan_iface &&v) = delete;

    void CreateSwapchain();

    void CreateImageViews();

    void InitCommands();

    void InitSyncStructures();

    void InitDescriptors();

    void InitPipelines();

    void InitBackgroundPipelines();

    void InitTrianglePipeline();

    void DrawGeometry(VkCommandBuffer cmd);

    void BeginDrawing();

    void UploadMesh(vector<U32> &indices, vector<vertex2d> &vertices);

    void AddPipeline(vk_pipeline_builder &builder, const char *vert_path,
        const char *frag_path);

    void extracted(VkDescriptorPoolCreateInfo &pool_info,
        VkDescriptorPool &imguiPool);
    void InitImgui();

    private:
    // -------- Destruction queue -----------------------------------
    //
    vector<std::function<void()>> FrameDeletionQueue[MAX_FRAMES_IN_FLIGHT];
    vector<std::function<void()>> MainDeletionQueue;
    vector<std::function<void()>> SwapchainDeletionQueue;

    // -------- Private Functions -----------------------------------
    //
    /*
    Params:
    * cmd            : CommandBuffer that we want to draw into
    * targetImageView: ImageView that we want to draw to
    */
    void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);

    /*
    Params:
    * flags: Usage flags to configure current command buffer
    */
    VkCommandBufferBeginInfo CommandBufferBeginInfo(VkCommandBufferUsageFlags flags);

    /*
    Params:
    * allocSize  : Size of buffer
    * usage      : Type of buffer
    * memoryUsage: Kind of usage over memory buffer
    */
    vk_buffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    /*
    Params:
    * function: Function that takes a command buffer to execute an user-custom function
    */
    void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function);

    /*
    Params:
    * cmd: CommandBuffer
    * image: The image we want to change its layout
    * currentLayout: Layout of the image
    * newLayout: The layout we want to transition to
    */
    void TransitionImage(VkCommandBuffer cmd, VkImage image,
        VkImageLayout currentLayout, VkImageLayout newLayout);

    /*
    Params:
    * cmd: The command buffer used to draw the background
    */
    void DrawBackground(VkCommandBuffer cmd);

    /*
    Params:
    * None
    */
    void DestroySwapchain();

    /*
    Params:
    * None
    */
    void ResizeSwapchain();

    bool LoadShaderModule(const char *FilePath, VkDevice Device,
        VkShaderModule *OutShaderModule);

    void CopyImageToImage(VkCommandBuffer cmd, VkImage source,
        VkImage destination, VkExtent2D srcSize,
        VkExtent2D dstSize);

    vk_buffer AllocateBuffer(size_t AllocSize, VkBufferUsageFlags Usage,
        VmaMemoryUsage MemoryUsage);

    void DestroyBuffer(const vk_buffer &buffer);

    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo();

    VkRenderingInfo RenderingInfo(VkExtent2D Extent,
        VkRenderingAttachmentInfo *ColorInfo,
        VkRenderingAttachmentInfo *DepthInfo);

    VkRenderingAttachmentInfo
    AttachmentInfo(VkImageView View, VkClearValue *Clear, VkImageLayout Layout);

    VkImageCreateInfo ImageCreateInfo(VkFormat format,
        VkImageUsageFlags usageFlags,
        VkExtent3D extent);

    VkImageViewCreateInfo ImageViewCreateInfo(VkFormat format, VkImage image,
        VkImageAspectFlags aspectFlags);

    VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer cmd);

    VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 stageMask,
        VkSemaphore semaphore);

    VkSubmitInfo2 SubmitInfo(VkCommandBufferSubmitInfo *cmd,
        VkSemaphoreSubmitInfo *signalSemaphoreInfo,
        VkSemaphoreSubmitInfo *waitSemaphoreInfo);

    VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags /*= 0*/);

    VkSemaphoreCreateInfo
    SemaphoreCreateInfo(VkSemaphoreCreateFlags flags /*= 0*/);

    VkCommandPoolCreateInfo
    CommandPoolCreateInfo(U32 queueFamilyIndex,
        VkCommandPoolCreateFlags flags /*= 0*/);

    VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool pool,
        uint32_t count /*= 1*/);

    swapchain_support_details QuerySwapChainSupport(VkPhysicalDevice Device);

    bool CheckDeviceExtensionSupport(VkPhysicalDevice Device);

    queue_family_indices FindQueueFamilies(VkPhysicalDevice &device);

    bool IsSuitableDevice(VkPhysicalDevice &device);

    bool CheckValidationLayerSupport();

    char **GetRequiredExtensions(uint32_t *ExtensionCount);
};

#endif // VULKAN_IMPL_H
