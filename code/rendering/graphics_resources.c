#include "graphics_internal.h"

static const VkDescriptorType descriptor_types[GRAPHICS_DESCRIPTOR_TYPES] = {
    VK_DESCRIPTOR_TYPE_SAMPLER,
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};

uint32_t graphics_descriptor_type_index(VkDescriptorType type) {
    uint32_t index = 0U;
    while (index < GRAPHICS_DESCRIPTOR_TYPES && descriptor_types[index] != type)
        ++index;
    return index;
}

Graphics_Result graphics_create_binding_layout(Graphics_Device_t *device,
                                               const Gpu_Binding_Desc_t *bindings,
                                               uint32_t count,
                                               Gpu_Binding_Layout_t **out) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (out == NULL || *out != NULL || bindings == NULL || count == 0U)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (count > GRAPHICS_MAX_BINDINGS)
        return Graphics_CAPACITY;
    Gpu_Binding_Layout_t *layout = (Gpu_Binding_Layout_t *)calloc(1U, sizeof(*layout));
    if (layout == NULL)
        return Graphics_NO_MEMORY;
    VkDescriptorSetLayoutBinding native[GRAPHICS_MAX_BINDINGS] = {0};
    Graphics_Result result = Graphics_NONE;
    for (uint32_t i = 0U; i < count && result == Graphics_NONE; ++i) {
        const Gpu_Binding_Desc_t *binding = &bindings[i];
        uint32_t type = graphics_descriptor_type_index(binding->type);
        if (type == GRAPHICS_DESCRIPTOR_TYPES || binding->count == 0U || binding->stages == 0U ||
            (binding->stages & ~(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                                 VK_SHADER_STAGE_COMPUTE_BIT)) != 0U)
            result = Graphics_ERROR_INVALID_ARGUMENT;
        for (uint32_t j = 0U; j < i; ++j)
            if (bindings[j].binding == binding->binding)
                result = Graphics_ERROR_INVALID_ARGUMENT;
        if (result == Graphics_NONE) {
            if (binding->count > UINT32_MAX - layout->budgets[type])
                result = Graphics_OUT_OF_RANGE;
            else {
                layout->budgets[type] += binding->count;
                layout->bindings[i] = *binding;
                native[i] = (VkDescriptorSetLayoutBinding){
                    binding->binding, binding->type, binding->count, binding->stages, NULL};
            }
        }
    }
    VkDescriptorSetLayoutCreateInfo info = {.sType =
                                                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                            .bindingCount = count,
                                            .pBindings = native};
    if (result == Graphics_NONE) {
        VkDescriptorSetLayoutSupport supported = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT};
        vkGetDescriptorSetLayoutSupport(device->device, &info, &supported);
        if (!supported.supported)
            result = Graphics_UNSUPPORTED;
    }
    if (result == Graphics_NONE)
        result = graphics_vk_result(
            device,
            vkCreateDescriptorSetLayout(device->device, &info, NULL, &layout->layout),
            Graphics_ERROR_RESOURCE);
    if (result == Graphics_NONE) {
        layout->owner = device->device;
        layout->count = count;
        *out = layout;
        ++device->live_objects;
        ++device->stats.descriptor_layout_creations;
    } else
        free(layout);
    return result;
}

Graphics_Result graphics_destroy_binding_layout(Graphics_Device_t *device,
                                                Gpu_Binding_Layout_t **layout) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (layout == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (*layout == NULL)
        return Graphics_NONE;
    if ((*layout)->owner != device->device)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if ((*layout)->references != 0U)
        return Graphics_ERROR_RESOURCE;
    vkDestroyDescriptorSetLayout(device->device, (*layout)->layout, NULL);
    --device->live_objects;
    free(*layout);
    *layout = NULL;
    return Graphics_NONE;
}

