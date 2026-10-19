#include "graphics_internal.h"

Gpu_Color_Target_t graphics_color_target_default(VkFormat format) {
    Gpu_Color_Target_t result = {
        .format = format,
        .blend = {.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                  .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                  .colorBlendOp = VK_BLEND_OP_ADD,
                  .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                  .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                  .alphaBlendOp = VK_BLEND_OP_ADD,
                  .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}};
    return result;
}

static Graphics_Result validate_pipeline(Graphics_Device_t *device,
                                         const Gpu_Pipeline_Desc_t *desc) {
    if (desc == NULL || desc->vertex_spirv == NULL || desc->vertex_words < 5U ||
        desc->vertex_words > SIZE_MAX / sizeof(uint32_t) || desc->vertex_spirv[0] != 0x07230203U ||
        (desc->fragment_words != 0U && (desc->fragment_spirv == NULL || desc->fragment_words < 5U ||
                                        desc->fragment_words > SIZE_MAX / sizeof(uint32_t) ||
                                        desc->fragment_spirv[0] != 0x07230203U)) ||
        (desc->color_count != 0U && desc->colors == NULL) ||
        (desc->layout_count != 0U && desc->layouts == NULL) ||
        (desc->push_range_count != 0U && desc->push_ranges == NULL))
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (desc->color_count > GRAPHICS_MAX_COLOR_ATTACHMENTS ||
        desc->color_count > device->properties.limits.maxColorAttachments ||
        desc->layout_count > GRAPHICS_MAX_SET_LAYOUTS ||
        desc->layout_count > device->properties.limits.maxBoundDescriptorSets ||
        desc->push_range_count > GRAPHICS_MAX_PUSH_RANGES)
        return Graphics_OUT_OF_RANGE;
    for (uint32_t i = 0U; i < desc->layout_count; ++i) {
        if (desc->layouts[i] == NULL || desc->layouts[i]->owner != device->device)
            return Graphics_ERROR_INVALID_ARGUMENT;
    }
    VkShaderStageFlags seen = 0U;
    for (uint32_t i = 0U; i < desc->push_range_count; ++i) {
        const VkPushConstantRange *range = &desc->push_ranges[i];
        if (range->size == 0U || range->size % 4U != 0U || range->offset % 4U != 0U ||
            range->stageFlags == 0U ||
            (range->stageFlags & ~(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)) !=
                0U ||
            (seen & range->stageFlags) != 0U)
            return Graphics_ERROR_INVALID_ARGUMENT;
        if (range->offset > device->properties.limits.maxPushConstantsSize ||
            range->size > device->properties.limits.maxPushConstantsSize - range->offset)
            return Graphics_OUT_OF_RANGE;
        seen |= range->stageFlags;
    }
    for (uint32_t i = 0U; i < desc->color_count; ++i) {
        if (desc->colors[i].format == VK_FORMAT_UNDEFINED ||
            graphics_format_aspects(desc->colors[i].format) != VK_IMAGE_ASPECT_COLOR_BIT)
            return Graphics_ERROR_INVALID_ARGUMENT;
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(
            device->physical_device, desc->colors[i].format, &properties);
        VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        if (desc->colors[i].blend.blendEnable)
            required |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
        if ((properties.optimalTilingFeatures & required) != required)
            return Graphics_UNSUPPORTED;
        /* independentBlend is deliberately not a baseline device requirement. */
        if (i != 0U &&
            memcmp(&desc->colors[i].blend, &desc->colors[0].blend, sizeof(desc->colors[i].blend)) !=
                0)
            return Graphics_UNSUPPORTED;
    }
    if ((desc->depth_format != VK_FORMAT_UNDEFINED &&
         (graphics_format_aspects(desc->depth_format) & VK_IMAGE_ASPECT_DEPTH_BIT) == 0U) ||
        (desc->stencil_format != VK_FORMAT_UNDEFINED &&
         (graphics_format_aspects(desc->stencil_format) & VK_IMAGE_ASPECT_STENCIL_BIT) == 0U) ||
        (desc->depth_format != VK_FORMAT_UNDEFINED && desc->stencil_format != VK_FORMAT_UNDEFINED &&
         desc->depth_format != desc->stencil_format))
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (desc->rasterization != NULL &&
        (desc->rasterization->sType != VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO ||
         desc->rasterization->pNext != NULL || desc->rasterization->flags != 0U ||
         desc->rasterization->depthClampEnable || desc->rasterization->depthBiasClamp != 0.0f ||
         desc->rasterization->polygonMode != VK_POLYGON_MODE_FILL ||
         desc->rasterization->lineWidth != 1.0f))
        return Graphics_UNSUPPORTED;
    if (desc->topology != NULL && (*desc->topology < VK_PRIMITIVE_TOPOLOGY_POINT_LIST ||
                                   *desc->topology > VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN))
        return Graphics_UNSUPPORTED;
    if (desc->vertex_input != NULL) {
        const VkPipelineVertexInputStateCreateInfo *input = desc->vertex_input;
        if (input->sType != VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO ||
            input->pNext != NULL || input->flags != 0U ||
            input->vertexBindingDescriptionCount >
                device->properties.limits.maxVertexInputBindings ||
            input->vertexAttributeDescriptionCount >
                device->properties.limits.maxVertexInputAttributes ||
            (input->vertexBindingDescriptionCount != 0U &&
             input->pVertexBindingDescriptions == NULL) ||
            (input->vertexAttributeDescriptionCount != 0U &&
             input->pVertexAttributeDescriptions == NULL))
            return Graphics_ERROR_INVALID_ARGUMENT;
        for (uint32_t i = 0U; i < input->vertexBindingDescriptionCount; ++i) {
            const VkVertexInputBindingDescription *binding = &input->pVertexBindingDescriptions[i];
            if (binding->binding >= device->properties.limits.maxVertexInputBindings ||
                binding->stride > device->properties.limits.maxVertexInputBindingStride ||
                (binding->inputRate != VK_VERTEX_INPUT_RATE_VERTEX &&
                 binding->inputRate != VK_VERTEX_INPUT_RATE_INSTANCE))
                return Graphics_ERROR_INVALID_ARGUMENT;
            for (uint32_t j = 0U; j < i; ++j)
                if (binding->binding == input->pVertexBindingDescriptions[j].binding)
                    return Graphics_ERROR_INVALID_ARGUMENT;
        }
        for (uint32_t i = 0U; i < input->vertexAttributeDescriptionCount; ++i) {
            const VkVertexInputAttributeDescription *attribute =
                &input->pVertexAttributeDescriptions[i];
            bool found = false;
            for (uint32_t j = 0U; j < input->vertexBindingDescriptionCount; ++j)
                found = found || attribute->binding == input->pVertexBindingDescriptions[j].binding;
            if (!found ||
                attribute->location >= device->properties.limits.maxVertexInputAttributes ||
                attribute->offset > device->properties.limits.maxVertexInputAttributeOffset)
                return Graphics_ERROR_INVALID_ARGUMENT;
            for (uint32_t j = 0U; j < i; ++j)
                if (attribute->location == input->pVertexAttributeDescriptions[j].location)
                    return Graphics_ERROR_INVALID_ARGUMENT;
            VkFormatProperties properties;
            vkGetPhysicalDeviceFormatProperties(
                device->physical_device, attribute->format, &properties);
            if ((properties.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) == 0U)
                return Graphics_UNSUPPORTED;
        }
    }
    return Graphics_NONE;
}

