#include "vk_descriptor.hpp"

#include <cstdio>
#include <cstdlib>

Descriptor::Builder::Builder(const Device* device, Allocator* allocator, U32 initial_capacity)
    : m_device(device) {
    m_info.allocator = allocator;
    m_info.bindings = dyn_vector<Descriptor_Binding_Info>::Init(allocator, initial_capacity == 0 ? 1 : initial_capacity);
}

Descriptor::Builder& Descriptor::Builder::Add_Binding(
    Descriptor_Type type,
    U32 descriptor_count,
    Shader_Stage_Flags shader_stages) {
    m_info.bindings.AppendByCopy({
        .type = type,
        .descriptor_count = descriptor_count,
        .shader_stages = shader_stages
    });
    return *this;
}

Descriptor::Builder& Descriptor::Builder::Enable_Update_After_Bind(bool enable) {
    m_info.enable_update_after_bind = enable;
    return *this;
}

Descriptor::Builder& Descriptor::Builder::Enable_Partially_Bound(bool enable) {
    m_info.enable_partially_bound = enable;
    return *this;
}

Descriptor::Builder& Descriptor::Builder::Enable_Variable_Descriptor_Count_Last_Binding(bool enable) {
    m_info.enable_variable_descriptor_count_on_last_binding = enable;
    return *this;
}

Descriptor Descriptor::Builder::Build() {
    Descriptor descriptor(m_device, m_info);
    m_info.bindings.Destroy();
    m_info.bindings = {};
    return descriptor;
}

void Descriptor::Validate_Create_Info(const Descriptor_Create_Info& descriptor_ci) const {
    if (m_device == nullptr) {
        printf("[Vulkan Error] Descriptor requires a valid device\n");
        exit(EXIT_FAILURE);
    }

    if (descriptor_ci.allocator == nullptr) {
        printf("[Vulkan Error] Descriptor requires a valid allocator\n");
        exit(EXIT_FAILURE);
    }

    if (descriptor_ci.bindings.Length() == 0) {
        printf("[Vulkan Error] Descriptor requires at least one binding\n");
        exit(EXIT_FAILURE);
    }

    for (U32 i = 0; i < descriptor_ci.bindings.Length(); ++i) {
        if (descriptor_ci.bindings[i].descriptor_count == 0) {
            printf("[Vulkan Error] Descriptor binding %u has descriptor_count = 0\n", i);
            exit(EXIT_FAILURE);
        }
    }
}