Graphics_Result graphics_create_resources(Graphics_Device_t *device,
                                          Gpu_Binding_Layout_t *layout,
                                          Gpu_Binding_Set_t *out) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (layout == NULL || layout->owner != device->device || out == NULL ||
        out->set != VK_NULL_HANDLE)
        return Graphics_ERROR_INVALID_ARGUMENT;
    for (uint32_t i = 0U; i < GRAPHICS_MAX_DESCRIPTOR_POOLS; ++i) {
        Graphics_Descriptor_Pool_t *pool = &device->descriptor_pools[i];
        bool created = pool->pool == VK_NULL_HANDLE;
        if (created) {
            VkDescriptorPoolSize budgets[GRAPHICS_DESCRIPTOR_TYPES];
            for (uint32_t j = 0U; j < GRAPHICS_DESCRIPTOR_TYPES; ++j)
                budgets[j] = (VkDescriptorPoolSize){
                    descriptor_types[j], layout->budgets[j] > 128U ? layout->budgets[j] : 128U};
            VkDescriptorPoolCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                .maxSets = 64U,
                .poolSizeCount = GRAPHICS_DESCRIPTOR_TYPES,
                .pPoolSizes = budgets};
            Graphics_Result result =
                graphics_vk_result(device,
                                   vkCreateDescriptorPool(device->device, &info, NULL, &pool->pool),
                                   Graphics_NO_MEMORY);
            if (result != Graphics_NONE)
                return result;
            for (uint32_t j = 0U; j < GRAPHICS_DESCRIPTOR_TYPES; ++j)
                pool->available[j] = budgets[j].descriptorCount;
            ++device->stats.descriptor_pool_creations;
        }
        bool fits = pool->live_sets < 64U;
        for (uint32_t j = 0U; j < GRAPHICS_DESCRIPTOR_TYPES; ++j)
            fits = fits && pool->available[j] >= layout->budgets[j];
        if (fits) {
            Gpu_Binding_Set_t set = {.owner = device->device, .layout = layout, .pool_index = i};
            VkDescriptorSetAllocateInfo info = {.sType =
                                                    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                .descriptorPool = pool->pool,
                                                .descriptorSetCount = 1U,
                                                .pSetLayouts = &layout->layout};
            VkResult result = vkAllocateDescriptorSets(device->device, &info, &set.set);
            if (result == VK_SUCCESS) {
                for (uint32_t j = 0U; j < GRAPHICS_DESCRIPTOR_TYPES; ++j)
                    pool->available[j] -= layout->budgets[j];
                ++pool->live_sets;
                ++layout->references;
                ++device->live_objects;
                *out = set;
                return Graphics_NONE;
            }
            if (created) {
                vkDestroyDescriptorPool(device->device, pool->pool, NULL);
                *pool = (Graphics_Descriptor_Pool_t){0};
            }
            if (created ||
                (result != VK_ERROR_OUT_OF_POOL_MEMORY && result != VK_ERROR_FRAGMENTED_POOL))
                return graphics_vk_result(device, result, Graphics_ERROR_RESOURCE);
        }
    }
    return Graphics_CAPACITY;
}

Graphics_Result graphics_destroy_resources(Graphics_Device_t *device, Gpu_Binding_Set_t *set) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (set == NULL || (set->owner != VK_NULL_HANDLE && set->owner != device->device))
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (set->set != VK_NULL_HANDLE) {
        Graphics_Descriptor_Pool_t *pool = &device->descriptor_pools[set->pool_index];
        Graphics_Result result =
            graphics_vk_result(device,
                               vkFreeDescriptorSets(device->device, pool->pool, 1U, &set->set),
                               Graphics_ERROR_RESOURCE);
        if (result != Graphics_NONE)
            return result;
        --pool->live_sets;
        for (uint32_t j = 0U; j < GRAPHICS_DESCRIPTOR_TYPES; ++j)
            pool->available[j] += set->layout->budgets[j];
        --set->layout->references;
        --device->live_objects;
    }
    *set = (Gpu_Binding_Set_t){0};
    return Graphics_NONE;
}