Graphics_Result graphics_create_pipeline(Graphics_Device_t *device,
                                         const Gpu_Pipeline_Desc_t *desc,
                                         Gpu_Pipeline_t *out) {
    if (device == NULL)
        return Graphics_NO_DEVICE;
    if (out == NULL || out->pipeline != VK_NULL_HANDLE || out->layout != VK_NULL_HANDLE)
        return Graphics_ERROR_INVALID_ARGUMENT;
    Graphics_Result result = validate_pipeline(device, desc);
    if (result != Graphics_NONE)
        return result;
    Gpu_Pipeline_t pipeline = {0};
    VkDescriptorSetLayout layouts[GRAPHICS_MAX_SET_LAYOUTS] = {0};
    for (uint32_t i = 0U; i < desc->layout_count; ++i)
        layouts[i] = desc->layouts[i]->layout;
    VkPipelineLayoutCreateInfo layout = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                         .setLayoutCount = desc->layout_count,
                                         .pSetLayouts = layouts,
                                         .pushConstantRangeCount = desc->push_range_count,
                                         .pPushConstantRanges = desc->push_ranges};
    result =
        graphics_vk_result(device,
                           vkCreatePipelineLayout(device->device, &layout, NULL, &pipeline.layout),
                           Graphics_ERROR_RESOURCE);
    VkShaderModule modules[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    uint32_t stage_count = desc->fragment_words != 0U ? 2U : 1U;
    for (uint32_t i = 0U; i < stage_count && result == Graphics_NONE; ++i) {
        VkShaderModuleCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = (i == 0U ? desc->vertex_words : desc->fragment_words) * sizeof(uint32_t),
            .pCode = i == 0U ? desc->vertex_spirv : desc->fragment_spirv};
        result = graphics_vk_result(device,
                                    vkCreateShaderModule(device->device, &info, NULL, &modules[i]),
                                    Graphics_ERROR_RESOURCE);
    }
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = modules[0],
         .pName = desc->vertex_entry != NULL ? desc->vertex_entry : "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = modules[1],
         .pName = desc->fragment_entry != NULL ? desc->fragment_entry : "main"}};
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = desc->topology != NULL ? *desc->topology : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f};
    VkPipelineMultisampleStateCreateInfo samples = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo depth = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState attachments[GRAPHICS_MAX_COLOR_ATTACHMENTS] = {0};
    VkFormat formats[GRAPHICS_MAX_COLOR_ATTACHMENTS] = {0};
    for (uint32_t i = 0U; i < desc->color_count; ++i) {
        formats[i] = desc->colors[i].format;
        attachments[i] = desc->colors[i].blend;
    }
    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = desc->color_count,
        .pAttachments = attachments};
    const VkDynamicState dynamic[] = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
                                      VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
                                      VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
                                      VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
                                      VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
                                      VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
                                      VK_DYNAMIC_STATE_STENCIL_OP,
                                      VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
                                      VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
                                      VK_DYNAMIC_STATE_STENCIL_REFERENCE};
    VkPipelineDynamicStateCreateInfo dynamic_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = (uint32_t)(sizeof(dynamic) / sizeof(dynamic[0])),
        .pDynamicStates = dynamic};
    VkPipelineRenderingCreateInfo rendering = {.sType =
                                                   VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                               .colorAttachmentCount = desc->color_count,
                                               .pColorAttachmentFormats = formats,
                                               .depthAttachmentFormat = desc->depth_format,
                                               .stencilAttachmentFormat = desc->stencil_format};
    VkGraphicsPipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .stageCount = stage_count,
        .pStages = stages,
        .pVertexInputState = desc->vertex_input != NULL ? desc->vertex_input : &vertex_input,
        .pInputAssemblyState = &assembly,
        .pViewportState = &viewport,
        .pRasterizationState = desc->rasterization != NULL ? desc->rasterization : &raster,
        .pMultisampleState = &samples,
        .pDepthStencilState = &depth,
        .pColorBlendState = &blend,
        .pDynamicState = &dynamic_info,
        .layout = pipeline.layout,
        .basePipelineIndex = -1};
    if (result == Graphics_NONE)
        result = graphics_vk_result(
            device,
            vkCreateGraphicsPipelines(
                device->device, device->pipeline_cache, 1U, &info, NULL, &pipeline.pipeline),
            Graphics_ERROR_RESOURCE);
    for (uint32_t i = 0U; i < 2U; ++i)
        vkDestroyShaderModule(device->device, modules[i], NULL);
    if (result == Graphics_NONE) {
        pipeline.owner = device->device;
        pipeline.layout_count = desc->layout_count;
        pipeline.push_range_count = desc->push_range_count;
        pipeline.color_count = desc->color_count;
        pipeline.depth_format = desc->depth_format;
        pipeline.stencil_format = desc->stencil_format;
        for (uint32_t i = 0U; i < desc->layout_count; ++i) {
            pipeline.layouts[i] = desc->layouts[i];
            ++pipeline.layouts[i]->references;
        }
        for (uint32_t i = 0U; i < desc->push_range_count; ++i)
            pipeline.push_ranges[i] = desc->push_ranges[i];
        for (uint32_t i = 0U; i < desc->color_count; ++i)
            pipeline.colors[i] = formats[i];
        *out = pipeline;
        ++device->live_objects;
    } else {
        vkDestroyPipeline(device->device, pipeline.pipeline, NULL);
        vkDestroyPipelineLayout(device->device, pipeline.layout, NULL);
    }
    return result;
}

