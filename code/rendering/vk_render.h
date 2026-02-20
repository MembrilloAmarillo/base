#ifndef VULKAN_RENDER_H
#define VULKAN_RENDER_H

#ifdef f32
    #undef f32
#endif

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wnullability-completeness"
# pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(__GNUC__)
# pragma GCC   diagnostic push
# pragma GCC   diagnostic ignored "-Wunused-parameter"
#endif

#include <array>
#include <source_location>
#include <memory>
#include <bitset>
#include <functional>

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>
#include <volk/volk.h>

// Include SDL3 before any X11 headers that might be pulled in
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <ktx.h>
#include <ktxvulkan.h>
#include <stdio.h>

// Simple tracing macro to help find where intermittent crashes occur.
// Prints function name and line so logs can be correlated with source.
#ifndef NO_DEBUG_TRACE
    #ifndef TRACE
    #define TRACE(fmt, ...) do { fprintf(stdout, "[%s:%d] " fmt "\n", __func__, __LINE__, ##__VA_ARGS__); fflush(stdout); } while(0)
    #endif
#else
    #ifndef TRACE
    #define TRACE(fmt, ...) do {} while(0)
    #endif
#endif

//#define TINYOBJLOADER_IMPLEMENTATION
#include "third-party/tiny_obj_loader.h"

#ifdef f32
    #undef f32
    #define f32 float
#endif

#include "../util/types.h"
#include "../memory/memory.h"
#include "../memory/allocator.h"
#include "../util/strings.h"

#include "../vector/DynamicVector.h"

#include "../window/window_creation.h"

// Undefine X11 macros AFTER window headers to prevent conflicts
#ifdef Window
#undef Window
#endif

namespace Vk {


static inline void Check(bool result, const std::source_location& loc = std::source_location::current()) {
    if (!result) {
        printf("Vulkan call returned an error at %s:%u (%s)\n", loc.file_name(), (unsigned)loc.line(), loc.function_name());
        exit(EXIT_FAILURE);
    }
}

static inline void Check(VkResult result, const std::source_location& loc = std::source_location::current()) {
    if (result != VK_SUCCESS) {
        printf("Vulkan call returned an error at %s:%u (%s) => %d\n", loc.file_name(), (unsigned)loc.line(), loc.function_name(), (int)result);
        exit(result);
    }
}

}

// forward declaration
//
class Vk_Render;

struct Shader_Data {
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 model[3];
	glm::vec4 lightPos{ 0.0f, -10.0f, 10.0f, 0.0f };
	uint32_t selected{ 1 };
};

struct V_3d {
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec2 uv;
};

class Shader_Loader {
    public:

    enum Shader_Stage {
        NONE = 0,
            VERTEX   = 1 << 0,
            FRAGMENT = 1 << 1,
            GEOMETRY = 1 << 2,
            COMPUTE  = 1 << 3
    };

    typedef U32 Shader_Stages_Flag;

    Slang::ComPtr<slang::IGlobalSession> slang_global_session;
    Slang::ComPtr<slang::ISession> slang_session;
    Slang::ComPtr<slang::IModule> slang_module;
    Slang::ComPtr<ISlangBlob> spirv;

    VkShaderModule shader_module;

    static inline void Check(VkResult result, const std::source_location& loc = std::source_location::current()) {
        if (result != VK_SUCCESS) {
            printf("Vulkan call returned an error at %s:%u (%s) => %d\n", loc.file_name(), (unsigned)loc.line(), loc.function_name(), (int)result);
            exit(result);
        }
    }

    Shader_Loader(Slang::ComPtr<slang::IGlobalSession> slang_global_session) {
        this->slang_global_session = slang_global_session;
    }

    void Shader_Loader_Destroy(VkDevice device) {
        vkDestroyShaderModule(device, shader_module, nullptr);
    }

    void Create_Session(slang::SessionDesc& slang_session_desc) {
        slang_global_session->createSession(slang_session_desc, slang_session.writeRef());
    }

    void Load_Module(VkDevice device, const char* module_name, const char* path) {
        slang_module = slang_session->loadModuleFromSource(module_name, path, nullptr, nullptr);

        slang_module->getTargetCode(0, spirv.writeRef());
        size_t spirv_size = spirv ? spirv->getBufferSize() : 0;
        const void* spirv_ptr = spirv ? spirv->getBufferPointer() : nullptr;

        if (spirv_size == 0 || spirv_ptr == nullptr) {
            fprintf(stderr, "Failed to produce SPIR-V for module '%s' from '%s' (codeSize=0)\n", module_name, path);
            fflush(stderr);
            exit(EXIT_FAILURE);
        }

        VkShaderModuleCreateInfo shader_module_ci {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv_size,
            .pCode = (U32*)spirv_ptr
        };

        Check(vkCreateShaderModule(device, &shader_module_ci, nullptr, &shader_module));
    }
};

class Vk_Descriptor {
private:
    struct DescriptorBinding {
        U32 binding;
        VkDescriptorType type;
        U32 count;
        VkShaderStageFlags stages;
    };

    struct PendingWrite {
        VkWriteDescriptorSet write;
        VkDescriptorBufferInfo buffer_info;  // Storage for single buffer write
        VkDescriptorImageInfo* image_infos;  // Allocated storage for image writes
        U32 image_count;
        Allocator* image_alloc;  // Allocator for image_infos cleanup
    };

    Allocator* allocator;
    VkDevice device;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorSet descriptor_set;

    dyn_vector<DescriptorBinding> bindings;
    dyn_vector<PendingWrite> pending_writes;

