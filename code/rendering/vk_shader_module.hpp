#pragma once

#include <vulkan/vulkan.h>
#include <SPIRV-Reflect/spirv_reflect.h>  // SPIRV-Reflect header
#include <span>
#include <vector>
#include <string>


#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

#include "vk_handle.hpp"

// Forward declarations
class Device;
struct VK_Render;

class Shader_Module {
public:
    // Reflection data extracted from SPIR-V
    struct Reflection_Info {
        VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;

        // Descriptor sets and bindings
        std::vector<SpvReflectDescriptorBinding*> descriptor_bindings;
        std::vector<SpvReflectDescriptorSet*> descriptor_sets;

        // Push constants
        std::vector<SpvReflectBlockVariable*> push_constants;

        // Input/output variables (for vertex attributes and fragment outputs)
        std::vector<SpvReflectInterfaceVariable*> input_variables;
        std::vector<SpvReflectInterfaceVariable*> output_variables;

        // Entry point
        std::string entry_point_name;
    };

    // Constructors
    Shader_Module() = default;
    Shader_Module(const Device& device, std::span<const uint32_t> spirv_code);
    Shader_Module(const Device& device, const char* file_path);

    // Move semantics
    Shader_Module(Shader_Module&& other) noexcept;
    Shader_Module& operator=(Shader_Module&& other) noexcept;

    // Non-copyable
    Shader_Module(const Shader_Module&) = delete;
    Shader_Module& operator=(const Shader_Module&) = delete;

    // Destructor
    ~Shader_Module();

    // Accessors
    VkShaderModule Get_Handle() const noexcept { return m_module.Get(); }
    bool Is_Valid() const noexcept { return static_cast<bool>(m_module); }

    const Reflection_Info& Get_Reflection() const { return m_reflection; }

    // Helper: Convert reflection to Vulkan descriptor set layout bindings
    std::vector<VkDescriptorSetLayoutBinding> Get_Descriptor_Set_Layout_Bindings(uint32_t set_index) const;

    // Helper: Get push constant ranges
    std::vector<VkPushConstantRange> Get_Push_Constant_Ranges() const;

    // Helper: Get vertex input attribute descriptions
    std::vector<VkVertexInputAttributeDescription> Get_Vertex_Input_Attributes(uint32_t binding) const;

    uint32_t Get_Format_Size(VkFormat format) const;
private:

    Slang::ComPtr<slang::IGlobalSession> slang_global_session;
    Slang::ComPtr<slang::ISession> slang_session;
    Slang::ComPtr<slang::IModule> slang_module;
    Slang::ComPtr<ISlangBlob> spirv;

    Shader_Module_Handle m_module;
    Reflection_Info m_reflection;

    // SPIRV-Reflect module (must be destroyed with spvReflectDestroyShaderModule)
    SpvReflectShaderModule m_reflect_module = {};

    const Device* m_device = nullptr;

    void Load_From_Binary(std::span<const uint32_t> code);
    void Load_From_File(const char* path);
    void Perform_Reflection(std::span<const uint32_t> code);
    void Cleanup_Reflection();
};