void graphics_command_defaults(Graphics_Command_t *command) {
    if (!command->state_valid) {
        vkCmdSetDepthTestEnable(command->command, VK_FALSE);
        vkCmdSetDepthWriteEnable(command->command, VK_TRUE);
        vkCmdSetDepthCompareOp(command->command, VK_COMPARE_OP_LESS);
        vkCmdSetStencilTestEnable(command->command, VK_FALSE);
        vkCmdSetStencilOp(command->command,
                          VK_STENCIL_FACE_FRONT_AND_BACK,
                          VK_STENCIL_OP_KEEP,
                          VK_STENCIL_OP_KEEP,
                          VK_STENCIL_OP_KEEP,
                          VK_COMPARE_OP_ALWAYS);
        vkCmdSetStencilCompareMask(command->command, VK_STENCIL_FACE_FRONT_AND_BACK, UINT32_MAX);
        vkCmdSetStencilWriteMask(command->command, VK_STENCIL_FACE_FRONT_AND_BACK, UINT32_MAX);
        vkCmdSetStencilReference(command->command, VK_STENCIL_FACE_FRONT_AND_BACK, 0U);
        command->depth_enabled = false;
        command->stencil_enabled = false;
        command->state_valid = true;
        command->device->stats.dynamic_state_commands += 8U;
    }
}