    // Helper to find binding by index and validate type
    const DescriptorBinding* FindBinding(U32 binding) const {
        for (U64 i = 0; i < bindings.Length(); ++i) {
            if (bindings[i].binding == binding) {
                return &bindings[i];
            }
        }
        return nullptr;
    }

    void ValidateBindingType(U32 binding, VkDescriptorType expected_type) {
        const DescriptorBinding* b = FindBinding(binding);
        if (!b) {
            fprintf(stderr, "Error: Binding %u not found in descriptor set\n", binding);
            exit(EXIT_FAILURE);
        }
        if (b->type != expected_type) {
            fprintf(stderr, "Error: Binding %u has type %d, but expected %d\n", binding, (int)b->type, (int)expected_type);
            exit(EXIT_FAILURE);
        }
    }

    static inline void Check(VkResult result, const std::source_location& loc = std::source_location::current()) {
        if (result != VK_SUCCESS) {
            printf("Vulkan call returned an error at %s:%u (%s) => %d\n", loc.file_name(), (unsigned)loc.line(), loc.function_name(), (int)result);
            exit(result);
        }
    }

public:
    // Builder class for fluent API
    class Builder {
    private:
        Allocator* allocator;
        VkDevice device;
        dyn_vector<DescriptorBinding> bindings;

    public:
        Builder(Allocator* alloc, VkDevice dev)
            : allocator(alloc), device(dev),
              bindings(dyn_vector<DescriptorBinding>::Init(alloc, 16)) {}

        Builder& Add_Binding(U32 binding, VkDescriptorType type, U32 count, VkShaderStageFlags stages) {
            // Validate no duplicate bindings
            for (U64 i = 0; i < bindings.Length(); ++i) {
                if (bindings[i].binding == binding) {
                    fprintf(stderr, "Error: Binding %u already added to descriptor\n", binding);
                    exit(EXIT_FAILURE);
                }
            }
            DescriptorBinding b{binding, type, count, stages};
            bindings.AppendByCopy(b);
            return *this;
        }

        Builder& Uniform_Buffer(U32 binding, VkShaderStageFlags stages, U32 count = 1) {
            return Add_Binding(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count, stages);
        }

        Builder& Storage_Buffer(U32 binding, VkShaderStageFlags stages, U32 count = 1) {
            return Add_Binding(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, count, stages);
        }

        Builder& Combined_Image_Sampler(U32 binding, VkShaderStageFlags stages, U32 count = 1) {
            return Add_Binding(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count, stages);
        }

        Builder& Storage_Image(U32 binding, VkShaderStageFlags stages, U32 count = 1) {
            return Add_Binding(binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, count, stages);
        }

        Builder& Sampler(U32 binding, VkShaderStageFlags stages, U32 count = 1) {
            return Add_Binding(binding, VK_DESCRIPTOR_TYPE_SAMPLER, count, stages);
        }

        Builder& Sampled_Image(U32 binding, VkShaderStageFlags stages, U32 count = 1) {
            return Add_Binding(binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, count, stages);
        }

        Builder& Acceleration_Structure(U32 binding, VkShaderStageFlags stages, U32 count = 1) {
            return Add_Binding(binding, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, count, stages);
        }

        Vk_Descriptor Build();
    };

    // Factory method to create a builder
    static Builder Create(Allocator* alloc, VkDevice device) {
        return Builder(alloc, device);
    }

    // Default constructor for legacy compatibility
    Vk_Descriptor() : allocator(nullptr), device(VK_NULL_HANDLE),
                      descriptor_pool(VK_NULL_HANDLE),
                      descriptor_set_layout(VK_NULL_HANDLE),
                      descriptor_set(VK_NULL_HANDLE) {}

    // Private constructor called by Builder
    Vk_Descriptor(Allocator* alloc, VkDevice dev,
                  const dyn_vector<DescriptorBinding>& binding_list)
        : allocator(alloc), device(dev),
          bindings(dyn_vector<DescriptorBinding>::Init(alloc, binding_list.Length())),
          pending_writes(dyn_vector<PendingWrite>::Init(alloc, binding_list.Length())) {

        // Copy bindings
        for (U64 i = 0; i < binding_list.Length(); ++i) {
            bindings.AppendByCopy(binding_list[i]);
        }

        // Create Vulkan layout bindings
        dyn_vector<VkDescriptorSetLayoutBinding> vk_bindings =
            dyn_vector<VkDescriptorSetLayoutBinding>::Init(alloc, binding_list.Length());

        for (U64 i = 0; i < bindings.Length(); ++i) {
            VkDescriptorSetLayoutBinding vk_binding{
                .binding = bindings[i].binding,
                .descriptorType = bindings[i].type,
                .descriptorCount = bindings[i].count,
                .stageFlags = bindings[i].stages
            };
            vk_bindings.AppendByCopy(vk_binding);
        }

        // Create descriptor set layout
        VkDescriptorSetLayoutCreateInfo layout_ci{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = (U32)vk_bindings.Length(),
            .pBindings = vk_bindings.Memory()
        };
        Check(vkCreateDescriptorSetLayout(dev, &layout_ci, nullptr, &descriptor_set_layout));

        // Build pool sizes (aggregate by type)
        dyn_vector<VkDescriptorPoolSize> pool_sizes =
            dyn_vector<VkDescriptorPoolSize>::Init(alloc, bindings.Length());

        for (U64 i = 0; i < bindings.Length(); ++i) {
            // Check if this type already exists in pool_sizes
            bool found = false;
            for (U64 j = 0; j < pool_sizes.Length(); ++j) {
                if (pool_sizes[j].type == bindings[i].type) {
                    pool_sizes[j].descriptorCount += bindings[i].count;
                    found = true;
                    break;
                }
            }
            if (!found) {
                VkDescriptorPoolSize ps{
                    .type = bindings[i].type,
                    .descriptorCount = bindings[i].count
                };
                pool_sizes.AppendByCopy(ps);
            }
        }

        // Create descriptor pool
        VkDescriptorPoolCreateInfo pool_ci{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = (U32)pool_sizes.Length(),
            .pPoolSizes = pool_sizes.Memory()
        };
        Check(vkCreateDescriptorPool(dev, &pool_ci, nullptr, &descriptor_pool));

        // Allocate descriptor set
        VkDescriptorSetAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptor_set_layout
        };
        Check(vkAllocateDescriptorSets(dev, &alloc_info, &descriptor_set));

