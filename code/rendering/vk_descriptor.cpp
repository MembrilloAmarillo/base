#include "vk_descriptor.hpp"

Descriptor::Descriptor(const Device* device, const Descriptor_Create_Info& descriptor_ci) :
    m_allocator(descriptor_ci.allocator)
    , m_descriptor_types(descriptor_ci.descriptor_types)
    , m_max_descriptor_types(descriptor_ci.max_descriptor_types)
    , m_attachment_types(descriptor_ci.attachment_types)
    , m_max_attachment_types(descriptor_ci.max_attachment_types)
    , m_shader_stages(descriptor_ci.shader_stages)
    , m_device(device)
{

    auto bindings = dyn_vector<VkDescriptorSetLayoutBinding>::Init(m_allocator, m_descriptor_types.Length());

    for( U32 i = 0; i < m_descriptor_types.Length(); i++ ) {
        VkDescriptorSetLayoutBinding new_bind {
            .binding         = i,
            .descriptorType  = (VkDescriptorType)m_descriptor_types[i],
            .descriptorCount = m_max_descriptor_types[i],
            .stageFlags      = m_shader_stages[i],
            .pImmutableSamplers = nullptr // This maybe would be set by the user later on
        };

        bindings.Append(new_bind);
    }

    VkDescriptorBindingFlags flags =
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;


    auto binding_flags = dyn_vector<VkDescriptorBindingFlags>::Init(m_allocator, bindings.Length());
    for( U32 i = 0; i < bindings.Length(); i++ ) {
        if( i == bindings.Length() - 1 ) {
            flags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
        }
        binding_flags.Append(flags);
    }

    // Provided by VK_VERSION_1_2
    /*
     * typedef struct VkDescriptorSetLayoutBindingFlagsCreateInfo {
     *     VkStructureType                    sType;
     *     const void*                        pNext;
     *     uint32_t                           bindingCount;
     *     const VkDescriptorBindingFlags*    pBindingFlags;
     * } VkDescriptorSetLayoutBindingFlagsCreateInfo;
     */

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_ci {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = NULL,
        .bindingCount = binding_flags.Length(),
        .pBindingFlags = binding_flags.Memory()
    };

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_ci {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &binding_flags_ci,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = bindings.Length(),
        .pBindings = bindings.Memory()
    };

    VkDescriptorSetLayout layout;

    vkCreateDescriptorSetLayout(
        m_device->Get_Handle(),
        &descriptor_set_layout_ci,
        nullptr,
        &layout
    );

    #ifndef NDEBUG
        printf("[Vulkan Info] Descriptor with %lu and %lu binding flags\n",
            bindings.Length(),
            binding_flags.Length()
        );
    #endif

    m_descriptor_set_layout = Vulkan_Handle<VkDescriptorSetLayout, Descriptor_Set_Layout_Deleter>(layout, m_device->Get_Handle());

    if( Descriptor::m_global_pool == VK_NULL_HANDLE ) {

        #ifndef NDEBUG
        printf("[Vulkan Info] Max sets: %lu\n", Descriptor::m_max_sets);
        #endif

        auto pool_sizes = dyn_vector<VkDescriptorPoolSize>::Init(m_allocator, bindings.Length());
        for( U32 i = 0; i < pool_sizes.Capacity(); i++ ) {
            pool_sizes.AppendByCopy({
                (VkDescriptorType)m_descriptor_types[i],
                Descriptor::m_max_sets * m_max_descriptor_types[i]
                }
            );
            #ifndef NDEBUG
            printf("[Vulkan Info] Pool size count: %d\n", pool_sizes[i].descriptorCount);
            #endif
        }

        VkDescriptorPoolCreateInfo pool_ci {
            .sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext   = nullptr,
            .flags   = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets = Descriptor::m_max_sets, // I guess 32 maxSets is a good number?
            .poolSizeCount = pool_sizes.Length(),
            .pPoolSizes = pool_sizes.Memory()
        };

        if(vkCreateDescriptorPool( device->Get_Handle(), &pool_ci, nullptr, &m_global_pool) != VK_SUCCESS ) {
            printf("[Vulkan Error] Could not create descriptor pool\n");
            exit(1);
        }
    }

    U32 variable_count = m_max_descriptor_types[m_max_descriptor_types.Length()-1];
    VkDescriptorSetVariableDescriptorCountAllocateInfo variable_ci {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &variable_count
    };

    VkDescriptorSetAllocateInfo set_alloc_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &variable_ci,
        .descriptorPool = m_global_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = reinterpret_cast<const VkDescriptorSetLayout*>(m_descriptor_set_layout.Get_Ptr())
    };

    if( vkAllocateDescriptorSets(m_device->Get_Handle(), &set_alloc_info, &m_descriptor_set) != VK_SUCCESS ) {
        printf("[Vulkan Error] Could not create descriptor set\n");
        exit(1);
    }
}


void Descriptor::Update(dyn_vector<VkWriteDescriptorSet>& descriptor_writes) noexcept {
    vkUpdateDescriptorSets(
        m_device->Get_Handle(),
        descriptor_writes.Length(),
        descriptor_writes.Memory(),
        0,
        nullptr // TODO: Maybe implement this later?
    );
}