Graphics_Result graphics_bind_pipeline(Graphics_Command_t *command,
                                       const Gpu_Pipeline_t *pipeline) {
    if (command == NULL || pipeline == NULL || pipeline->owner != command->device->device ||
        pipeline->pipeline == VK_NULL_HANDLE)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!graphics_command_recording(command))
        return Graphics_ERROR_STATE;
    graphics_command_defaults(command);
    if (command->pipeline != pipeline) {
        vkCmdBindPipeline(command->command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        ++command->device->stats.pipeline_binds;
        memset(command->sets, 0, sizeof(command->sets));
        command->pipeline = pipeline;
    }
    return Graphics_NONE;
}

Graphics_Result graphics_bind_resources(Graphics_Command_t *command,
                                        uint32_t set_index,
                                        const Gpu_Binding_Set_t *set) {
    if (command == NULL || set == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!graphics_command_recording(command) || command->pipeline == NULL)
        return Graphics_ERROR_STATE;
    if (set_index >= command->pipeline->layout_count || set->set == VK_NULL_HANDLE ||
        set->owner != command->device->device ||
        set->layout != command->pipeline->layouts[set_index])
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (command->sets[set_index] != set->set) {
        vkCmdBindDescriptorSets(command->command,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                command->pipeline->layout,
                                set_index,
                                1U,
                                &set->set,
                                0U,
                                NULL);
        command->sets[set_index] = set->set;
    }
    return Graphics_NONE;
}

