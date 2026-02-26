#ifndef _VULKAN_HANDLE_HPP_
#define _VULKAN_HANDLE_HPP_

template<typename Handle_Type, typename Deleter>
class Vulkan_Handle {
public:
    // Construction
    explicit Vulkan_Handle(Handle_Type handle = VK_NULL_HANDLE) noexcept
        : m_handle(handle) {}

    // RAII: acquire resource
    Vulkan_Handle(Handle_Type handle, VkDevice device) noexcept
        : m_handle(handle), m_device(device) {}

    // Destructor: release resource
    ~Vulkan_Handle() {
        if (m_handle != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
            Deleter{}(m_device, m_handle, nullptr);
        }
    }

    // Move-only: transfer ownership
    Vulkan_Handle(Vulkan_Handle&& other) noexcept
        : m_handle(other.m_handle), m_device(other.m_device) {
        other.m_handle = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    Vulkan_Handle& operator=(Vulkan_Handle&& other) noexcept {
        if (this != &other) {
            // Release current
            if (m_handle != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
                Deleter{}(m_device, m_handle, nullptr);
            }
            // Acquire new
            m_handle = other.m_handle;
            m_device = other.m_device;
            other.m_handle = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    // Non-copyable
    Vulkan_Handle(const Vulkan_Handle&) = delete;
    Vulkan_Handle& operator=(const Vulkan_Handle&) = delete;

    // Access
    Handle_Type Get() const noexcept { return m_handle; }
    explicit operator bool() const noexcept { return m_handle != VK_NULL_HANDLE; }

    Handle_Type* Get_Ptr() noexcept { return &m_handle; }

    // Release ownership (for handing to Vulkan)
    Handle_Type Release() noexcept {
        Handle_Type tmp = m_handle;
        m_handle = VK_NULL_HANDLE;
        return tmp;
    }

private:
    Handle_Type m_handle = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;  // Needed for deletion
};

struct Buffer_Deleter {
    void operator()(VkDevice d, VkBuffer h, const VkAllocationCallbacks* p) const {
        vkDestroyBuffer(d, h, p);
    }
};

struct Pipeline_Deleter {
    void operator()(VkDevice d, VkPipeline h, const VkAllocationCallbacks* p) const {
        vkDestroyPipeline(d, h, p);
    }
};

struct Swapchain_Deleter {
    void operator()(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* p) const {
        vkDestroySwapchainKHR(device, swapchain, p);
    }
};

struct Semaphore_Deleter {
    void operator()(VkDevice d, VkSemaphore h, const VkAllocationCallbacks* p) const {
        vkDestroySemaphore(d, h, p);
    }
};

struct Fence_Deleter {
    void operator()(VkDevice d, VkFence h, const VkAllocationCallbacks* p) const {
        vkDestroyFence(d, h, p);
    }
};

struct Image_View_Deleter {
    void operator()(VkDevice d, VkImageView h, const VkAllocationCallbacks* p) const {
        vkDestroyImageView(d, h, p);
    }
};

struct Sampler_Deleter {
    void operator()(VkDevice d, VkSampler sampler, const VkAllocationCallbacks* p) const {
        vkDestroySampler(d, sampler, p);
    }
};

struct Shader_Module_Deleter {
    void operator()(VkDevice d, VkShaderModule m, const VkAllocationCallbacks* p) const {
        vkDestroyShaderModule(d, m, p);
    }
};

struct Descriptor_Set_Layout_Deleter {
    void operator()(VkDevice d, VkDescriptorSetLayout dsl, const VkAllocationCallbacks* p) const {
        vkDestroyDescriptorSetLayout(d, dsl, p);
    }
};

struct Descriptor_Pool_Deleter {
    void operator()(VkDevice d, VkDescriptorPool dp, const VkAllocationCallbacks* p) const {
        vkDestroyDescriptorPool(d, dp, p);
    }
};

struct Pipeline_Layout_Deleter {
    void operator()(VkDevice d, VkPipelineLayout l, const VkAllocationCallbacks* p) const {
        vkDestroyPipelineLayout(d, l, p);
    }
};

using Buffer_Handle                = Vulkan_Handle<VkBuffer, Buffer_Deleter>;
//using Pipeline_Handle            = Vulkan_Handle<VkPipeline, Pipeline_Deleter>;
using Pipeline_Layout_Handle       = Vulkan_Handle<VkPipelineLayout, Pipeline_Layout_Deleter>;
using Swapchain_Handle             = Vulkan_Handle<VkSwapchainKHR, Swapchain_Deleter>;
using Semaphore_Handle             = Vulkan_Handle<VkSemaphore, Semaphore_Deleter>;
using Fence_Handler                = Vulkan_Handle<VkFence, Fence_Deleter>;
using Image_View_Handle            = Vulkan_Handle<VkImageView, Image_View_Deleter>;
using Shader_Module_Handle         = Vulkan_Handle<VkShaderModule, Shader_Module_Deleter>;
using Descriptor_Set_Layout_Handle = Vulkan_Handle<VkDescriptorSetLayout, Descriptor_Set_Layout_Deleter>;
using Descriptor_Pool_Handle       = Vulkan_Handle<VkDescriptorPool, Descriptor_Pool_Deleter>;
using Sampler_Handle               = Vulkan_Handle<VkSampler, Sampler_Deleter>;

#endif // _VULKAN_HANDLE_HPP_