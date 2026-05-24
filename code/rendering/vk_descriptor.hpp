#pragma once

#include <vulkan/vulkan.h>

#include "../memory/allocator.h"
#include "../util/types.h"
#include "../vector/DynamicVector.h"

#include "vk_device.hpp"
#include "vk_handle.hpp"

enum Descriptor_Type {
    Storage_Image          = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    Sampler                = VK_DESCRIPTOR_TYPE_SAMPLER,
    Sampled_Image          = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    Combined_Image_Sampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    Uniform_Texel_Buffer   = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
    Storage_Buffer         = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    Uniform_Buffer         = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    Dynamic_Uniform_Buffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    Dynamic_Storage_Buffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
    Inline_Uniform_Block   = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
    Sample_Weight_Image    = VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM,
    Block_Matching_Image   = VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM,
    Acceleration_Structure = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    Storage_Tensor         = VK_DESCRIPTOR_TYPE_TENSOR_ARM
};

enum Shader_Stages {
    Vertex                 = VK_SHADER_STAGE_VERTEX_BIT,
    Tesellation_Control    = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    Tesellation_Evaluation = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    Geometry               = VK_SHADER_STAGE_GEOMETRY_BIT,
    Fragment               = VK_SHADER_STAGE_FRAGMENT_BIT,
    Compute                = VK_SHADER_STAGE_COMPUTE_BIT,
    All_Graphics           = VK_SHADER_STAGE_ALL_GRAPHICS,
    All                    = VK_SHADER_STAGE_ALL,
    RayGen                 = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
    Any_Hit                = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
    Closes_Hit             = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
    Miss                   = VK_SHADER_STAGE_MISS_BIT_KHR,
    Intersection           = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
    Callable               = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
    Task                   = VK_SHADER_STAGE_TASK_BIT_EXT,
    Mesh                   = VK_SHADER_STAGE_MESH_BIT_EXT,
    Subpass_Shading_Huawei = VK_SHADER_STAGE_SUBPASS_SHADING_BIT_HUAWEI,
    Cluster_Culling_Huawei = VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI,
    Raygen_Nv              = VK_SHADER_STAGE_RAYGEN_BIT_NV,
    Any_Hit_Nv             = VK_SHADER_STAGE_ANY_HIT_BIT_NV,
    Closes_Hit_Nv          = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV,
    Miss_Nv                = VK_SHADER_STAGE_MISS_BIT_NV,
    Intersection_Nv        = VK_SHADER_STAGE_INTERSECTION_BIT_NV,
    Callable_Nv            = VK_SHADER_STAGE_CALLABLE_BIT_NV,
    Task_Nv                = VK_SHADER_STAGE_TASK_BIT_NV,
    Mesh_Nv                = VK_SHADER_STAGE_MESH_BIT_NV
};

typedef U64 Shader_Stage_Flags;

struct Descriptor_Binding_Info {
    Descriptor_Type type;
    U32             descriptor_count;
    Shader_Stage_Flags shader_stages;
};

struct Descriptor_Create_Info {
    dyn_vector<Descriptor_Binding_Info> bindings;
    Allocator* allocator = nullptr;

    bool enable_update_after_bind = true;
    bool enable_partially_bound = true;
    bool enable_variable_descriptor_count_on_last_binding = false;
};

class Descriptor {
public:
    class Builder {
    public:
        Builder(const Device* device, Allocator* allocator, U32 initial_capacity = 4);

        Builder& Add_Binding(Descriptor_Type type, U32 descriptor_count, Shader_Stage_Flags shader_stages);
        Builder& Enable_Update_After_Bind(bool enable = true);
        Builder& Enable_Partially_Bound(bool enable = true);
        Builder& Enable_Variable_Descriptor_Count_Last_Binding(bool enable = true);

        Descriptor Build();

    private:
        const Device* m_device = nullptr;
        Descriptor_Create_Info m_info{};
    };

    Descriptor() = default;
    Descriptor(const Device* device, const Descriptor_Create_Info& descriptor_ci);

    Descriptor(const Descriptor& desc) = delete;
    Descriptor& operator=(const Descriptor& desc) = delete;

    Descriptor(Descriptor&& other) noexcept = default;
    Descriptor& operator=(Descriptor&& other) noexcept = default;

    VkDescriptorSetLayout Get_Handle_Layout() const noexcept { return m_descriptor_set_layout.Get(); }
    VkDescriptorSet Get_Set() const noexcept { return m_descriptor_set; }

    Descriptor& Update(dyn_vector<VkWriteDescriptorSet>& descriptor_writes) noexcept;

    VkWriteDescriptorSet Make_Buffer_Write(
        U32 binding,
        const VkDescriptorBufferInfo* buffer_info,
        Descriptor_Type type = Descriptor_Type::Uniform_Buffer,
        U32 descriptor_count = 1,
        U32 dst_array_element = 0) const noexcept;

    VkWriteDescriptorSet Make_Image_Write(
        U32 binding,
        const VkDescriptorImageInfo* image_info,
        Descriptor_Type type = Descriptor_Type::Combined_Image_Sampler,
        U32 descriptor_count = 1,
        U32 dst_array_element = 0) const noexcept;

    VkWriteDescriptorSet Make_Texel_Buffer_Write(
        U32 binding,
        const VkBufferView* buffer_view,
        Descriptor_Type type = Descriptor_Type::Uniform_Texel_Buffer,
        U32 descriptor_count = 1,
        U32 dst_array_element = 0) const noexcept;

private:
    void Validate_Create_Info(const Descriptor_Create_Info& descriptor_ci) const;

    Allocator* m_allocator = nullptr;
    const Device* m_device = nullptr;

    Descriptor_Set_Layout_Handle m_descriptor_set_layout;
    Descriptor_Pool_Handle m_descriptor_pool;
    VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
};
