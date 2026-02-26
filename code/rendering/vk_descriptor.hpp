#pragma once

/**
 * From the vulkan specfication
 * Opaque data structure representing a shader resource such a buffer, buffer view, image view, sampler
 * or combined image sampler. Descriptors are organized into descriptor sets, which are bound during command
 * recording for use in subsequent drawing commands. The arrangement of content in each descriptor set
 * is determined by a descriptor set layout, which determines what descriptors can be stored within it.
 * The sequence of descriptor set layouts that can be used by a pipeline is specified in a pipeline layout.
 * Each pipeline object can use up to maxBoundDescriptorSets descriptor sets.
 * Shaders can also access buffers without going through descriptors by using Physical Storage Buffer ACcess
 * to access them through 64-bit addresses.
 * ---
 * Storage_Image <---> VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
 * * Storage image loads are supported in all shader stages for image views whose
 * * format features contain VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT.
 * * Atomic operations on storage images are supported in task, mesh and compute
 * * shaders for image views whose format features contain VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT.
 * * The image subresources for a storage image must be in the
 * * VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR or VK_IMAGE_LAYOUT_TENSOR_ALIASING_ARM or
 * * VK_IMAGE_LAYOUT_GENERAL layout in order to access its data in a shader.
 *
 * Sampler <---> VK_DESCRIPTOR_TYPE_SAMPLER
 * * descriptor type associated with a sampler object, used to control the behavior of
 * * sampling operations performed on a sampled image.
 *
 * Sampled_Image <---> VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
 * * is a descriptor type associated with an image resource via an image
 * * view that sampling operations can be performed on.
 * * Sampled images are supported in all shader stages for image views whose format
 * * features contain VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT.
 * * An image subresources for a sampled image must be in one of the following layouts:
 * * * VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_GENERAL
 * * * VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR
 * * * VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
 * * * VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR
 * * * VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT
 * * * VK_IMAGE_LAYOUT_TENSOR_ALIASING_ARM
 *
 * Combined_Image_Sampler <---> VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
 * * is a single descriptor type associated with both a sampler and an image resource,
 * * combining both a sampler and sampled image descriptor into a single descriptor.
 * *
 * * If the descriptor refers to a sampler that performs Y′CBCR conversion or samples a subsampled image,
 * * the sampler must only be used to sample the image in the same descriptor. Otherwise, the sampler
 * * and image in this type of descriptor can be used freely with any other samplers and images.
 * * An image subresources for a combined image sampler must be in one of the following layouts:
 * * * VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_GENERAL
 * * * VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR
 * * * VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
 * * * VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR
 * * * VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT
 * * * VK_IMAGE_LAYOUT_TENSOR_ALIASING_ARM
 *
 * Uniform_Texel_Buffer <---> VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
 * * is a descriptor type associated with a buffer resource via a buffer view that image sampling
 * * operations can be performed on.
 * * Uniform texel buffers define a tightly-packed 1-dimensional linear array of texels,
 * * with texels going through format conversion when read in a shader in the same way as they are for an image.
 * * Load operations from uniform texel buffers are supported in all shader stages for
 * * buffer view formats which report format features support for VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT
 *
 * Storage_Buffer <---> VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
 * * is a descriptor type associated with a buffer resource directly, described in a shader
 * * as a structure with various members that load, store, and atomic operations can be performed on.
 *
 * Uniform_Buffer <---> VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
 * * is a descriptor type associated with a buffer resource directly, described in a shader
 * * as a structure with various members that load operations can be performed on.
 *
 * Dynamic_Uniform_Buffer <---> VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
 * * is almost identical to a uniform buffer, and differs only in how the offset into the buffer is specified.
 * * The base offset calculated by the VkDescriptorBufferInfo when initially updating the descriptor set
 * * is added to a dynamic offset when binding the descriptor set.
 *
 * Dynamic_Storage_Buffer <---> VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
 * * is almost identical to a storage buffer, and differs only in how the offset into the buffer is specified.
 * * The base offset calculated by the VkDescriptorBufferInfo when initially updating the descriptor set
 * * is added to a dynamic offset when binding the descriptor set.
 *
 * Inline_Uniform_Block <---> VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK
 * * is almost identical to a uniform buffer, and differs only in taking its storage directly from
 * * the encompassing descriptor set instead of being backed by buffer memory. It is typically used
 * * to access a small set of constant data that does not require the additional flexibility provided
 * * by the indirection enabled when using a uniform buffer where the descriptor and the referenced
 * * buffer memory are decoupled. Compared to push constants, they allow reusing the same set of constant
 * * data across multiple disjoint sets of drawing and dispatching commands.
 * * Inline uniform block descriptors cannot be aggregated into arrays. Instead, the array size
 * * specified for an inline uniform block descriptor binding specifies the binding’s capacity in bytes.
 *
 * Sample_Weight_Image <---> VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM
 * * A sample weight image (VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM) is a descriptor type associated
 * * with an image resource via an image view that can be used in weight image sampling. The image
 * * view must have been created with VkImageViewSampleWeightCreateInfoQCOM.
 * * Shaders can combine a weight image variable, a sampled image variable, and a sampler variable to
 * * perform weight image sampling.
 * * Weight image sampling is supported in all shader stages if the weight image view specifies a format that
 * * supports format feature VK_FORMAT_FEATURE_2_WEIGHT_IMAGE_BIT_QCOM and the sampled image view specifies a
 * * format that supports format feature VK_FORMAT_FEATURE_2_WEIGHT_SAMPLED_IMAGE_BIT_QCOM
 * * The image subresources for the weight image must be in the VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, or
 * * VK_IMAGE_LAYOUT_GENERAL layout in order to access its data in a shader.
 *
 * Block_Matching_Image <---> VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM
 * * is a descriptor type associated with an image resource via an image view that can be used in block matching.
 * * Shaders can combine a target image variable, a reference image variable, and a sampler variable
 * * to perform block matching.
 * * Block matching is supported in all shader stages for if both the target view and reference view
 * * specifies a format that supports format feature VK_FORMAT_FEATURE_2_BLOCK_MATCHING_BIT_QCOM
 * * The image subresources for block matching must be in the VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, or
 * * VK_IMAGE_LAYOUT_GENERAL layout in order to access its data in a shader.
 *
 * Input_Attachment <---> VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
 * * is a descriptor type associated with an image resource via an image view that can be used for framebuffer local load operations in fragment shaders.
 * * All image formats that are supported for color attachments (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
 * * or VK_FORMAT_FEATURE_2_LINEAR_COLOR_ATTACHMENT_BIT_NV ) or depth/stencil attachments
 * * (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) for a given image tiling mode are also supported
 * * for input attachments.
 * * An image view used as an input attachment must be in one of the following layouts:
 * * * VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_GENERAL
 * * * VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR
 * * * VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
 * * * VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL
 * * * VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR
 * * * VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT
 * * * VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ
 *
 * Acceleration_Structure <---> VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR or VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV
 * * is a descriptor type that is used to retrieve scene geometry from within shaders that are used
 * * for ray traversal. Shaders have read-only access to the memory.
 *
 * Storage_Tensor <---> VK_DESCRIPTOR_TYPE_TENSOR_ARM
 * * is a descriptor type associated with a tensor resource via a tensor view that read and write
 * * operations can be performed on.
 * * Storage tensor reads and writes are supported in shaders for tensor views whose format features
 * * contain VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM.
 * * Storage tensor reads and writes are supported in graph pipelines for tensor views whose format features
 * * contain VK_FORMAT_FEATURE_2_TENSOR_DATA_GRAPH_BIT_ARM.
 *
 * --- To create a descriptor set layout ----
 * VkResult vkCreateDescriptorSetLayout(
    VkDevice                                    device,
    const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDescriptorSetLayout*                      pSetLayout);
 */

