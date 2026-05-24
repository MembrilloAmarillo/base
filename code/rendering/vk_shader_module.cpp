#include <fstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <array>

#include "vk_device.hpp"
#include "vk_shader_module.hpp"

// ============================================================================
// Constructor: From SPIR-V binary in memory
// ============================================================================
Shader_Module::Shader_Module(const Device& device, std::span<const uint32_t> spirv_code) {
    m_device = &device;
    Load_From_Binary(spirv_code);
    Perform_Reflection(spirv_code);
}

// ============================================================================
// Constructor: From SPIR-V file
// ============================================================================
Shader_Module::Shader_Module(const Device& device, const char* file_path) {
    m_device = &device;
#if HAS_SLANG
    slang::createGlobalSession(slang_global_session.writeRef());
	auto slang_target = std::to_array<slang::TargetDesc>({
			{
				.format = SLANG_SPIRV,
				.profile = slang_global_session->findProfile("spirv_1_4")
			}
		});
	auto slang_options = std::to_array<slang::CompilerOptionEntry>({
		{
			slang::CompilerOptionName::EmitSpirvDirectly,
			{slang::CompilerOptionValueKind::Int, 1}
		}
	});

    slang::SessionDesc slang_session_description = {
		.targets = slang_target.data(),
		.targetCount = SlangInt(slang_target.size()),
		.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
		.compilerOptionEntries = slang_options.data(),
		.compilerOptionEntryCount = U32(slang_options.size())
	};

	slang_global_session->createSession(slang_session_description, slang_session.writeRef());
	slang_module = slang_session->loadModuleFromSource(file_path, file_path, nullptr, nullptr);
	slang_module->getTargetCode(0, spirv.writeRef());

	const uint32_t* spirv_data = static_cast<const uint32_t*>(spirv->getBufferPointer());
    size_t spirv_size = spirv->getBufferSize() / sizeof(uint32_t);

    auto spirv_code = std::span<const U32>(spirv_data, spirv_size);

    Load_From_Binary(spirv_code);
    Perform_Reflection(spirv_code);
#else
    (void)file_path;
    throw std::runtime_error("slang headers not found: cannot compile shader source at runtime");
#endif
}

// ============================================================================
// Move semantics
// ============================================================================
Shader_Module::Shader_Module(Shader_Module&& other) noexcept
    : m_module(std::move(other.m_module))
    , m_reflection(std::move(other.m_reflection))
#if HAS_SPIRV_REFLECT
    , m_reflect_module(other.m_reflect_module)
#endif
    , m_device(other.m_device) {

#if HAS_SPIRV_REFLECT
    other.m_reflect_module = {};
#endif
    other.m_device = nullptr;
}

Shader_Module& Shader_Module::operator=(Shader_Module&& other) noexcept {
    if (this != &other) {
        Cleanup_Reflection();

        m_module = std::move(other.m_module);
        m_reflection = std::move(other.m_reflection);
#if HAS_SPIRV_REFLECT
        m_reflect_module = other.m_reflect_module;
#endif
        m_device = other.m_device;

#if HAS_SPIRV_REFLECT
        other.m_reflect_module = {};
#endif
        other.m_device = nullptr;
    }
    return *this;
}

// ============================================================================
// Destructor
// ============================================================================
Shader_Module::~Shader_Module() {
    Cleanup_Reflection();
}

// ============================================================================
// Load SPIR-V from binary span
// ============================================================================
void Shader_Module::Load_From_Binary(std::span<const uint32_t> spirv_code) {
    if (spirv_code.empty()) {
        throw std::runtime_error("Empty SPIR-V code provided");
    }

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = spirv_code.size() * sizeof(uint32_t);
    create_info.pCode = spirv_code.data();

    VkShaderModule raw_module;
    if (vkCreateShaderModule(m_device->Get_Handle(), &create_info, nullptr, &raw_module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    m_module = Vulkan_Handle<VkShaderModule, Shader_Module_Deleter>(raw_module, m_device->Get_Handle());
}

// ============================================================================
// Load SPIR-V from file
// ============================================================================
void Shader_Module::Load_From_File(const char* file_path) {
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("Failed to open shader file: ") + file_path);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size % sizeof(uint32_t) != 0) {
        throw std::runtime_error("SPIR-V file size must be multiple of 4 bytes");
    }

    std::vector<uint32_t> spirv_code(size / sizeof(uint32_t));
    if (!file.read(reinterpret_cast<char*>(spirv_code.data()), size)) {
        throw std::runtime_error("Failed to read shader file");
    }

    Load_From_Binary(spirv_code);
    Perform_Reflection(spirv_code);
}