Graphics_Result graphics_update_resources(Graphics_Device_t *device,
                                          const Gpu_Binding_Write_t *writes,
                                          uint32_t count) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (writes == NULL || count == 0U)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (count > GRAPHICS_MAX_DESCRIPTOR_WRITES)
        return Graphics_CAPACITY;
    VkWriteDescriptorSet native[GRAPHICS_MAX_DESCRIPTOR_WRITES] = {0};
    VkDescriptorBufferInfo buffers[GRAPHICS_MAX_DESCRIPTOR_VALUES] = {0};
    VkDescriptorImageInfo images[GRAPHICS_MAX_DESCRIPTOR_VALUES] = {0};
    uint32_t used = 0U;
    for (uint32_t i = 0U; i < count; ++i) {
        const Gpu_Binding_Write_t *write = &writes[i];
        if (write->set == NULL || write->set->owner != device->device ||
            write->set->set == VK_NULL_HANDLE || write->values == NULL || write->count == 0U)
            return Graphics_ERROR_INVALID_ARGUMENT;
        if (write->count > GRAPHICS_MAX_DESCRIPTOR_VALUES - used)
            return Graphics_CAPACITY;
        const Gpu_Binding_Desc_t *binding = NULL;
        for (uint32_t j = 0U; j < write->set->layout->count; ++j)
            if (write->set->layout->bindings[j].binding == write->binding)
                binding = &write->set->layout->bindings[j];
        if (binding == NULL)
            return Graphics_ERROR_INVALID_ARGUMENT;
        if (write->first_element > binding->count ||
            write->count > binding->count - write->first_element)
            return Graphics_OUT_OF_RANGE;
        bool buffer_type = binding->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                           binding->type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        native[i] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                           .dstSet = write->set->set,
                                           .dstBinding = write->binding,
                                           .dstArrayElement = write->first_element,
                                           .descriptorCount = write->count,
                                           .descriptorType = binding->type,
                                           .pBufferInfo = buffer_type ? &buffers[used] : NULL,
                                           .pImageInfo = buffer_type ? NULL : &images[used]};
        for (uint32_t j = 0U; j < write->count; ++j) {
            const Gpu_Binding_Value_t *value = &write->values[j];
            if (buffer_type) {
                const Gpu_Buffer_t *buffer = value->buffer;
                bool uniform = binding->type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                VkBufferUsageFlags usage = uniform ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                                                   : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                VkDeviceSize alignment =
                    uniform ? device->properties.limits.minUniformBufferOffsetAlignment
                            : device->properties.limits.minStorageBufferOffsetAlignment;
                VkDeviceSize max_size = uniform ? device->properties.limits.maxUniformBufferRange
                                                : device->properties.limits.maxStorageBufferRange;
                if (buffer == NULL || buffer->owner != device->device ||
                    buffer->buffer == VK_NULL_HANDLE || (buffer->usage & usage) == 0U ||
                    value->size == 0U || value->size > max_size || value->offset % alignment != 0U)
                    return Graphics_ERROR_INVALID_ARGUMENT;
                if (value->offset > buffer->size || value->size > buffer->size - value->offset)
                    return Graphics_OUT_OF_RANGE;
                buffers[used] =
                    (VkDescriptorBufferInfo){buffer->buffer, value->offset, value->size};
            } else {
                bool needs_image = binding->type != VK_DESCRIPTOR_TYPE_SAMPLER;
                bool needs_sampler = binding->type == VK_DESCRIPTOR_TYPE_SAMPLER ||
                                     binding->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                if (needs_sampler &&
                    (value->sampler == NULL || value->sampler->owner != device->device ||
                     value->sampler->sampler == VK_NULL_HANDLE))
                    return Graphics_ERROR_INVALID_ARGUMENT;
                if (needs_image) {
                    VkImageUsageFlags usage = binding->type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                                  ? VK_IMAGE_USAGE_STORAGE_BIT
                                                  : VK_IMAGE_USAGE_SAMPLED_BIT;
                    if (value->texture == NULL || value->texture->owner != device->device ||
                        value->texture->view == VK_NULL_HANDLE ||
                        value->texture->aspects ==
                            (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) ||
                        (value->texture->usage & usage) == 0U ||
                        (value->image_layout != VK_IMAGE_LAYOUT_GENERAL &&
                         (binding->type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                          value->image_layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)))
                        return Graphics_ERROR_INVALID_ARGUMENT;
                }
                images[used] = (VkDescriptorImageInfo){
                    needs_sampler ? value->sampler->sampler : VK_NULL_HANDLE,
                    needs_image ? value->texture->view : VK_NULL_HANDLE,
                    needs_image ? value->image_layout : VK_IMAGE_LAYOUT_UNDEFINED};
            }
            ++used;
        }
    }
    vkUpdateDescriptorSets(device->device, count, native, 0U, NULL);
    ++device->stats.descriptor_update_calls;
    return Graphics_NONE;
}