#include "../vector/DynamicVector.h"
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

enum Attachment_Type {
    Depth_Stencil_Read_Only            = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    Shader_Read_Only                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    General                            = VK_IMAGE_LAYOUT_GENERAL,
    Present                            = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
    Depth_Read_Only_Stencil_Attachment = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
    Depth_Attachment_Stencil_Read_Only = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
    Depth_Read_Only                    = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
    Stencil_Read_Only                  = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
    Read_Only                          = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR,
    Feedback_Loop                      = VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT,
    Rendering_Local_Read               = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ
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

struct Descriptor_Create_Info {
    dyn_vector<Descriptor_Type> descriptor_types;
    dyn_vector<U32>             max_descriptor_types;
    dyn_vector<Attachment_Type> attachment_types;
    dyn_vector<U32>             max_attachment_types;
    dyn_vector<Shader_Stage_Flags>   shader_stages;
    Allocator                   *allocator;
};



class Descriptor {

public:
    Descriptor(const Device* device, const Descriptor_Create_Info& descriptor_ci);

    Descriptor(Descriptor& desc) = delete;
    Descriptor& operator=(Descriptor& desc) = delete;

    Descriptor(Descriptor&& other) noexcept;
    Descriptor& operator=(Descriptor&& other) noexcept;

    VkDescriptorSetLayout Get_Handle_Layout() const noexcept { return m_descriptor_set_layout.Get(); }