// ============================================================================
// Perform SPIRV-Reflect analysis
// ============================================================================
void Shader_Module::Perform_Reflection(std::span<const uint32_t> spirv_code) {
#if HAS_SPIRV_REFLECT
    SpvReflectResult result = spvReflectCreateShaderModule(
        spirv_code.size() * sizeof(uint32_t),
        spirv_code.data(),
        &m_reflect_module
    );

    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        throw std::runtime_error("SPIRV-Reflect failed to parse shader module");
    }

    // Extract shader stage
    m_reflection.stage = static_cast<VkShaderStageFlagBits>(m_reflect_module.shader_stage);

    // Extract entry point — access directly from module
    if (m_reflect_module.entry_point_count > 0) {
        m_reflection.entry_point_name = m_reflect_module.entry_points[0].name;
    }

    // Extract descriptor bindings
    uint32_t binding_count = 0;
    spvReflectEnumerateDescriptorBindings(&m_reflect_module, &binding_count, nullptr);
    if (binding_count > 0) {
        m_reflection.descriptor_bindings.resize(binding_count);
        spvReflectEnumerateDescriptorBindings(
            &m_reflect_module,
            &binding_count,
            m_reflection.descriptor_bindings.data()
        );
    }

    // Extract descriptor sets
    uint32_t set_count = 0;
    spvReflectEnumerateDescriptorSets(&m_reflect_module, &set_count, nullptr);
    if (set_count > 0) {
        m_reflection.descriptor_sets.resize(set_count);
        spvReflectEnumerateDescriptorSets(
            &m_reflect_module,
            &set_count,
            m_reflection.descriptor_sets.data()
        );
    }

    // Extract push constants
    uint32_t push_constant_count = 0;
    spvReflectEnumeratePushConstantBlocks(&m_reflect_module, &push_constant_count, nullptr);
    if (push_constant_count > 0) {
        m_reflection.push_constants.resize(push_constant_count);
        spvReflectEnumeratePushConstantBlocks(
            &m_reflect_module,
            &push_constant_count,
            m_reflection.push_constants.data()
        );
    }

    // Extract input variables (vertex attributes, etc.)
    uint32_t input_count = 0;
    spvReflectEnumerateInputVariables(&m_reflect_module, &input_count, nullptr);
    if (input_count > 0) {
        m_reflection.input_variables.resize(input_count);
        spvReflectEnumerateInputVariables(
            &m_reflect_module,
            &input_count,
            m_reflection.input_variables.data()
        );
    }

    // Extract output variables
    uint32_t output_count = 0;
    spvReflectEnumerateOutputVariables(&m_reflect_module, &output_count, nullptr);
    if (output_count > 0) {
        m_reflection.output_variables.resize(output_count);
        spvReflectEnumerateOutputVariables(
            &m_reflect_module,
            &output_count,
            m_reflection.output_variables.data()
        );
    }
#else
    (void)spirv_code;
    m_reflection.stage = VK_SHADER_STAGE_VERTEX_BIT;
    m_reflection.entry_point_name = "main";
#endif
}

// ============================================================================
// Cleanup SPIRV-Reflect data
// ============================================================================
void Shader_Module::Cleanup_Reflection() {
#if HAS_SPIRV_REFLECT
    if (m_reflect_module._internal) {  // Check if initialized
        spvReflectDestroyShaderModule(&m_reflect_module);
        m_reflect_module = {};
    }
#endif
}