        vk_bindings.Destroy();
        pool_sizes.Destroy();
    }

    // Accessors
    VkDescriptorSetLayout Get_Layout() const { return descriptor_set_layout; }
    VkDescriptorSet Get_Set() const { return descriptor_set; }

    // Typed write methods with validation
    void Write_Buffer(U32 binding, const VkDescriptorBufferInfo& buffer_info) {
        ValidateBindingType(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        PendingWrite pw{};
        pw.write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        pw.write.dstSet = descriptor_set;
        pw.write.dstBinding = binding;
        pw.write.descriptorCount = 1;
        pw.write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pw.write.pBufferInfo = &pw.buffer_info;
        pw.buffer_info = buffer_info;
        pw.image_infos = nullptr;
        pw.image_count = 0;

        pending_writes.AppendByCopy(pw);
    }

    void Write_Storage_Buffer(U32 binding, const VkDescriptorBufferInfo& buffer_info) {
        ValidateBindingType(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        PendingWrite pw{};
        pw.write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        pw.write.dstSet = descriptor_set;
        pw.write.dstBinding = binding;
        pw.write.descriptorCount = 1;
        pw.write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pw.write.pBufferInfo = &pw.buffer_info;
        pw.buffer_info = buffer_info;
        pw.image_infos = nullptr;
        pw.image_count = 0;

        pending_writes.AppendByCopy(pw);
    }

    void Write_Image(U32 binding, const VkDescriptorImageInfo* image_infos, U32 count) {
        ValidateBindingType(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        PendingWrite pw{};
        pw.write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        pw.write.dstSet = descriptor_set;
        pw.write.dstBinding = binding;
        pw.write.descriptorCount = count;
        pw.write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        // Allocate and copy image infos so they remain valid
        if (image_infos && count > 0) {
            pw.image_infos = Mem_Allocator::Make<VkDescriptorImageInfo>(allocator, count);
            memcpy(pw.image_infos, image_infos, sizeof(VkDescriptorImageInfo) * count);
        } else {
            pw.image_infos = nullptr;
        }
        pw.write.pImageInfo = pw.image_infos;

        pw.buffer_info = {};
        pw.image_count = count;
        pw.image_alloc = allocator;

        pending_writes.AppendByCopy(pw);
    }

    void Write_Storage_Image(U32 binding, const VkDescriptorImageInfo* image_infos, U32 count) {
        ValidateBindingType(binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        PendingWrite pw{};
        pw.write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        pw.write.dstSet = descriptor_set;
        pw.write.dstBinding = binding;
        pw.write.descriptorCount = count;
        pw.write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        // Allocate and copy image infos so they remain valid
        if (image_infos && count > 0) {
            pw.image_infos = Mem_Allocator::Make<VkDescriptorImageInfo>(allocator, count);
            memcpy(pw.image_infos, image_infos, sizeof(VkDescriptorImageInfo) * count);
        } else {
            pw.image_infos = nullptr;
        }
        pw.write.pImageInfo = pw.image_infos;

        pw.buffer_info = {};
        pw.image_count = count;
        pw.image_alloc = allocator;

        pending_writes.AppendByCopy(pw);
    }

    void Write_Sampler(U32 binding, const VkSampler& sampler) {
        ValidateBindingType(binding, VK_DESCRIPTOR_TYPE_SAMPLER);

        // Note: This is simplified; for real samplers you'd want to store a VkDescriptorImageInfo
        fprintf(stderr, "Write_Sampler not fully implemented yet\n");
    }

    // Flush all pending writes in a single Vulkan call
    void Flush() {
        if (pending_writes.Length() == 0) {
            return;
        }

        dyn_vector<VkWriteDescriptorSet> writes =
            dyn_vector<VkWriteDescriptorSet>::Init(allocator, pending_writes.Length());

        for (U64 i = 0; i < pending_writes.Length(); ++i) {
            VkWriteDescriptorSet write = pending_writes[i].write;

            // Fix dangling pointers: point to buffer/image info within pending_writes
            if (write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                write.pBufferInfo = &pending_writes[i].buffer_info;
            } else if (write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                       write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                       write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
                write.pImageInfo = pending_writes[i].image_infos;
            }

            writes.AppendByCopy(write);
        }

        vkUpdateDescriptorSets(device, (U32)writes.Length(), writes.Memory(), 0, nullptr);
        writes.Destroy();

        // Clean up allocated image_infos
        for (U64 i = 0; i < pending_writes.Length(); ++i) {
            if (pending_writes[i].image_infos != nullptr && pending_writes[i].image_alloc != nullptr) {
                Mem_Allocator::Delete(pending_writes[i].image_alloc, pending_writes[i].image_infos);
            }
        }

        pending_writes.Len = 0;  // Clear pending writes
    }

    // Proper destructor with resource cleanup
    ~Vk_Descriptor() {
        if (device != VK_NULL_HANDLE) {
            if (descriptor_set_layout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
            }
            if (descriptor_pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
            }
        }
        if (allocator) {
            // Clean up allocated image_infos in pending writes
            for (U64 i = 0; i < pending_writes.Length(); ++i) {
                if (pending_writes[i].image_infos != nullptr && pending_writes[i].image_alloc != nullptr) {
                    Mem_Allocator::Delete(pending_writes[i].image_alloc, pending_writes[i].image_infos);
                }
            }
            bindings.Destroy();
            pending_writes.Destroy();
        }
    }

    // Prevent copying
    Vk_Descriptor(const Vk_Descriptor&) = delete;
    Vk_Descriptor& operator=(const Vk_Descriptor&) = delete;

    // Allow moving
    Vk_Descriptor(Vk_Descriptor&& other) noexcept
        : allocator(other.allocator), device(other.device),
          descriptor_pool(other.descriptor_pool),
          descriptor_set_layout(other.descriptor_set_layout),
          descriptor_set(other.descriptor_set),
          bindings(other.bindings),
          pending_writes(other.pending_writes) {
        other.descriptor_pool = VK_NULL_HANDLE;
        other.descriptor_set_layout = VK_NULL_HANDLE;
        other.descriptor_set = VK_NULL_HANDLE;
    }

    Vk_Descriptor& operator=(Vk_Descriptor&& other) noexcept {
        if (this != &other) {
            this->~Vk_Descriptor();
            allocator = other.allocator;
            device = other.device;
            descriptor_pool = other.descriptor_pool;
            descriptor_set_layout = other.descriptor_set_layout;
            descriptor_set = other.descriptor_set;
            bindings = other.bindings;
            pending_writes = other.pending_writes;
            other.descriptor_pool = VK_NULL_HANDLE;
            other.descriptor_set_layout = VK_NULL_HANDLE;
            other.descriptor_set = VK_NULL_HANDLE;
        }
        return *this;
    }
};

// ============================================================================
// Bindless Descriptor
// ============================================================================
class Vk_Bindless_Table {
private:
    static constexpr U32 MAX_TEXTURES = 16384;
    static constexpr U32 MAX_SAMPLERS = 128;
    static constexpr U32 MAX_STORAGE_BUFFERS = 4096;

    struct Handle_Entry {
        U32 generation;
        bool is_valid;
    };

    Allocator* allocator;
    VkDevice device;

    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkDescriptorSetLayout descriptor_set_layout;

    // Handle allocation tracking
    dyn_vector<Handle_Entry> handle_entries;
    dyn_vector<U32> free_handles;
    U32 next_generation;

    // Track current occupancy for variable descriptor count
    U32 current_texture_count;
    U32 current_sampler_count;
    U32 current_buffer_count;

public:
    Vk_Bindless_Table()
        : allocator(nullptr), device(VK_NULL_HANDLE),
          descriptor_pool(VK_NULL_HANDLE), descriptor_set(VK_NULL_HANDLE),
          descriptor_set_layout(VK_NULL_HANDLE),
          next_generation(1), current_texture_count(0),
          current_sampler_count(0), current_buffer_count(0) {}

    // Initialize the bindless table
    void Init(Allocator* alloc, VkDevice dev);

    // Shutdown and cleanup
    void Shutdown();

    // Register a texture and get a handle (index)
    U32 Register_Texture(VkImageView image_view, VkImageLayout layout);

    // Register a sampler and get a handle (index)
    U32 Register_Sampler(VkSampler sampler);

    // Register a storage buffer and get a handle (index)
    U32 Register_Storage_Buffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size);

    // Unregister a resource by handle
    void Unregister(U32 handle);

    // Accessors
    VkDescriptorSetLayout Get_Layout() const { return descriptor_set_layout; }
    VkDescriptorSet Get_Set() const { return descriptor_set; }

    // Destructor
    ~Vk_Bindless_Table() { Shutdown(); }

    // Prevent copying
    Vk_Bindless_Table(const Vk_Bindless_Table&) = delete;
    Vk_Bindless_Table& operator=(const Vk_Bindless_Table&) = delete;

    // Allow moving
    Vk_Bindless_Table(Vk_Bindless_Table&& other) noexcept
        : allocator(other.allocator), device(other.device),
          descriptor_pool(other.descriptor_pool), descriptor_set(other.descriptor_set),
          descriptor_set_layout(other.descriptor_set_layout),
          handle_entries(other.handle_entries), free_handles(other.free_handles),
          next_generation(other.next_generation),
          current_texture_count(other.current_texture_count),
          current_sampler_count(other.current_sampler_count),
          current_buffer_count(other.current_buffer_count) {
        other.descriptor_pool = VK_NULL_HANDLE;
        other.descriptor_set = VK_NULL_HANDLE;
        other.descriptor_set_layout = VK_NULL_HANDLE;
    }

    Vk_Bindless_Table& operator=(Vk_Bindless_Table&& other) noexcept {
        if (this != &other) {
            Shutdown();
            allocator = other.allocator;
            device = other.device;
            descriptor_pool = other.descriptor_pool;
            descriptor_set = other.descriptor_set;
            descriptor_set_layout = other.descriptor_set_layout;
            handle_entries = other.handle_entries;
            free_handles = other.free_handles;
            next_generation = other.next_generation;
            current_texture_count = other.current_texture_count;
            current_sampler_count = other.current_sampler_count;
            current_buffer_count = other.current_buffer_count;
            other.descriptor_pool = VK_NULL_HANDLE;
            other.descriptor_set = VK_NULL_HANDLE;
            other.descriptor_set_layout = VK_NULL_HANDLE;
        }
        return *this;
    }
};

class Vk_Pipeline : public Shader_Loader {

    public:
    Allocator* allocator;

    U64 handler;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;

    dyn_vector<VkVertexInputBindingDescription>    vertex_bindings;
    dyn_vector<VkVertexInputAttributeDescription>  vertex_attributes;
    dyn_vector<VkPipelineShaderStageCreateInfo>    shader_stages;
    dyn_vector<VkDynamicState>                     dynamic_states;
    dyn_vector<VkPushConstantRange>                push_contant_ranges;
    dyn_vector<VkDescriptorSetLayout>              descriptor_set_layouts;

    // To explicitly specify if depth stencil is necessary
    //
    VkPipelineDepthStencilStateCreateInfo depth_ci;

    Vk_Pipeline(Allocator* allocator, Slang::ComPtr<slang::IGlobalSession> slang_global_session) :
        allocator(allocator),
        Shader_Loader(slang_global_session)
    {
        // by default, 1 push constant, 1 descriptor set layout, 2 dynamic states (viewport, scissor),
        // 2 shader stages (vertex, fragment)
        //
        push_contant_ranges    = dyn_vector<VkPushConstantRange>::Init(allocator, 1);
        vertex_bindings        = dyn_vector<VkVertexInputBindingDescription>::Init(allocator, 1);
        vertex_attributes      = dyn_vector<VkVertexInputAttributeDescription>::Init(allocator, 8);
        descriptor_set_layouts = dyn_vector<VkDescriptorSetLayout>::Init(allocator, 1);
        dynamic_states         = dyn_vector<VkDynamicState>::Init(allocator, 2);
        shader_stages          = dyn_vector<VkPipelineShaderStageCreateInfo>::Init(allocator, 2);
    }
    ~Vk_Pipeline() {}

    void Destroy(VkDevice device) {
        Shader_Loader_Destroy(device);
        vkDestroyPipeline(device, pipeline, nullptr);
    }

    void Push_Descriptor_Set_Layout(VkDescriptorSetLayout layout) {
        descriptor_set_layouts.Append(layout);
    }

    // Insert bindless layout at the beginning (set 0)
    void Prepend_Bindless_Layout(VkDescriptorSetLayout bindless_layout) {
        // Insert bindless layout at position 0
        // This is a bit inefficient but fine for this use case
        dyn_vector<VkDescriptorSetLayout> temp = dyn_vector<VkDescriptorSetLayout>::Init(allocator, descriptor_set_layouts.Length() + 1);
        temp.AppendByCopy(bindless_layout);
        for (U64 i = 0; i < descriptor_set_layouts.Length(); ++i) {
            temp.AppendByCopy(descriptor_set_layouts[i]);
        }
        descriptor_set_layouts.Destroy();
        descriptor_set_layouts = temp;
    }

    void Set_Push_Constant_Range(VkPushConstantRange constant) {
        push_contant_ranges.Append(constant);
    }

    void Push_Dynamic_State(VkDynamicState state) {
        dynamic_states.Append(state);
    }

    void Push_Dynamic_States(dyn_vector<VkDynamicState>& states) {
        for( auto state : states ) {
            dynamic_states.Append(state);
        }
    }

    void Set_Depth() {
        depth_ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
        };
    }
    void Set_Shader_Stages(Shader_Stages_Flag flags) {
        if( flags & Shader_Loader::VERTEX ) {
            shader_stages.AppendByCopy(
    	  { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = shader_module, .pName = "VSMain"}
            );
        }
        if( flags & Shader_Loader::FRAGMENT ) {
            shader_stages.AppendByCopy(
    	  { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = shader_module, .pName = "PSMain"}
            );
        }
        if( flags & Shader_Loader::GEOMETRY ) {
            shader_stages.AppendByCopy(
    	  { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_GEOMETRY_BIT, .module = shader_module, .pName = "GSMain"}
            );
        }
        if( flags & Shader_Loader::COMPUTE ) {
            shader_stages.AppendByCopy(
    	  { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader_module, .pName = "CSMain"}
            );
        }
    }
    void Push_Vertex_Binding(U32 binding, U32 stride, VkVertexInputRate rate) {
        vertex_bindings.AppendByCopy(
            {
    		.binding = binding,
    		.stride = stride,
    		.inputRate = rate
    	   }
        );
    }
    void Push_Vertex_Attributes(U32 location, U32 binding, VkFormat format, U32 offset = 0) {
        vertex_attributes.AppendByCopy(
            { .location = location, .binding = binding, .format = format, .offset = offset }
        );
    }

    void Create(VkDevice device, VkFormat image_format, VkFormat depth_format) {

        VkPipelineLayoutCreateInfo pipelineLayoutCI{
        	.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        	.setLayoutCount = static_cast<U32>(descriptor_set_layouts.Length()),
        	.pSetLayouts = descriptor_set_layouts.Length() == 0 ? nullptr : descriptor_set_layouts.Memory(),
        	.pushConstantRangeCount = static_cast<U32>(push_contant_ranges.Length()),
        	.pPushConstantRanges = push_contant_ranges.Memory()
        };
        Check(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipeline_layout));

        VkPipelineVertexInputStateCreateInfo vertexInputState{
    		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    		.vertexBindingDescriptionCount = static_cast<U32>(vertex_bindings.Length()),
    		.pVertexBindingDescriptions = vertex_bindings.Memory(),
    		.vertexAttributeDescriptionCount = static_cast<U32>(vertex_attributes.Length()),
    		.pVertexAttributeDescriptions = vertex_attributes.Memory(),
    	};

    	VkPipelineInputAssemblyStateCreateInfo input_assembly_state{ .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
    	VkPipelineDynamicStateCreateInfo dynamic_state {
    	   .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    	   .dynamicStateCount = static_cast<U32>(dynamic_states.Length()),
    	   .pDynamicStates = dynamic_states.Memory()
    	};
    	VkPipelineViewportStateCreateInfo viewport_state { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1 };
    	VkPipelineRasterizationStateCreateInfo rasterization_state {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .lineWidth = 1.0
        };
    	VkPipelineMultisampleStateCreateInfo multi_sample_state{ .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
    	VkPipelineColorBlendAttachmentState blend_attachment{ .colorWriteMask = 0xF };
    	VkPipelineColorBlendStateCreateInfo color_blend_state{
    	   .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    	   .attachmentCount = 1,
    	   .pAttachments = &blend_attachment
    	};

        VkPipelineRenderingCreateInfo renderingCI{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &image_format,
		.depthAttachmentFormat = depth_format
    	};

    	VkGraphicsPipelineCreateInfo pipelineCI{
    		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    		.pNext = &renderingCI,
    		.stageCount = static_cast<U32>(shader_stages.Length()),
    		.pStages = shader_stages.Memory(),
    		.pVertexInputState = &vertexInputState,
    		.pInputAssemblyState = &input_assembly_state,
    		.pViewportState = &viewport_state ,
    		.pRasterizationState = &rasterization_state,
    		.pMultisampleState = &multi_sample_state,
    		.pDepthStencilState = &depth_ci,
    		.pColorBlendState = &color_blend_state,
    		.pDynamicState = &dynamic_state,
    		.layout = pipeline_layout
    	};
    	Check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));
    }
};

