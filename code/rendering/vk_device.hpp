#ifndef _VK_DEVICE_HPP_
#define _VK_DEVICE_HPP_

#include <vulkan/vulkan.h>
#include <set>
#include <vector>
#include <optional>
#include <cstdint>
#include <functional>
#include <string>

#include "../util/types.h"
#include "../third-party/vk_mem_alloc.h"

// Forward declarations
class Instance;
struct Allocator;

// Queue family indices helper
struct Queue_Family_Indices {
    std::optional<U32> graphics_family;
    std::optional<U32> present_family;
    std::optional<U32> compute_family;
    std::optional<U32> transfer_family;

    bool Is_Complete() const {
        return graphics_family.has_value() && present_family.has_value();
    }
};

inline const char* VkResult_To_String(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
        default: return "UNKNOWN_VK_RESULT";
    }
}

// Device creation configuration
struct Device_Create_Info {
    VkSurfaceKHR surface = VK_NULL_HANDLE;  // For present queue selection
    std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    void* p_next_chain = nullptr;  // For device features chain (Vulkan 1.3+)
    bool enable_vma = true;  // Initialize Vulkan Memory Allocator

    Allocator* allocator = nullptr;
};

class Device {
public:
    // Constructors
    Device() = default;
    Device(const Instance& instance, const Device_Create_Info& info);

    // Move semantics
    Device(Device&& other) noexcept;
    Device& operator=(Device&& other) noexcept;

    // Non-copyable
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Destructor
    ~Device();

    // Accessors
    VkDevice Get_Handle() const noexcept { return m_device; }
    VkPhysicalDevice Get_Physical_Device() const noexcept { return m_physical_device; }
    bool Is_Valid() const noexcept { return m_device != VK_NULL_HANDLE; }
    void Wait_Idle() const noexcept { if (m_device != VK_NULL_HANDLE) { vkDeviceWaitIdle(m_device); } }

    bool Is_UMA() const noexcept { return is_uma; }

    // Queue accessors
    VkQueue Get_Graphics_Queue() const noexcept { return m_graphics_queue; }
    VkQueue Get_Present_Queue() const noexcept { return m_present_queue; }
    VkQueue Get_Compute_Queue() const noexcept { return m_compute_queue; }
    VkQueue Get_Transfer_Queue() const noexcept { return m_transfer_queue; }

    uint32_t Get_Graphics_Queue_Family() const noexcept { return m_graphics_queue_family; }
    uint32_t Get_Present_Queue_Family() const noexcept { return m_present_queue_family; }

    // Command pool management
    VkCommandPool Get_Graphics_Command_Pool() const noexcept { return m_graphics_command_pool; }
    VkCommandPool Create_Command_Pool(uint32_t queue_family_index, VkCommandPoolCreateFlags flags = 0);
    void Destroy_Command_Pool(VkCommandPool pool);

    // Command buffer allocation
    VkCommandBuffer Allocate_Command_Buffer(VkCommandPool pool, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    void Free_Command_Buffer(VkCommandBuffer buffer, VkCommandPool pool);

    // Immediate command submission (convenience)
    void Submit_Immediate_Commands(std::function<void(VkCommandBuffer)>&& record_func);

    // VMA integration
    VmaAllocator Get_Vma_Allocator() const noexcept { return m_vma_allocator; }
    bool Vma_Enabled() const noexcept { return m_vma_allocator != nullptr; }

    // Utility queries
    VkPhysicalDeviceProperties Get_Device_Properties() const;
    VkPhysicalDeviceMemoryProperties Get_Memory_Properties() const;
    uint32_t Find_Memory_Type(uint32_t type_filter, VkMemoryPropertyFlags properties) const;

    // Static helpers for physical device selection
    static Queue_Family_Indices Find_Queue_Families(VkPhysicalDevice device, VkSurfaceKHR surface);
    static bool Check_Device_Extension_Support(VkPhysicalDevice device, const std::vector<const char*>& extensions);
    static int Score_Physical_Device(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& extensions);

private:

    Allocator* m_mem_allocator = nullptr;

    // Vulkan handles
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;

    // Queues
    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    VkQueue m_present_queue = VK_NULL_HANDLE;
    VkQueue m_compute_queue = VK_NULL_HANDLE;
    VkQueue m_transfer_queue = VK_NULL_HANDLE;

    // Queue family indices
    U32 m_graphics_queue_family = UINT32_MAX;
    U32 m_present_queue_family = UINT32_MAX;
    U32 m_compute_queue_family = UINT32_MAX;
    U32 m_transfer_queue_family = UINT32_MAX;

    // Command pools (one per family for primary use)
    VkCommandPool m_graphics_command_pool = VK_NULL_HANDLE;

    VmaAllocator m_vma_allocator = nullptr;

    bool is_uma = false;

    // Instance reference (non-owning, for context)
    const Instance* m_instance = nullptr;

    // Internal helpers
    void Create_Logical_Device(const Device_Create_Info& info);
    void Retrieve_Queues();
    void Create_Graphics_Command_Pool();
    void Initialize_Vma(const Instance& instance);
    void Shutdown_Vma();
};

#endif // _VK_DEVICE_HPP_