Graphics_Result graphics_push_constants(Graphics_Command_t *command,
                                        VkShaderStageFlags stages,
                                        uint32_t offset,
                                        uint32_t size,
                                        const void *data) {
    if (command == NULL || data == NULL || size == 0U || stages == 0U || offset % 4U != 0U ||
        size % 4U != 0U)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!graphics_command_recording(command) || command->pipeline == NULL)
        return Graphics_ERROR_STATE;
    VkShaderStageFlags covered = 0U;
    for (uint32_t i = 0U; i < command->pipeline->push_range_count; ++i) {
        const VkPushConstantRange *range = &command->pipeline->push_ranges[i];
        if (offset >= range->offset && offset - range->offset <= range->size &&
            size <= range->size - (offset - range->offset))
            covered |= range->stageFlags;
        /* Any stage whose range overlaps the write must be included (Vulkan push-constant
         * compatibility). */
        if ((uint64_t)offset < (uint64_t)range->offset + range->size &&
            (uint64_t)range->offset < (uint64_t)offset + size &&
            (stages & range->stageFlags) != range->stageFlags)
            return Graphics_ERROR_INVALID_ARGUMENT;
    }
    if ((covered & stages) != stages)
        return Graphics_OUT_OF_RANGE;
    vkCmdPushConstants(command->command, command->pipeline->layout, stages, offset, size, data);
    return Graphics_NONE;
}

Graphics_Result graphics_set_depth_test_enabled(Graphics_Command_t *command, bool enabled) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!graphics_command_recording(command))
        return Graphics_ERROR_STATE;
    graphics_command_defaults(command);
    if (command->depth_enabled != enabled) {
        vkCmdSetDepthTestEnable(command->command, enabled);
        command->depth_enabled = enabled;
        ++command->device->stats.dynamic_state_commands;
    }
    return Graphics_NONE;
}

Graphics_Result graphics_set_stencil_test_enabled(Graphics_Command_t *command, bool enabled) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!graphics_command_recording(command))
        return Graphics_ERROR_STATE;
    graphics_command_defaults(command);
    if (command->stencil_enabled != enabled) {
        vkCmdSetStencilTestEnable(command->command, enabled);
        command->stencil_enabled = enabled;
        ++command->device->stats.dynamic_state_commands;
    }
    return Graphics_NONE;
}

Graphics_Result
graphics_set_depth_state(Graphics_Command_t *command, bool write, VkCompareOp compare) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!graphics_command_recording(command))
        return Graphics_ERROR_STATE;
    if (compare < VK_COMPARE_OP_NEVER || compare > VK_COMPARE_OP_ALWAYS)
        return Graphics_ERROR_INVALID_ARGUMENT;
    graphics_command_defaults(command);
    vkCmdSetDepthWriteEnable(command->command, write);
    vkCmdSetDepthCompareOp(command->command, compare);
    command->device->stats.dynamic_state_commands += 2U;
    return Graphics_NONE;
}

Graphics_Result graphics_set_stencil_state(Graphics_Command_t *command,
                                           const VkStencilOpState *front,
                                           const VkStencilOpState *back) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!graphics_command_recording(command))
        return Graphics_ERROR_STATE;
    if (front == NULL || back == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    const VkStencilOpState *states[2] = {front, back};
    for (uint32_t i = 0U; i < 2U; ++i) {
        const VkStencilOpState *s = states[i];
        if (s->compareOp < VK_COMPARE_OP_NEVER || s->compareOp > VK_COMPARE_OP_ALWAYS ||
            s->failOp < VK_STENCIL_OP_KEEP || s->failOp > VK_STENCIL_OP_DECREMENT_AND_WRAP ||
            s->passOp < VK_STENCIL_OP_KEEP || s->passOp > VK_STENCIL_OP_DECREMENT_AND_WRAP ||
            s->depthFailOp < VK_STENCIL_OP_KEEP ||
            s->depthFailOp > VK_STENCIL_OP_DECREMENT_AND_WRAP)
            return Graphics_ERROR_INVALID_ARGUMENT;
    }
    graphics_command_defaults(command);
    for (uint32_t i = 0U; i < 2U; ++i) {
        VkStencilFaceFlags face = i == 0U ? VK_STENCIL_FACE_FRONT_BIT : VK_STENCIL_FACE_BACK_BIT;
        const VkStencilOpState *s = states[i];
        vkCmdSetStencilOp(
            command->command, face, s->failOp, s->passOp, s->depthFailOp, s->compareOp);
        vkCmdSetStencilCompareMask(command->command, face, s->compareMask);
        vkCmdSetStencilWriteMask(command->command, face, s->writeMask);
        vkCmdSetStencilReference(command->command, face, s->reference);
    }
    command->device->stats.dynamic_state_commands += 8U;
    return Graphics_NONE;
}