class Vk_Texture {
    public:
    Vk_Texture( Allocator* allocator ) : allocator(allocator) {}
    ~Vk_Texture() {}

    // Load texture from KTX file and return bindless handles (register automatically)
    // Returns: texture handle
    U32 Load_From_Ktx_Bindless(Vk_Render *render, const char *path);

    // Internal implementation returning both handles
    // returns texture handler and sampler handler
    //
    std::pair<U32, U32> Load_From_Ktx_Bindless_With_Sampler(Vk_Render *render, const char *path);

    // Legacy method - returns descriptor image info (deprecated, kept for compatibility)
    VkDescriptorImageInfo Load_From_Ktx(Vk_Render *render, const char *path);

    void Load_From_Png( const char* path, VkDevice device, VmaAllocator& allocator );
    void Load_From_Data( const char* path, VkDevice device, VmaAllocator& allocator );

    void Create_Sampler(VkDevice device, f32 max_lod = 0) {
        VkSamplerCreateInfo sampler_ci {
    		.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    		.magFilter        = VK_FILTER_LINEAR,
    		.minFilter        = VK_FILTER_LINEAR,
    		.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    		.anisotropyEnable = VK_TRUE,
    		.maxAnisotropy    = 8.0f,
    		.maxLod           = max_lod,
    	};
    	Vk::Check(vkCreateSampler(device, &sampler_ci , nullptr, &sampler));
    }

