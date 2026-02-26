#include "vk_pipeline.hpp"

Pipeline_Layout::Pipeline_Layout(const Device* device, const Create_Info& info) :
    m_device(device)
    , m_allocator(info.allocator)
    , m_push_constants(info.push_constant_ranges)
 {
    /*
    VkResult vkCreatePipelineLayout(
        VkDevice                                    device,
        const VkPipelineLayoutCreateInfo*           pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkPipelineLayout*                           pPipelineLayout);

    // Provided by VK_VERSION_1_0
    typedef struct VkPipelineLayoutCreateInfo {
        VkStructureType                 sType;
        const void*                     pNext;
        VkPipelineLayoutCreateFlags     flags;
        uint32_t                        setLayoutCount;
        const VkDescriptorSetLayout*    pSetLayouts;
        uint32_t                        pushConstantRangeCount;
        const VkPushConstantRange*      pPushConstantRanges;
    } VkPipelineLayoutCreateInfo;
    */

    auto descriptor_layouts = dyn_vector<VkDescriptorSetLayout>::Init(m_allocator, info.sets.Length());

    for( auto p_descriptor : info.sets ) {
        descriptor_layouts.AppendByCopy(p_descriptor->Get_Handle_Layout());
    }

    VkPipelineLayoutCreateInfo layout_ci {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .setLayoutCount = descriptor_layouts.Length(),
        .pSetLayouts = descriptor_layouts.Memory(),
        .pushConstantRangeCount = m_push_constants.Length(),
        .pPushConstantRanges = m_push_constants.Memory()
    };

    VkPipelineLayout layout;
    if( vkCreatePipelineLayout(device->Get_Handle(), &layout_ci, nullptr, &layout) != VK_SUCCESS )
    {
        printf("[Vulkan Error] Could not create pipeline layout\n");
        exit(1);
    }

    m_layout = Pipeline_Layout_Handle(layout, device->Get_Handle());
}