Descriptor::Descriptor(const Device* device, const Descriptor_Create_Info& descriptor_ci)
    : m_allocator(descriptor_ci.allocator)
    , m_device(device) {
    Validate_Create_Info(descriptor_ci);

    auto bindings = dyn_vector<VkDescriptorSetLayoutBinding>::Init(m_allocator, descriptor_ci.bindings.Length());
    for (U32 i = 0; i < descriptor_ci.bindings.Length(); ++i) {
        const auto& b = descriptor_ci.bindings[i];
        bindings.AppendByCopy({
            .binding = i,
            .descriptorType = static_cast<VkDescriptorType>(b.type),
            .descriptorCount = b.descriptor_count,
            .stageFlags = static_cast<VkShaderStageFlags>(b.shader_stages),
            .pImmutableSamplers = nullptr
        });
    }

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<U32>(bindings.Length()),
        .pBindings = bindings.Memory()
    };

    auto binding_flags = dyn_vector<VkDescriptorBindingFlags>::Init(m_allocator, bindings.Length());
    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = 0,
        .pBindingFlags = nullptr
    };

    if (descriptor_ci.enable_update_after_bind ||
        descriptor_ci.enable_partially_bound ||
        descriptor_ci.enable_variable_descriptor_count_on_last_binding) {
        VkDescriptorBindingFlags flags = 0;
        if (descriptor_ci.enable_update_after_bind) {
            flags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
            descriptor_set_layout_ci.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        }
        if (descriptor_ci.enable_partially_bound) {
            flags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        }

        for (U32 i = 0; i < bindings.Length(); ++i) {
            VkDescriptorBindingFlags per_binding_flags = flags;
            if (descriptor_ci.enable_variable_descriptor_count_on_last_binding && i == bindings.Length() - 1) {
                per_binding_flags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            }
            binding_flags.AppendByCopy(per_binding_flags);
        }

        binding_flags_ci.bindingCount = static_cast<U32>(binding_flags.Length());
        binding_flags_ci.pBindingFlags = binding_flags.Memory();
        descriptor_set_layout_ci.pNext = &binding_flags_ci;
    }

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(m_device->Get_Handle(), &descriptor_set_layout_ci, nullptr, &layout) != VK_SUCCESS) {
        printf("[Vulkan Error] Could not create descriptor set layout\n");
        exit(EXIT_FAILURE);
    }

    m_descriptor_set_layout = Descriptor_Set_Layout_Handle(layout, m_device->Get_Handle());

    auto pool_sizes = dyn_vector<VkDescriptorPoolSize>::Init(m_allocator, descriptor_ci.bindings.Length());
    for (U32 i = 0; i < descriptor_ci.bindings.Length(); ++i) {
        pool_sizes.AppendByCopy({
            .type = static_cast<VkDescriptorType>(descriptor_ci.bindings[i].type),
            .descriptorCount = descriptor_ci.bindings[i].descriptor_count
        });
    }

    VkDescriptorPoolCreateFlags pool_flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    if (descriptor_ci.enable_update_after_bind) {
        pool_flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    }

    VkDescriptorPoolCreateInfo pool_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = pool_flags,
        .maxSets = 1,
        .poolSizeCount = static_cast<U32>(pool_sizes.Length()),
        .pPoolSizes = pool_sizes.Memory()
    };

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(m_device->Get_Handle(), &pool_ci, nullptr, &descriptor_pool) != VK_SUCCESS) {
        printf("[Vulkan Error] Could not create descriptor pool\n");
        exit(EXIT_FAILURE);
    }
    m_descriptor_pool = Descriptor_Pool_Handle(descriptor_pool, m_device->Get_Handle());

    U32 last_binding_count = descriptor_ci.bindings[descriptor_ci.bindings.Length() - 1].descriptor_count;
    VkDescriptorSetVariableDescriptorCountAllocateInfo variable_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &last_binding_count
    };

    VkDescriptorSetAllocateInfo set_alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = descriptor_ci.enable_variable_descriptor_count_on_last_binding ? &variable_ci : nullptr,
        .descriptorPool = m_descriptor_pool.Get(),
        .descriptorSetCount = 1,
        .pSetLayouts = reinterpret_cast<const VkDescriptorSetLayout*>(m_descriptor_set_layout.Get_Ptr())
    };

    if (vkAllocateDescriptorSets(m_device->Get_Handle(), &set_alloc_info, &m_descriptor_set) != VK_SUCCESS) {
        printf("[Vulkan Error] Could not allocate descriptor set\n");
        exit(EXIT_FAILURE);
    }

    bindings.Destroy();
    binding_flags.Destroy();
    pool_sizes.Destroy();
}

Descriptor& Descriptor::Update(dyn_vector<VkWriteDescriptorSet>& descriptor_writes) noexcept {
    vkUpdateDescriptorSets(
        m_device->Get_Handle(),
        static_cast<U32>(descriptor_writes.Length()),
        descriptor_writes.Memory(),
        0,
        nullptr);
    return *this;
}

VkWriteDescriptorSet Descriptor::Make_Buffer_Write(
    U32 binding,
    const VkDescriptorBufferInfo* buffer_info,
    Descriptor_Type type,
    U32 descriptor_count,
    U32 dst_array_element) const noexcept {
    return VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = m_descriptor_set,
        .dstBinding = binding,
        .dstArrayElement = dst_array_element,
        .descriptorCount = descriptor_count,
        .descriptorType = static_cast<VkDescriptorType>(type),
        .pImageInfo = nullptr,
        .pBufferInfo = buffer_info,
        .pTexelBufferView = nullptr
    };
}

VkWriteDescriptorSet Descriptor::Make_Image_Write(
    U32 binding,
    const VkDescriptorImageInfo* image_info,
    Descriptor_Type type,
    U32 descriptor_count,
    U32 dst_array_element) const noexcept {
    return VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = m_descriptor_set,
        .dstBinding = binding,
        .dstArrayElement = dst_array_element,
        .descriptorCount = descriptor_count,
        .descriptorType = static_cast<VkDescriptorType>(type),
        .pImageInfo = image_info,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };
}

VkWriteDescriptorSet Descriptor::Make_Texel_Buffer_Write(
    U32 binding,
    const VkBufferView* buffer_view,
    Descriptor_Type type,
    U32 descriptor_count,
    U32 dst_array_element) const noexcept {
    return VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = m_descriptor_set,
        .dstBinding = binding,
        .dstArrayElement = dst_array_element,
        .descriptorCount = descriptor_count,
        .descriptorType = static_cast<VkDescriptorType>(type),
        .pImageInfo = nullptr,
        .pBufferInfo = nullptr,
        .pTexelBufferView = buffer_view
    };
}