    VkImage       Get_Image()      const { return image;      }
    VkImageView   Get_Image_View() const { return image_view; }
    VkSampler     Get_Sampler()    const { return sampler;    }
    VkImageLayout Get_Layout()     const { return layout;     }

private:
    Allocator* allocator;
    VkImage image;
    VkImageView image_view;
    VkSampler sampler;
    VkImageLayout layout;
    VmaAllocation allocation;
};

template <typename T>
class Vk_Buffer {
    public:

    enum Buffer_Usage : U32
    {
        Uniform  = 1 << 0,
        Index32  = 1 << 1,
        Vertex   = 1 << 2,
        Storage  = 1 << 3,
        Indirect = 1 << 4,
        Upload   = 1 << 5,

        Custom_Bits_Begin = 1 << 6,
    };

    struct Buffer_Description {

        U8_String    debug_name;
        Buffer_Usage usage;
        U64          size;
        U32          vertex_stride;
        void const*  initial_data;
        U32          max_frames_in_flight;
        bool         allow_cpu_updates;

        Buffer_Description() :
        debug_name()
        , usage(Buffer_Usage::Uniform)
        , size(0)
        , vertex_stride(0)
        , initial_data(nullptr)
        , max_frames_in_flight(2)
        , allow_cpu_updates(false)
        {}

