#ifndef _VULKAN_BUFFER_HPP_
#define _VULKAN_BUFFER_HPP_

#include <vulkan/vulkan.h>
#include "../third-party/vk_mem_alloc.h"  // VMA integration
#include <cstdint>
#include <span>
#include "../util/types.h"

#include "vk_handle.hpp"

class Device;

// Custom RAII handle for VMA-allocated buffers
class Vma_Allocated_Buffer {
public:
    Vma_Allocated_Buffer() = default;
    Vma_Allocated_Buffer(VkBuffer buffer, VmaAllocator allocator, VmaAllocation allocation);
    ~Vma_Allocated_Buffer();

    // Move semantics
    Vma_Allocated_Buffer(Vma_Allocated_Buffer&& other) noexcept;
    Vma_Allocated_Buffer& operator=(Vma_Allocated_Buffer&& other) noexcept;

    // Delete copy
    Vma_Allocated_Buffer(const Vma_Allocated_Buffer&) = delete;
    Vma_Allocated_Buffer& operator=(const Vma_Allocated_Buffer&) = delete;

    // Access
    VkBuffer Get_Handle() const noexcept { return m_buffer; }
    explicit operator bool() const noexcept { return m_buffer != VK_NULL_HANDLE; }

private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
};

// Buffer usage categories for API-agnostic interface
enum class Buffer_Usage {
    Vertex,           // GPU-only, vertex data
    Index,            // GPU-only, index data
    Uniform,          // GPU-only, uniform buffer (frequently updated)
    Storage,          // GPU read/write, shader storage
    Staging,          // CPU write, GPU read (transfer source)
    Readback,         // GPU write, CPU read (transfer destination)
    Dynamic_Vertex,   // CPU write, GPU read (every frame)
    Dynamic_Index,    // CPU write, GPU read (every frame)
    Dynamic_Uniform,  // CPU write, GPU read (every frame)
    Dynamic_Storage,  // CPU write, GPU read (every frame)
    Count
};

// Buffer creation parameters
struct Buffer_Create_Info {
    VkDeviceSize size = 0;
    Buffer_Usage usage_category = Buffer_Usage::Staging;
    VkBufferUsageFlags additional_vulkan_usage = 0;  // For specialized cases
    bool mapped_permanently = false;  // Keep CPU pointer valid
    const void* initial_data = nullptr;  // Optional immediate upload
    VmaMemoryUsage vma_usage_override = VMA_MEMORY_USAGE_UNKNOWN;  // Optional override
    VmaAllocationCreateFlags vma_flags_override = 0;               // Optional override
    bool use_vma_flags_override = false;                           // Apply vma_flags_override when true
};

class Buffer {
public:
    // Constructors
    Buffer() = default;
    Buffer(const Device& device, const Buffer_Create_Info& info);

    // Move semantics
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    // Non-copyable
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Destructor
    ~Buffer() = default;  // RAII handles cleanup

    // Handle access
    VkBuffer Get_Handle() const noexcept { return m_buffer.Get_Handle(); }
    VmaAllocation Get_Allocation() const noexcept { return m_allocation; }
    bool Is_Valid() const noexcept { return static_cast<bool>(m_buffer); }

    // Size and properties
    VkDeviceSize Get_Size() const noexcept { return m_size; }
    Buffer_Usage Get_Usage_Category() const noexcept { return m_usage_category; }

    // CPU access (if applicable for usage category)
    void* Map();
    void Unmap();
    bool Is_Mapped() const noexcept { return m_mapped_data != nullptr; }
    void* Get_Mapped_Pointer() const noexcept { return m_mapped_data; }

    // Data operations
    void Upload_Data(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    // For staging-to-device transfers
    void Upload_To_Buffer(const Buffer& destination, VkCommandBuffer cmd);

    // GPU-side copy
    void Copy_To(const Buffer& destination, VkCommandBuffer cmd,
                 VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize src_offset = 0,
                 VkDeviceSize dst_offset = 0);

    // Barrier helpers for render graph integration
    void Record_Barrier(VkCommandBuffer cmd,
                       VkPipelineStageFlags src_stage,
                       VkPipelineStageFlags dst_stage,
                       VkAccessFlags src_access,
                       VkAccessFlags dst_access,
                       uint32_t src_queue_family = VK_QUEUE_FAMILY_IGNORED,
                       uint32_t dst_queue_family = VK_QUEUE_FAMILY_IGNORED);

    // API-agnostic descriptor info for VK_Render population
    VkDescriptorBufferInfo Get_Descriptor_Info(VkDeviceSize offset = 0,
                                                VkDeviceSize range = VK_WHOLE_SIZE) const;

    // Address for ray tracing / buffer device address
    VkDeviceAddress Get_Device_Address() const;

    // Debug/inspection
    const char* Get_Debug_Name() const noexcept { return m_debug_name; }
    void Set_Debug_Name(const char* name);

private:
    // Note: Custom Vulkan_Handle variant that stores VmaAllocator instead of VkDevice
    Vma_Allocated_Buffer m_buffer;  // Custom RAII type, see below

    // Allocation info from VMA
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VmaAllocationInfo m_allocation_info{};

    // Properties
    VkDeviceSize m_size = 0;
    Buffer_Usage m_usage_category = Buffer_Usage::Staging;

    // CPU mapping state
    void* m_mapped_data = nullptr;
    bool m_permanently_mapped = false;

    // References
    const Device* m_device = nullptr;
    VmaAllocator m_vma_allocator = VK_NULL_HANDLE;

    // Debug
    const char* m_debug_name = nullptr;

    // Internal helpers
    void Create_Buffer(const Buffer_Create_Info& info);
    VkBufferUsageFlags Derive_Vulkan_Usage(Buffer_Usage category) const;
    VmaMemoryUsage Derive_VMA_Usage(Buffer_Usage category) const;
    VmaAllocationCreateFlags Derive_VMA_Flags(Buffer_Usage category, bool mapped) const;
};

#endif // _VULKAN_BUFFER_HPP_
