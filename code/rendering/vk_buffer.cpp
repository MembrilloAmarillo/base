#include "vk_buffer.h"

Buffer::Buffer(const Device& device, const Buffer_Create_Info& info)
    : m_size(info.size)
    , m_usage_category(info.usage_category)
    , m_permanently_mapped(info.mapped_permanently)
    , m_device(&device)
    , m_vma_allocator(device.Get_Vma_Allocator()) {

    // Validate inputs
    if (info.size == 0) {
        throw std::runtime_error("Buffer size cannot be zero");
    }

    m_permanently_mapped = device.Is_UMA();

    // Create the buffer via VMA
    Create_Buffer(info);

    // Handle initial data upload if provided
    if (info.initial_data && info.size > 0) {
        // For host-visible buffers: direct write
        if (m_mapped_data) {
            //std::memcpy(m_mapped_data, info.initial_data, info.size);
            //Flush();  // Ensure visible to GPU
            Upload_Data(info.initial_data, info.size);
        }
        // For device-local buffers: need staging (deferred to explicit Upload_Data)
        else {
            // Cannot upload immediately without command buffer
            // Option: create temporary staging buffer, or require explicit upload
            // For now: throw or warn that initial_data requires host-visible
            throw std::runtime_error(
                "initial_data requires host-visible buffer or explicit Upload_Data with command buffer");
        }
    }
}

void Buffer::Create_Buffer(const Buffer_Create_Info& info) {
    // Derive Vulkan usage flags from category + additional flags
    VkBufferUsageFlags usage = Derive_Vulkan_Usage(m_usage_category)
                             | info.additional_vulkan_usage;

    // Add transfer flags for staging/upload support
    if (m_usage_category == Buffer_Usage::Staging) {
        usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }
    if (m_usage_category == Buffer_Usage::Vertex ||
        m_usage_category == Buffer_Usage::Index ||
        m_usage_category == Buffer_Usage::Uniform ||
        m_usage_category == Buffer_Usage::Storage) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;  // Can receive uploads
    }

    // Buffer create info
    VkBufferCreateInfo buffer_ci {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = m_size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,  // Simpler, usually sufficient
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };

    // VMA allocation create info
    VmaAllocationCreateInfo alloc_ci{};
    alloc_ci.usage = Derive_VMA_Usage(m_usage_category);
    alloc_ci.flags = Derive_VMA_Flags(m_usage_category, m_permanently_mapped);
    alloc_ci.requiredFlags = 0;
    alloc_ci.preferredFlags = 0;
    alloc_ci.memoryTypeBits = 0;  // Let VMA decide
    alloc_ci.pool = VK_NULL_HANDLE;
    alloc_ci.pUserData = nullptr;

    // Create buffer with VMA
    VkBuffer raw_buffer = VK_NULL_HANDLE;
    VkResult result = vmaCreateBuffer(
        m_vma_allocator,
        &buffer_ci,
        &alloc_ci,
        &raw_buffer,
        &m_allocation,
        &m_allocation_info
    );

    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string("vmaCreateBuffer failed: ") + std::to_string(result));
    }

    // Wrap in RAII handle
    m_buffer = Vma_Allocated_Buffer(raw_buffer, m_vma_allocator, m_allocation);

    // Handle permanent mapping
    if (m_permanently_mapped && m_allocation_info.pMappedData) {
        // VMA already mapped it (VMA_ALLOCATION_CREATE_MAPPED_BIT)
        m_mapped_data = m_allocation_info.pMappedData;
    } else if (m_permanently_mapped) {
        // Need to map explicitly
        result = vmaMapMemory(m_vma_allocator, m_allocation, &m_mapped_data);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to permanently map buffer");
        }
    }
}

Buffer& Buffer::operator=(Buffer&& other) {

    this->m_allocation = other.m_allocation;
    this->m_allocation_info = other.m_allocation_info;
    this->m_size = other.m_size;
    this->m_usage_category = other.m_usage_category;
    this->m_mapped_data = other.m_mapped_data;
    this->m_permanently_mapped = other.m_permanently_mapped;
    this->m_device = other.m_device;
    this->m_vma_allocator = other.m_vma_allocator;
    this->m_debug_name = other.m_debug_name;

    other.m_size = 0;
    other.m_device = nullptr;
    other.m_debug_name = nullptr;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_allocation_info = {};

    return *this;
}