        Buffer_Description( U8_String& debug_name, U64 size, U32 stride, Buffer_Usage usage = Uniform) :
        debug_name(debug_name)
        , usage(usage)
        , size(size)
        , vertex_stride(stride)
        , initial_data(nullptr)
        , max_frames_in_flight(2)
        , allow_cpu_updates(false)
        {}

        Buffer_Description( const Buffer_Description& description ) :
        debug_name(description.debug_name)
        , usage(description.usage)
        , size(description.size)
        , vertex_stride(description.vertex_stride)
        , initial_data(description.initial_data)
        , max_frames_in_flight(description.max_frames_in_flight)
        , allow_cpu_updates(description.allow_cpu_updates)
        {}

        void Set_Data(void* data, size_t byte_size) { initial_data = data; size = byte_size; }

        VkBufferUsageFlags Usage_Convert_To_Vulkan() const {
            VkBufferUsageFlags flags = 0;
            if( usage & Vertex ) {
                flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            }
            if( usage & Uniform ) {
                flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            }
            if( usage & Index32 ) {
                flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            }
            if( usage & Storage ) {
                flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            }
            if( usage & Indirect ) {
                flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            }
            if( usage & Upload ) {
                flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            }

            return flags;
        }

        private:
    };

    Vk_Buffer(Allocator* alloc, Vk_Render* Render, const Buffer_Description& description);
    ~Vk_Buffer();

    // Description & Core Allocator
    //
    Buffer_Description description;
    Allocator* allocator;

    // GPU Buffer Resources
    //
    VkBuffer buffer;
    VkDeviceAddress buffer_address;
    VmaAllocation buffer_allocation;
    VmaAllocator vma_allocator_copy;                // Cached for destructor cleanup