    VkDescriptorSet Get_Set() const noexcept { return m_descriptor_set; }

    void Update(dyn_vector<VkWriteDescriptorSet>& descriptor_writes) noexcept;

    ~Descriptor() {
        if( m_global_pool != VK_NULL_HANDLE ) {
            vkDestroyDescriptorPool(m_device->Get_Handle(), m_global_pool, nullptr);
        }
    }

    static VkDescriptorPool m_global_pool;

private:
    static constexpr U32 m_max_sets = 32;

    Allocator* m_allocator;

    const Device *m_device;

    dyn_vector<Descriptor_Type>      m_descriptor_types;
    dyn_vector<U32>                  m_max_descriptor_types;
    dyn_vector<Attachment_Type>      m_attachment_types;
    dyn_vector<U32>                  m_max_attachment_types;
    dyn_vector<Shader_Stage_Flags>   m_shader_stages;

    Descriptor_Set_Layout_Handle m_descriptor_set_layout;
    VkDescriptorSet              m_descriptor_set;


};

VkDescriptorPool Descriptor::m_global_pool = VK_NULL_HANDLE;



class Descritor_Write {
public:

    Descritor_Write() = delete;

    Descritor_Write(Descriptor_Type t, U32 bind, U32 array_el, U32 desc_count) :
        type(t)
        , dst_binding(bind)
        , dst_array_element(array_el)
        , descriptor_count(desc_count)
    {

    }
    ~Descritor_Write() = default;

    virtual void                   Set_Info(VkSampler sampler, VkImageView image_view, VkImageLayout image_layout);
    virtual void                   Set_Info(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
    virtual VkDescriptorBufferInfo Get_Buffer_Info();
    virtual VkDescriptorImageInfo  Get_Descriptor_Image_Info();
    virtual VkBufferView           Get_Buffer_View();
    virtual VkWriteDescriptorSet   Get_Write_Descriptor_set(const Descriptor &descriptor);

    Descriptor_Type type;
    U32 dst_binding;
    U32 dst_array_element;
    U32 descriptor_count;
};

class Descriptor_Write_Image : public Descritor_Write {
public:

    Descriptor_Write_Image(Descriptor_Type t, U32 bind, U32 array_el, U32 desc_count) :
        Descritor_Write(t, bind, array_el, desc_count) {}

    void Set_Info(VkSampler sampler, VkImageView image_view, VkImageLayout image_layout) {
        m_sampler      = sampler;
        m_image_view   = image_view;
        m_image_layout = image_layout;
    }