Graphics_Result graphics_bind_vertex_buffer(Graphics_Command_t *command,
                                            uint32_t binding,
                                            const Gpu_Buffer_t *buffer,
                                            VkDeviceSize offset) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!graphics_command_recording(command))
        return Graphics_ERROR_STATE;
    if (buffer == NULL || buffer->owner != command->device->device ||
        (buffer->usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) == 0U || offset >= buffer->size ||
        binding >= command->device->properties.limits.maxVertexInputBindings)
        return Graphics_ERROR_INVALID_ARGUMENT;
    vkCmdBindVertexBuffers(command->command, binding, 1U, &buffer->buffer, &offset);
    return Graphics_NONE;
}

Graphics_Result graphics_bind_index_buffer(Graphics_Command_t *command,
                                           const Gpu_Buffer_t *buffer,
                                           VkDeviceSize offset,
                                           VkIndexType type) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (!graphics_command_recording(command))
        return Graphics_ERROR_STATE;
    uint32_t stride = type == VK_INDEX_TYPE_UINT16 ? 2U : (type == VK_INDEX_TYPE_UINT32 ? 4U : 0U);
    if (buffer == NULL || buffer->owner != command->device->device ||
        (buffer->usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) == 0U || stride == 0U ||
        offset % stride != 0U || offset >= buffer->size)
        return Graphics_ERROR_INVALID_ARGUMENT;
    vkCmdBindIndexBuffer(command->command, buffer->buffer, offset, type);
    command->index_bytes = buffer->size - offset;
    command->index_stride = stride;
    return Graphics_NONE;
}

Graphics_Result graphics_check_draw(Graphics_Command_t *command) {
    if (command == NULL)
        return Graphics_ERROR_INVALID_ARGUMENT;
    if (command->phase != GRAPHICS_COMMAND_RENDERING || command->pipeline == NULL)
        return Graphics_ERROR_STATE;
    const Gpu_Pipeline_t *pipeline = command->pipeline;
    if (pipeline->color_count != command->color_count ||
        pipeline->depth_format != command->depth_format ||
        pipeline->stencil_format != command->stencil_format ||
        (command->depth_enabled && command->depth_format == VK_FORMAT_UNDEFINED) ||
        (command->stencil_enabled && command->stencil_format == VK_FORMAT_UNDEFINED))
        return Graphics_ERROR_RESOURCE;
    for (uint32_t i = 0U; i < pipeline->color_count; ++i)
        if (pipeline->colors[i] != command->colors[i])
            return Graphics_ERROR_RESOURCE;
    for (uint32_t i = 0U; i < pipeline->layout_count; ++i)
        if (command->sets[i] == VK_NULL_HANDLE)
            return Graphics_ERROR_RESOURCE;
    return Graphics_NONE;
}

Graphics_Result graphics_draw(Graphics_Command_t *command,
                              uint32_t vertices,
                              uint32_t instances,
                              uint32_t first_vertex,
                              uint32_t first_instance) {
    Graphics_Result result = graphics_check_draw(command);
    if (result == Graphics_NONE)
        vkCmdDraw(command->command, vertices, instances, first_vertex, first_instance);
    return result;
}

Graphics_Result graphics_draw_indexed(Graphics_Command_t *command,
                                      uint32_t indices,
                                      uint32_t instances,
                                      uint32_t first_index,
                                      int32_t vertex_offset,
                                      uint32_t first_instance) {
    Graphics_Result result = graphics_check_draw(command);
    if (result == Graphics_NONE &&
        (command->index_stride == 0U ||
         ((uint64_t)first_index + indices) * command->index_stride > command->index_bytes))
        result = Graphics_OUT_OF_RANGE;
    if (result == Graphics_NONE)
        vkCmdDrawIndexed(
            command->command, indices, instances, first_index, vertex_offset, first_instance);
    return result;
}