    // Memory Properties (Essential for Barriers & Optimization)
    //
    VkMemoryPropertyFlags memory_properties;
    bool is_host_coherent;                          // Skip vkFlushMappedMemoryRanges?
    bool is_host_visible;                           // CPU can access?
    bool is_device_local;                           // GPU-optimized?

    // CPU Mapping (For Host-Visible Buffers)
    //
    void* cpu_mapped_ptr;                           // Persistent mapping, nullptr if device-only
    bool is_persistently_mapped;

    // Frame Synchronization (Essential for Multi-Frame Safety)
    //
    struct Frame_State {
        U64 frame_number;
        bool gpu_work_pending;
        U64 last_write_time_ns;
    };

    dyn_vector<Frame_State> frame_states;

    // Staging Buffer (For Device-Local Initial Data & GPU Transfers)
    //
    VkBuffer staging_buffer;
    VmaAllocation staging_buffer_allocation;
    void* staging_cpu_ptr;
    bool owns_staging_buffer;

    // Statistics & Debugging
    //
    struct Buffer_Statistics {
        U64 total_writes;
        U64 total_gpu_transfers;
        U64 last_access_frame;
    };

    Buffer_Statistics stats;

    // Accessors
    //
    VkBuffer        Get_Buffer() const { return buffer; }
    VkDeviceAddress Get_Buffer_Device_Address() const { return buffer_address; }
    VmaAllocation   Get_Vma_Allocation() const { return buffer_allocation; }
    U64             Get_Size() const { return description.size; }

    // Buffer modification
    //
    void Upload_Data_To_Buffer(Vk_Render *render, dyn_vector<T>& data);

private:
    void Create_Staging_Buffer(Vk_Render *render, U64 size);
    Vk_Buffer& operator=(const Vk_Buffer& b) = delete;
    Vk_Buffer(const Vk_Buffer&) = delete;
};

class Command_Buffer_Guard {
private:
    VkDevice        device;
    VkCommandPool   command_pool;
    VkQueue         queue;
    VkCommandBuffer cmd_buffer;
    VkFence         fence;
    bool            executed;
public:
    Command_Buffer_Guard(VkDevice dev, VkCommandPool pool, VkQueue q) :
        device(dev), command_pool(pool), queue(q), executed(false) {

        VkFenceCreateInfo fence_one_time_ci {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
        };
        Vk::Check(vkCreateFence(device, &fence_one_time_ci, nullptr, &fence));

        VkCommandBufferAllocateInfo alloc_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        vkAllocateCommandBuffers(device, &alloc_info, &cmd_buffer);

        VkCommandBufferBeginInfo begin_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        vkBeginCommandBuffer(cmd_buffer, &begin_info);
    }

    // Prevent copying
    Command_Buffer_Guard(const Command_Buffer_Guard&) = delete;
    Command_Buffer_Guard& operator=(const Command_Buffer_Guard&) = delete;

    // Allow moving
    Command_Buffer_Guard(Command_Buffer_Guard&& other) noexcept
        : device(other.device), command_pool(other.command_pool),
          queue(other.queue), cmd_buffer(other.cmd_buffer), executed(other.executed) {
        other.executed = true; // Prevent double cleanup
    }

    // Get the command buffer for recording commands
    VkCommandBuffer Get() const { return cmd_buffer; }
    operator VkCommandBuffer() const { return cmd_buffer; }

    // Manual submit if you need to do something before cleanup
    void Submit_And_Wait() {
        if (executed) return;
        executed = true;

        VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd_buffer
        };

        vkQueueSubmit(queue, 1, &submit_info, fence);
        Vk::Check(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
    }

    ~Command_Buffer_Guard() {
        if( !executed ) {
            vkEndCommandBuffer(cmd_buffer);
            Submit_And_Wait();
        }
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, command_pool, 1, &cmd_buffer);
    }
};

class Vk_Render {
    public:

    Vk_Render() {}
    ~Vk_Render() {}

    // Memory
    //
    Allocator* Alloc;

    static const U32        max_frames_in_flight { 2 };
    U32                     image_index { 0 };
    U32                     frame_index { 0 };
    VkInstance              instance {VK_NULL_HANDLE};
    VkDevice                device {VK_NULL_HANDLE};
    VkPhysicalDevice        physical_device {VK_NULL_HANDLE};
    VkQueue                 queue {VK_NULL_HANDLE};
    bool                    update_swapchain {false};
    VkSwapchainKHR          swapchain {VK_NULL_HANDLE};
    VkCommandPool           command_pool {VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debug_messenger{ VK_NULL_HANDLE };
    VkImage                 depth_image;
	VkFormat                depth_format{ VK_FORMAT_UNDEFINED };
    VmaAllocator            allocator {VK_NULL_HANDLE};
    VmaAllocation           depth_image_allocation;
    VkImageView             depth_image_view;
    dyn_vector<VkImage>     swapchain_images;
    dyn_vector<VkImageView> swapchain_image_views;
    VkCommandBuffer         command_buffers[max_frames_in_flight];
    VkFence                 fences[max_frames_in_flight];
    VkSemaphore             present_semaphores[max_frames_in_flight];
    dyn_vector<VkSemaphore> render_semaphores;

    // Shaders
    Shader_Data shader_data{};
    struct Shader_Data_Buffer {
        VmaAllocation allocation{ VK_NULL_HANDLE };
        VkBuffer buffer{ VK_NULL_HANDLE };
    };
    Shader_Data_Buffer shader_data_buffers[max_frames_in_flight];

    // Slang
    //
    Slang::ComPtr<slang::IGlobalSession> slang_global_session;
    std::array<slang::TargetDesc, 1> slang_target;
    std::array<slang::CompilerOptionEntry, 1> slang_options;
    slang::SessionDesc slang_session_description;

    // Bindless resource table
    Vk_Bindless_Table bindless_table;

    // Camera and rotations
    //
    glm::vec3 cam_pos{ 0.0f, 0.0f, -6.0f };
    dyn_vector<glm::vec3> object_rotations;

    // Window size.
    api_window window;

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    // Current rendering state (for binding)
    VkPipeline current_pipeline { VK_NULL_HANDLE };
    VkPipelineLayout current_pipeline_layout { VK_NULL_HANDLE };

    static inline void Check(VkResult result, const std::source_location& loc = std::source_location::current()) {
        if (result != VK_SUCCESS) {
            printf("Vulkan call returned an error at %s:%u (%s) => %d\n", loc.file_name(), (unsigned)loc.line(), loc.function_name(), (int)result);
            exit(result);
        }
    }

    inline void Check_Swapchain(VkResult result, const std::source_location& loc = std::source_location::current()) {
        if (result < VK_SUCCESS) {
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                update_swapchain = true;
                return;
            }
            printf("Vulkan call returned an error at %s:%u (%s) => %d\n", loc.file_name(), (unsigned)loc.line(), loc.function_name(), (int)result);
            exit(result);
        }
    }