// ============================================================================
// Convert reflection to Vulkan descriptor set layout bindings
// ============================================================================
std::vector<VkDescriptorSetLayoutBinding> Shader_Module::Get_Descriptor_Set_Layout_Bindings(uint32_t set_index) const {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

#if HAS_SPIRV_REFLECT
    for (const auto* spv_binding : m_reflection.descriptor_bindings) {
        if (spv_binding->set != set_index) continue;

        VkDescriptorSetLayoutBinding binding{};
        binding.binding = spv_binding->binding;
        binding.descriptorType = static_cast<VkDescriptorType>(spv_binding->descriptor_type);
        binding.descriptorCount = spv_binding->count;
        binding.stageFlags = m_reflection.stage;
        binding.pImmutableSamplers = nullptr;  // Optional: set if using immutable samplers

        bindings.push_back(binding);
    }
#else
    (void)set_index;
#endif

    return bindings;
}

// ============================================================================
// Get push constant ranges for pipeline layout
// ============================================================================
std::vector<VkPushConstantRange> Shader_Module::Get_Push_Constant_Ranges() const {
    std::vector<VkPushConstantRange> ranges;

#if HAS_SPIRV_REFLECT
    for (const auto* push_constant : m_reflection.push_constants) {
        VkPushConstantRange range{};
        range.stageFlags = m_reflection.stage;
        range.offset = push_constant->offset;
        range.size = push_constant->size;

        ranges.push_back(range);
    }
#endif

    return ranges;
}

// ============================================================================
// Get vertex input attribute descriptions
// ============================================================================
std::vector<VkVertexInputAttributeDescription> Shader_Module::Get_Vertex_Input_Attributes(uint32_t binding) const {
    std::vector<VkVertexInputAttributeDescription> attributes;

#if HAS_SPIRV_REFLECT
    // Filter and sort by location
    std::vector<SpvReflectInterfaceVariable*> sorted_inputs;
    for (auto* input_var : m_reflection.input_variables) {
        if (input_var->location != 0xFFFFFFFF) {  // Skip built-ins
            sorted_inputs.push_back(input_var);
        }
    }

    std::sort(sorted_inputs.begin(), sorted_inputs.end(),
              [](auto* a, auto* b) { return a->location < b->location; });

    // Calculate offsets based on format size
    uint32_t current_offset = 0;

    for (const auto* input_var : sorted_inputs) {
        VkVertexInputAttributeDescription attr{};
        attr.location = input_var->location;
        attr.binding = binding;
        attr.format = static_cast<VkFormat>(input_var->format);

        // Calculate byte offset based on accumulated format sizes
        attr.offset = current_offset;
        current_offset += Get_Format_Size(attr.format);

        attributes.push_back(attr);
    }
#else
    (void)binding;
#endif

    return attributes;
}

// Helper to get byte size for VkFormat
uint32_t Shader_Module::Get_Format_Size(VkFormat format) const {
    switch (format) {
        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R8_SNORM:
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8_SINT: return 1;

        case VK_FORMAT_R8G8_UNORM:
        case VK_FORMAT_R8G8_SNORM:
        case VK_FORMAT_R16_UNORM:
        case VK_FORMAT_R16_SNORM:
        case VK_FORMAT_R16_SFLOAT:
        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16_SINT: return 2;

        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_R8G8B8_SNORM: return 3;

        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SNORM:
        case VK_FORMAT_R16G16_UNORM:
        case VK_FORMAT_R16G16_SNORM:
        case VK_FORMAT_R16G16_SFLOAT:
        case VK_FORMAT_R32_SFLOAT:
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT: return 4;

        case VK_FORMAT_R16G16B16A16_UNORM:
        case VK_FORMAT_R16G16B16A16_SNORM:
        case VK_FORMAT_R16G16B16A16_SFLOAT:
        case VK_FORMAT_R32G32_SFLOAT:
        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32_SINT: return 8;

        case VK_FORMAT_R32G32B32_SFLOAT:
        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32_SINT: return 12;

        case VK_FORMAT_R32G32B32A32_SFLOAT:
        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R32G32B32A32_SINT: return 16;

        default: return 16; // Conservative default
    }
}