    VkDescriptorImageInfo Get_Descriptor_Image_Info() {
        return (VkDescriptorImageInfo){
            .sampler     = m_sampler,
            .imageView   = m_image_view,
            .imageLayout = m_image_layout
        };
    }

    VkWriteDescriptorSet Get_Write_Descriptor_set(const Descriptor &descriptor) {

        VkDescriptorImageInfo info = Get_Descriptor_Image_Info();

        VkWriteDescriptorSet write {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = 0,
            .dstSet = descriptor.Get_Set(),
            .dstBinding = dst_binding,
            .dstArrayElement = dst_array_element,
            .descriptorCount = descriptor_count,
            .descriptorType = static_cast<VkDescriptorType>(type),
            .pImageInfo = &info,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };

        return write;
    }

private:
    VkSampler     m_sampler;
    VkImageView   m_image_view;
    VkImageLayout m_image_layout;
};

class Descriptor_Write_Buffer : public Descritor_Write {
public:

    Descriptor_Write_Buffer(Descriptor_Type t, U32 bind, U32 array_el, U32 desc_count) :
        Descritor_Write(t, bind, array_el, desc_count) {}

    void Set_Info(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
        m_buffer = buffer;
        m_offset = offset;
        m_range  = range;
    }

    VkDescriptorBufferInfo Get_Buffer_Info() {
        return (VkDescriptorBufferInfo){
            .buffer = m_buffer,
            .offset = m_offset,
            .range  = m_range
        };
    }

    VkWriteDescriptorSet Get_Write_Descriptor_set(const Descriptor &descriptor) {

        VkDescriptorBufferInfo info = Get_Buffer_Info();

        VkWriteDescriptorSet write {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = 0,
            .dstSet = descriptor.Get_Set(),
            .dstBinding = dst_binding,
            .dstArrayElement = dst_array_element,
            .descriptorCount = descriptor_count,
            .descriptorType = static_cast<VkDescriptorType>(type),
            .pImageInfo = nullptr,
            .pBufferInfo = &info,
            .pTexelBufferView = nullptr
        };

        return write;
    }

private:
    VkBuffer     m_buffer;
    VkDeviceSize m_offset;
    VkDeviceSize m_range;
};

class Descriptor_Write_Buffer_View : public Descritor_Write {
public:

    Descriptor_Write_Buffer_View(const Device *device, VkFormat format, Descriptor_Type t, U32 bind, U32 array_el, U32 desc_count) :
        m_device(device)
        , m_format(format)
        , Descritor_Write(t, bind, array_el, desc_count) {}

    void Set_Info(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
        if( m_buffer_view != VK_NULL_HANDLE ) {
            return;
        }

        m_buffer = buffer;
        m_offset = offset;
        m_range  = range;

        VkBufferViewCreateInfo create_info {
            .sType  = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
            .pNext  = nullptr,
            .flags  = 0,
            .buffer = m_buffer,
            .format = m_format,
            .offset = m_offset,
            .range  = m_range
        };

        if( vkCreateBufferView(m_device->Get_Handle(), &create_info, nullptr, &m_buffer_view) != VK_SUCCESS ) {
            printf("[Vulkan Error] Buffer view create error\n");
            exit(1);
        }
    }

    VkBufferView Get_Buffer_View() {
        return m_buffer_view;
    }

    VkWriteDescriptorSet Get_Write_Descriptor_set(const Descriptor &descriptor) {

        VkBufferView info = Get_Buffer_View();

        VkWriteDescriptorSet write {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = 0,
            .dstSet = descriptor.Get_Set(),
            .dstBinding = dst_binding,
            .dstArrayElement = dst_array_element,
            .descriptorCount = descriptor_count,
            .descriptorType = static_cast<VkDescriptorType>(type),
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = &info
        };

        return write;
    }

private:
    VkBufferView  m_buffer_view;
    const Device* m_device;
    VkFormat      m_format;
    VkBuffer      m_buffer;
    VkDeviceSize  m_offset;
    VkDeviceSize  m_range;
};