Buffer::Buffer(Buffer&& other) :
    m_allocation(other.m_allocation)
    , m_allocation_info(other.m_allocation_info)
    , m_size(other.m_size)
    , m_usage_category(other.m_usage_category)
    , m_mapped_data(other.m_mapped_data)
    , m_permanently_mapped(other.m_permanently_mapped)
    , m_device(other.m_device)
    , m_vma_allocator(other.m_vma_allocator)
    , m_debug_name(other.m_debug_name)
 {
    m_buffer = std::move(other.m_buffer);
    other.m_size = 0;
    other.m_device = nullptr;
    other.m_debug_name = nullptr;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_allocation_info = {};
}

// CPU access (if applicable for usage category)
void* Buffer::Map() {
    if (m_mapped_data) {
        return m_mapped_data;  // Already mapped (permanent or previously mapped)
    }

    if (!m_allocation) {
        throw std::runtime_error("Cannot map unallocated buffer");
    }

    VkResult result = vmaMapMemory(m_vma_allocator, m_allocation, &m_mapped_data);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to map buffer memory");
    }

    return m_mapped_data;
}

void Buffer::Unmap() {
    if (!m_mapped_data) {
        return;  // Not mapped, nothing to do
    }

    // Only unmap if not permanently mapped
    if (!m_permanently_mapped) {
        vmaUnmapMemory(m_vma_allocator, m_allocation);
        m_mapped_data = nullptr;
    }
    // If permanently mapped, keep m_mapped_data valid
}

void Buffer::Upload_Data(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    // Validation
    if (offset > m_size || size > m_size - offset) {
        throw std::runtime_error("Upload range exceeds buffer size");
    }

    if (!data) {
        throw std::runtime_error("Null data pointer");
    }

    // Ensure we have mapped access
    void* mapped = m_mapped_data;
    if (!mapped) {
        // Temporary map for non-permanently-mapped buffers
        VkResult result = vmaMapMemory(m_vma_allocator, m_allocation, &mapped);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to map buffer for upload");
        }
    }

    // Perform copy
    std::memcpy(static_cast<U8*>(mapped) + offset, data, size);

    // Flush if not HOST_COHERENT (VMA usually handles this, but explicit is safer)
    // Note: VMA with HOST_SEQUENTIAL_WRITE flag typically handles coherence
    //vmaGetAllocationInfo(m_vma_allocator, m_allocation, &m_allocation_info);
    // Check if we need manual flush based on memory type

    // Unmap if we did temporary mapping
    if (!m_mapped_data) {
        vmaUnmapMemory(m_vma_allocator, m_allocation);
    }
}

void Buffer::Upload_To_Buffer(const Buffer& destination, VkCommandBuffer cmd) {
    // This buffer must be TRANSFER_SRC, destination must be TRANSFER_DST
    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = std::min(m_size, destination.m_size);

    vkCmdCopyBuffer(cmd, m_buffer.Get_Handle(), destination.m_buffer.Get_Handle(),
                    1, &copy_region);
}

// GPU-side copy
void Buffer::Copy_To(const Buffer& destination, VkCommandBuffer cmd,
                     VkDeviceSize size, VkDeviceSize src_offset,
                     VkDeviceSize dst_offset) {
    VkDeviceSize copy_size = (size == VK_WHOLE_SIZE)
        ? (m_size - src_offset)
        : size;

    // Clamp to destination bounds
    copy_size = std::min(copy_size, destination.m_size - dst_offset);

    VkBufferCopy copy_region{};
    copy_region.srcOffset = src_offset;
    copy_region.dstOffset = dst_offset;
    copy_region.size = copy_size;

    vkCmdCopyBuffer(cmd, m_buffer.Get_Handle(), destination.m_buffer.Get_Handle(),
                    1, &copy_region);
}

// Barrier helpers for render graph integration
void Buffer::Record_Barrier(VkCommandBuffer cmd,
                           VkPipelineStageFlags src_stage,
                           VkPipelineStageFlags dst_stage,
                           VkAccessFlags src_access,
                           VkAccessFlags dst_access,
                           uint32_t src_queue_family,
                           uint32_t dst_queue_family) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.srcQueueFamilyIndex = src_queue_family;
    barrier.dstQueueFamilyIndex = dst_queue_family;
    barrier.buffer = Get_Handle();
    barrier.offset = 0;
    barrier.size = m_size;

    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0,
                         0, nullptr,
                         1, &barrier,
                         0, nullptr);
}

