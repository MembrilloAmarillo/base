#ifndef INSTANCE_H
#define INSTANCE_H

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <source_location>


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

class Instance {
public:
    // Builder pattern for clean configuration
    struct Create_Info {
        const char* app_name = "Vulkan Application";
        uint32_t app_version = VK_MAKE_VERSION(1, 0, 0);
        uint32_t api_version = VK_API_VERSION_1_3;
        std::vector<const char*> extensions;
        std::vector<const char*> layers;
        bool enable_validation = true;
        bool enable_debug_messenger = true;
        Allocator* mem_allocator = nullptr;
    };

    // Constructors
    Instance() = default;
    explicit Instance(const Create_Info& info);

    // Move semantics
    Instance(Instance&& other) noexcept;
    Instance& operator=(Instance&& other) noexcept;

    // Non-copyable
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    // Destructor
    ~Instance();

    // Accessors
    VkInstance Get_Handle() const noexcept { return m_instance; }
    bool Is_Valid() const noexcept { return m_instance != VK_NULL_HANDLE; }
    bool Validation_Enabled() const noexcept { return m_validation_enabled; }

    // Query physical devices
    std::vector<VkPhysicalDevice> Enumerate_Physical_Devices() const;

    // Static helpers for setup
    static std::vector<const char*> Get_Required_Extensions(bool enable_validation);
    static bool Check_Validation_Layer_Support();
    static bool Check_Extension_Support(const char* extension_name);

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    bool m_validation_enabled = false;
    bool m_debug_messenger_enabled = false;

    Allocator* m_mem_allocator = nullptr;

    // Internal helpers
    void Create_Instance(const Create_Info& info);
    void Setup_Debug_Messenger();
    void Destroy_Debug_Messenger();

    // Debug callback
    static VKAPI_ATTR VkBool32 VKAPI_CALL Debug_Callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data);
};

#endif // INSTANCE_H
