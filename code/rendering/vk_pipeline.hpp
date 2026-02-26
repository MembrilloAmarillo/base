#pragma once

#include <vulkan/vulkan.h>

#include "../memory/allocator.h"

#include "../vector/DynamicVector.h"

#include "vk_handle.hpp"
#include "vk_device.hpp"
#include "vk_descriptor.hpp"

class Pipeline_Layout {
public:
    struct Create_Info {
        Allocator* allocator;
        dyn_vector<const Descriptor*> sets;
        dyn_vector<const VkPushConstantRange> push_constant_ranges;
    };

    static Pipeline_Layout From_Shader_Reflection(const Device& device, dyn_vector<const Shader_Module* const> shaders);

    // Manual construction
    Pipeline_Layout() = default;
    Pipeline_Layout(const Device& device, const Create_Info& info);

    // Move semantics
    Pipeline_Layout(Pipeline_Layout&& other) noexcept;
    Pipeline_Layout& operator=(Pipeline_Layout&& other) noexcept;

    // Non-copyable
    Pipeline_Layout(const Pipeline_Layout&) = delete;
    Pipeline_Layout& operator=(const Pipeline_Layout&) = delete;

    ~Pipeline_Layout();

    VkPipelineLayout Get_Handle() const noexcept { return m_layout.Get(); }
    const Descriptor* Get_Descriptor_set(U32 set_index) const;
private:
    Allocator *m_allocator;
    Pipeline_Layout_Handle m_layout;
    dyn_vector<Descriptor>            m_owned_layouts;  // If created from reflection
    dyn_vector<VkPushConstantRange>   m_push_constants;
    const Device* m_device = nullptr;
};