// API-agnostic descriptor info for VK_Render population
VkDescriptorBufferInfo Buffer::Get_Descriptor_Info(VkDeviceSize offset,
                                                  VkDeviceSize range) const {
    VkDescriptorBufferInfo info{};
    info.buffer = m_buffer.Get_Handle();
    info.offset = offset;
    info.range = (range == VK_WHOLE_SIZE) ? (m_size - offset) : range;
    return info;
}

// Address for ray tracing / buffer device address
VkDeviceAddress Buffer::Get_Device_Address() const {
    VkBufferDeviceAddressInfo addr_info{};
    addr_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addr_info.buffer = m_buffer.Get_Handle();
    return vkGetBufferDeviceAddress(m_device->Get_Handle(), &addr_info);
}

VkBufferUsageFlags Buffer::Derive_Vulkan_Usage(Buffer_Usage category) const {
    switch (category) {
        case Buffer_Usage::Vertex:           return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case Buffer_Usage::Index:            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case Buffer_Usage::Uniform:          return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        case Buffer_Usage::Storage:          return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        case Buffer_Usage::Staging:          return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        case Buffer_Usage::Readback:         return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case Buffer_Usage::Dynamic_Vertex:   return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case Buffer_Usage::Dynamic_Index:    return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        default:                             return 0;
    }
}

VmaMemoryUsage Buffer::Derive_VMA_Usage(Buffer_Usage category) const {
    switch (category) {
        case Buffer_Usage::Vertex:
        case Buffer_Usage::Index:
        case Buffer_Usage::Uniform:
        case Buffer_Usage::Storage:
            return VMA_MEMORY_USAGE_GPU_ONLY;  // Fast GPU access

        case Buffer_Usage::Staging:
            return VMA_MEMORY_USAGE_CPU_ONLY;  // CPU write, GPU read via copy

        case Buffer_Usage::Readback:
            return VMA_MEMORY_USAGE_GPU_TO_CPU;  // GPU write, CPU read

        case Buffer_Usage::Dynamic_Vertex:
        case Buffer_Usage::Dynamic_Index:
            return VMA_MEMORY_USAGE_CPU_TO_GPU;  // Frequent CPU updates

        default:
            return VMA_MEMORY_USAGE_UNKNOWN;
    }
}

VmaAllocationCreateFlags Buffer::Derive_VMA_Flags(Buffer_Usage category, bool mapped) const {
    VmaAllocationCreateFlags flags = 0;

    if (mapped) {
        flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    // Host access for CPU-visible categories
    switch (category) {
        case Buffer_Usage::Staging:
            flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            break;
        case Buffer_Usage::Readback:
            flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            break;
        case Buffer_Usage::Dynamic_Vertex:
        case Buffer_Usage::Dynamic_Index:
            flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            break;
        default:
            break;
    }

    // Dedicated memory for large buffers (optional heuristic)
    if (m_size > 256 * 1024 * 1024) {  // > 256 MB
        flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    return flags;
}

Vma_Allocated_Buffer::Vma_Allocated_Buffer(VkBuffer buffer, VmaAllocator allocator, VmaAllocation allocation) :
    m_buffer(buffer)
    , m_allocator(allocator)
    , m_allocation(allocation)
 { }

Vma_Allocated_Buffer::~Vma_Allocated_Buffer() {
    if (m_buffer != VK_NULL_HANDLE && m_allocator != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    }
}

Vma_Allocated_Buffer::Vma_Allocated_Buffer(Vma_Allocated_Buffer&& other) noexcept :
    m_buffer(other.m_buffer)
    , m_allocator(other.m_allocator)
    , m_allocation(other.m_allocation)
{
    other.m_buffer     = VK_NULL_HANDLE;
    other.m_allocator  = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
}

Vma_Allocated_Buffer& Vma_Allocated_Buffer::operator=(Vma_Allocated_Buffer&& other) noexcept {
    if (this != &other) {  // Self-assignment check
        // Destroy existing resources first
        if (m_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        }

        // Steal from other
        m_buffer = other.m_buffer;
        m_allocator = other.m_allocator;
        m_allocation = other.m_allocation;

        // Null out other
        other.m_buffer = VK_NULL_HANDLE;
        other.m_allocator = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
    }
    return *this;
}