    static inline void Check(bool result, const std::source_location& loc = std::source_location::current()) {
        if (!result) {
            printf("Vulkan call returned an error at %s:%u (%s)\n", loc.file_name(), (unsigned)loc.line(), loc.function_name());
            exit(EXIT_FAILURE);
        }
    }

    void Init(F64 w, F64 h, Allocator* Alloc);

    // ========================================================================
    // Hybrid Frame Loop API
    // ========================================================================

    // High-level: Simple frame loop for basic usage
    void Render_Loop();

    // Low-level: Individual frame operations for advanced usage
    void Begin_Frame();   // Wait for fence, acquire swapchain image, begin recording
    void End_Frame();     // End recording, submit, present

    // Record callback version - convenient for simple rendering
    void Record_Frame(std::function<void()> record_callback);

    VkDevice Get_Device() const { return device; }
    const dyn_vector<VkImage>*     Get_Swapchain_Images()      const { return &swapchain_images; }
    const dyn_vector<VkImageView>* Get_Swapchain_Image_Views() const { return &swapchain_image_views; }

    VkImage     Get_Current_Swapchain_Image() const { return swapchain_images[image_index]; }
    VkImageView Get_Current_Swapchain_Image_View() const { return swapchain_image_views[image_index]; }

    U32  Get_Current_Frame_Idx() const { return frame_index; }
    void Update_Frame_Idx() { frame_index = (frame_index + 1) % max_frames_in_flight; }

    VkCommandBuffer Get_Current_Cmd_Buffer() const { return command_buffers[frame_index]; }

    Slang::ComPtr<slang::IGlobalSession> Get_Slang_Global_Session() const { return slang_global_session; }
    slang::SessionDesc&                  Get_Slang_Session_Description() { return slang_session_description; }

    VkFormat Get_Swapchain_Image_Format() const { return VK_FORMAT_B8G8R8A8_SRGB; }
    VkFormat Get_Depth_Image_Format() const { return depth_format; }

    VkQueue Get_Queue() const { return queue; }

    VkCommandPool Get_Command_Pool() const { return command_pool; }

    VmaAllocator Get_Vma_Allocator() { return allocator; }

    Vk_Bindless_Table& Get_Bindless_Table() { return bindless_table; }

    VkPhysicalDeviceMemoryProperties2 Get_Device_Memory_Properties() {
        VkPhysicalDeviceMemoryProperties2 properties{};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        vkGetPhysicalDeviceMemoryProperties2(physical_device, &properties);

        return properties;
    }

    VkCommandBuffer Begin_Single_Time_Command();
    VkCommandBuffer End_Single_Time_Command();

    void Begin_Command_Buffer();
    void End_Command_Buffer();

    // Rendering Commands (Draw Calls)
    //
    void Begin_Rendering(VkClearColorValue clear_color = {0.0f, 0.0f, 0.0f, 1.0f});
    void End_Rendering();

    void Bind_Pipeline(VkPipeline pipeline, VkPipelineLayout pipeline_layout);
    void Bind_Descriptor_Sets(VkPipelineLayout pipeline_layout, U32 set_index,
                              U32 descriptor_set_count, const VkDescriptorSet* descriptor_sets);

    void Bind_Vertex_Buffer(VkBuffer buffer, VkDeviceSize offset = 0);
    void Bind_Index_Buffer(VkBuffer buffer, VkDeviceSize offset = 0, VkIndexType index_type = VK_INDEX_TYPE_UINT32);

     void Set_Viewport(F32 x = 0, F32 y = 0, F32 width = 0, F32 height = 0,
                      F32 min_depth = 0.0f, F32 max_depth = 1.0f);
    void Set_Scissor(i32 x = 0, i32 y = 0, u32 width = 0, u32 height = 0);

    void Draw(u32 vertex_count, u32 instance_count = 1, u32 first_vertex = 0, u32 first_instance = 0);
    void Draw_Indexed(u32 index_count, u32 instance_count = 1, u32 first_index = 0, i32 vertex_offset = 0, u32 first_instance = 0);

    void Push_Constants(VkPipelineLayout pipeline_layout, VkShaderStageFlags stage_flags,
                       u32 offset, u32 size, const void* data);
};

#if defined(__clang__)
# pragma clang diagnostic pop
#elif defined(__GNUC__)
# pragma GCC   diagnostic pop
#endif

#endif // VULKAN_RENDER_H
