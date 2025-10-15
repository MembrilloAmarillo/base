#ifndef _UI_RENDER_H_
#define _UI_RENDER_H_

/** LOG -- Information
  * 08/09/2025. Added a stop condition for gpu rendering and main thread computation while no event for the user is
                being given or an unfocusing event was set.
  */


typedef struct ui_uniform ui_uniform;
struct ui_uniform {
    f32 TextureWidth;
    f32 TextureHeight;
    f32 ScreenWidth;
    f32 ScreenHeight;
};

typedef struct UI_Graphics UI_Graphics;
struct UI_Graphics {
    vulkan_base* base;
    allocated_buffer ui_buffer[MAX_FRAMES_IN_FLIGHT];
    allocated_buffer ui_buffer_idx[MAX_FRAMES_IN_FLIGHT];
    allocated_buffer ui_vertex_staging[MAX_FRAMES_IN_FLIGHT];

    allocated_buffer UniformBuffer;
    ui_uniform       UniformData;

    vector ui_rects; // type: v_2d
    vector ui_indxs; // type: u32

    // pipeline to render the ui
    //
    vk_pipeline            UI_Pipeline;
    VkDescriptorSet        UI_DescriptorSet;
    VkDescriptorSetLayout  UI_DescriptorLayout;
    vk_image               UI_TextureImage;
    VkExtent3D             UI_TextureSize;
    VkDescriptorPool       UI_DescriptorPool;

    // pipeline to render the computed texture
    //
    vk_pipeline           BG_RenderPipeline;
    VkDescriptorSet       BG_RenderDescriptorSet;
    VkDescriptorSetLayout BG_RenderDescriptorLayout;

    // Compute pipeline to draw grid
    //
    vk_pipeline           BG_Pipeline;
    VkDescriptorSet       BG_DescriptorSet;
    VkDescriptorSetLayout BG_DescriptorLayout;
    vk_image              BG_TextureImage;

    vec2 LastMousePosition;
    bool UI_EnableDebug;
};

internal UI_Graphics UI_GraphicsInit(vulkan_base* VkBase, u8* Bitmap, u32 Width, u32 Height);

internal void UI_Render(UI_Graphics* gfx, VkCommandBuffer cmd);

internal void UI_ExternalRecreateSwapchain( UI_Graphics* gfx );
internal void UI_CreateRenderComputePipeline( UI_Graphics* gfx );
internal void UI_CreateRenderDescriptorSet( UI_Graphics* gfx );
internal void UI_CreateUI_Pipeline( UI_Graphics* gfx, u8* Bitmap, u32 Width, u32 Height);
internal void UI_CreateComputePipeline( UI_Graphics* gfx );
internal void UI_CreateComputeBG_DescriptorSet( UI_Graphics* gfx );

#endif

#ifdef UI_RENDER_IMPL

internal void
UI_Render(UI_Graphics* gfx, VkCommandBuffer cmd) {

	{
		gfx->UniformData.ScreenWidth   = gfx->base->Swapchain.Extent.width;
		gfx->UniformData.ScreenHeight  = gfx->base->Swapchain.Extent.height;
		gfx->UniformData.TextureWidth  = gfx->UI_TextureImage.Width;
		gfx->UniformData.TextureHeight = gfx->UI_TextureImage.Height;
		VkMappedMemoryRange flushRange = {0};
		flushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		// Get the VkDeviceMemory from your buffer's allocation info
		flushRange.memory = gfx->UniformBuffer.Info.deviceMemory;
		flushRange.offset = 0;
		flushRange.size = VK_WHOLE_SIZE; // Or sizeof(ui_uniform)
		vkFlushMappedMemoryRanges(gfx->base->Device, 1, &flushRange);
		memcpy(
			gfx->UniformBuffer.Info.pMappedData,
			&gfx->UniformData,
			sizeof(ui_uniform)
		);
	}

    {
        memcpy(
               gfx->ui_vertex_staging[gfx->base->CurrentFrame].Info.pMappedData,
               gfx->ui_rects.data,
               gfx->ui_rects.offset
               );

        void* data = gfx->ui_vertex_staging[gfx->base->CurrentFrame].Info.pMappedData;
        void* data_offset = (u8*)data + gfx->ui_rects.offset;

        memcpy(
               data_offset,
               gfx->ui_indxs.data,
               gfx->ui_indxs.offset
               );

        VkBufferCopy vertexCopy = {0};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = gfx->ui_rects.offset;

        vkCmdCopyBuffer(
                        cmd,
                        gfx->ui_vertex_staging[gfx->base->CurrentFrame].Buffer,
                        gfx->ui_buffer[gfx->base->CurrentFrame].Buffer,
                        1,
                        &vertexCopy
                        );

        VkBufferCopy indexCopy = {0};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = gfx->ui_rects.offset;
        indexCopy.size      = gfx->ui_indxs.offset;

        if( indexCopy.srcOffset + indexCopy.size > gfx->ui_buffer_idx[gfx->base->CurrentFrame].Info.size ) {
            fprintf(stderr, "[VULKAN_ERROR] Copy size is greater that destination size %d\n", gfx->ui_buffer_idx[gfx->base->CurrentFrame].Info.size);
        }

        vkCmdCopyBuffer(
                        cmd,
                        gfx->ui_vertex_staging[gfx->base->CurrentFrame].Buffer,
                        gfx->ui_buffer_idx[gfx->base->CurrentFrame].Buffer,
                        1,
                        &indexCopy
                        );

        // Add pipeline barrier before vertex input
        VkMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
        };

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                             0, 1, &barrier, 0, 0, 0, 0);
    }
    {
        TransitionImage(
                        cmd,
                        gfx->BG_TextureImage.Image,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_GENERAL,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_WRITE_BIT
                        );

        // bind the gradient drawing compute pipeline
        vkCmdBindPipeline(
                          cmd,
                          VK_PIPELINE_BIND_POINT_COMPUTE,
                          gfx->BG_Pipeline.Pipeline
                          );

        // bind the descriptor set containing the draw image for the compute pipeline
        vkCmdBindDescriptorSets(
                                cmd,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                gfx->BG_Pipeline.Layout,
                                0, 1,
                                &gfx->BG_DescriptorSet,
                                0, 0
                                );

        VkExtent3D Extent = gfx->BG_TextureImage.Extent;
        // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
        vkCmdDispatch(
                      cmd,
                      ceil((f32)Extent.width / 32.0),
                      ceil((f32)Extent.height / 32.0),
                      1
                      );

        TransitionImage(
                        cmd,
                        gfx->BG_TextureImage.Image,
                        VK_IMAGE_LAYOUT_GENERAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT
                        );
    }

    VkClearValue clear_value = (VkClearValue){
        .color = (VkClearColorValue){ .float32 = {12.0 / 255.0, 12.0 / 255.0, 12.0 / 255.0, 255.0 / 255.0} },
    };

    VkExtent2D Extent2d = gfx->base->Swapchain.Extent;
    VkRenderingAttachmentInfo  colorAttachment = AttachmentInfo(gfx->base->Swapchain.ImageViews[gfx->base->SwapchainImageIdx], &clear_value, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo                 = RenderingInfo( Extent2d, &colorAttachment, NULL );
    vkCmdBeginRendering(cmd, &renderInfo);
    {
        vkCmdBindPipeline(
                          cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          gfx->BG_RenderPipeline.Pipeline
                          );
        vkCmdBindDescriptorSets(
                                cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                gfx->BG_RenderPipeline.Layout,
                                0, 1,
                                &gfx->BG_RenderDescriptorSet,
                                0, 0
                                );
        VkViewport viewport = {0};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width  = gfx->base->Swapchain.Extent.width;
        viewport.height = gfx->base->Swapchain.Extent.height;
        viewport.minDepth = 0;
        viewport.maxDepth = 1;

        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = gfx->base->Swapchain.Extent
        };
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    ///////////////////////////////////////////////////////////
    // UI Rendering
    {
        VkExtent2D Extent = gfx->base->Swapchain.Extent;
        vkCmdBindPipeline(
                          cmd,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          gfx->UI_Pipeline.Pipeline
                          );

        vkCmdBindDescriptorSets(
                                cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                gfx->UI_Pipeline.Layout, 0, 1,
                                &gfx->UI_DescriptorSet, 0, 0
                                );

        VkBuffer Buffers[] = {gfx->ui_buffer[gfx->base->CurrentFrame].Buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(
                               cmd, 0, 1,
                               Buffers,
                               offsets
                               );
        vkCmdBindIndexBuffer(
                             cmd,
                             gfx->ui_buffer_idx[gfx->base->CurrentFrame].Buffer,
                             0,
                             VK_INDEX_TYPE_UINT32
                             );

        // set dynamic viewport and scissor
        VkViewport viewport = {0};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width  = Extent.width;
        viewport.height = Extent.height;
        viewport.minDepth = 0;
        viewport.maxDepth = 1;

        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor = {0};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width  = Extent.width;
        scissor.extent.height = Extent.height;

        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdDrawIndexed(cmd, 6, gfx->ui_rects.len, 0, 0, 0);
    }

    vkCmdEndRendering(cmd);

    TransitionImageDefault(
                           cmd,
                           gfx->base->Swapchain.Images[gfx->base->SwapchainImageIdx],
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                           );
    VectorClear(&gfx->ui_rects);
    VectorClear(&gfx->ui_indxs);
}

internal void
UI_CreateComputeBG_DescriptorSet( UI_Graphics* gfx ) {
    // Create storage image for background texture
    gfx->BG_TextureImage = CreateImageDefault(
                                              gfx->base,
                                              (VkExtent3D){
                                                  .width = gfx->base->Swapchain.Extent.width,
                                                  .height = gfx->base->Swapchain.Extent.height,
                                                  .depth = 1
                                              },
                                              VK_FORMAT_R32G32B32A32_SFLOAT,
                                              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                              false
                                              );

    // Setup descriptor set for background texture
    vk_descriptor_set builder;
    InitDescriptorSet(&builder, 1, &gfx->base->Allocator);
    AddBindingDescriptorSet(&builder, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    gfx->BG_DescriptorLayout = BuildDescriptorSet(&builder,
                                                  gfx->base->Device,
                                                  VK_SHADER_STAGE_COMPUTE_BIT,
                                                  NULL,
                                                  0);

    gfx->BG_DescriptorSet = DescriptorSetAllocate(
                                                  &gfx->base->GlobalDescriptorAllocator,
                                                  gfx->base->Device,
                                                  &gfx->BG_DescriptorLayout);

    descriptor_writer writer = DescriptorWriterInit(1, &gfx->base->TempAllocator);

    WriteImage(&writer, 0,
               gfx->BG_TextureImage.ImageView,
               VK_NULL_HANDLE,
               VK_IMAGE_LAYOUT_GENERAL,
               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    UpdateDescriptorSet(&writer,
                        gfx->base->Device,
                        gfx->BG_DescriptorSet);
}

///////////////////////////////////////////////////////////////////////
// Create compute pipeline for grid
internal void
UI_CreateComputePipeline( UI_Graphics* gfx ) {

    UI_CreateComputeBG_DescriptorSet(gfx);

    // Create pipeline layout for compute pipeline
    VkPipelineLayoutCreateInfo computeLayout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .setLayoutCount = 1,
        .pSetLayouts = &gfx->BG_DescriptorLayout
    };

    VK_CHECK(vkCreatePipelineLayout(gfx->base->Device, &computeLayout, NULL, &gfx->BG_Pipeline.Layout));

    // Load compute shader module
    VkShaderModule computeDrawShader;
    if (!LoadShaderModule("./data/compute.comp.spv", gfx->base->Device, &computeDrawShader)) {
        fprintf(stderr, "[ERROR] Could not find shader!\n");
        exit(EXIT_FAILURE);
    }

    // Configure shader stage
    VkPipelineShaderStageCreateInfo stageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeDrawShader,
        .pName = "main"
    };

    // Create compute pipeline info
    VkComputePipelineCreateInfo computePipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .layout = gfx->BG_Pipeline.Layout,
        .stage = stageInfo
    };

    // Create compute pipeline
    VK_CHECK(vkCreateComputePipelines(gfx->base->Device, VK_NULL_HANDLE, 1,
                                      &computePipelineCreateInfo, NULL,
                                      &gfx->BG_Pipeline.Pipeline));

    // Clean up shader module
    vkDestroyShaderModule(gfx->base->Device, computeDrawShader, NULL);
}

///////////////////////////////////////////////////////////////////////
// Create graphic pipeline for ui rendering
internal void
UI_CreateUI_Pipeline( UI_Graphics* gfx, u8* Bitmap, u32 Width, u32 Height ) {
    pipeline_builder p_Build = InitPipelineBuilder(2, &gfx->base->Allocator);
    SetInputTopology(&p_Build, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    SetPolygonMode(&p_Build, VK_POLYGON_MODE_FILL);
    SetCullMode(&p_Build, 0, VK_FRONT_FACE_CLOCKWISE);
    EnableBlendingAlphaBlend(&p_Build);
    DisableDepthTest(&p_Build);
    SetMultisamplingNone(&p_Build);
    {
        VkVertexInputBindingDescription binding_description = {0};
        binding_description.binding   = 0;
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        binding_description.stride    = sizeof(v_2d);

        VkVertexInputAttributeDescription attribute_descriptions[] = {
            // location = 0: LeftCorner (vec2)
            {
                .location = 0,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = 0,
            },
            // location = 1: Size (vec2)
            {
                .location = 1,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = sizeof(vec2),
            },
            // location = 2: UV (vec2)
            {
                .location = 2,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = 2 * sizeof(vec2),
            },
            // location = 3: UVSize (vec2)
            {
                .location = 3,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = 3 * sizeof(vec2),
            },
            // location = 3: Color (vec4)
            {
                .location = 4,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset   = 4 * sizeof(vec2),
            },
            {
                .location = 5,
                .binding  = 0,
                .format   = VK_FORMAT_R32_SFLOAT,
                .offset   = 4 * sizeof(vec2) + sizeof(vec4),
            },
            {
                .location = 6,
                .binding  = 0,
                .format   = VK_FORMAT_R32_SFLOAT,
                .offset   = 4 * sizeof(vec2) + sizeof(vec4) + sizeof(float),
            },
        };
        SetVertexInputAttributeDescription(&p_Build, attribute_descriptions, 7);
        SetVertexInputBindingDescription(&p_Build, &binding_description, 1);

        gfx->UI_TextureImage = CreateImageData(
                                               gfx->base,
                                               Bitmap,
                                               (VkExtent3D){Width, Height, 1},
                                               VK_FORMAT_R8_UNORM,
                                               VK_IMAGE_USAGE_SAMPLED_BIT,
                                               false
                                               );
    }

    VkSamplerCreateInfo sampler = {0};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;

    vkCreateSampler(gfx->base->Device, &sampler, 0, &gfx->UI_TextureImage.Sampler);
    vk_descriptor_set builder = {0};
    InitDescriptorSet(&builder, 2, &gfx->base->Allocator);
    AddBindingDescriptorSet(
        &builder,
        0,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    );
    AddBindingDescriptorSet(
        &builder,
        1,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    );
    gfx->UI_DescriptorLayout = BuildDescriptorSet(
        &builder,
        gfx->base->Device,
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        0,
        0
    );

    pool_size_ratio sizes[2];
    sizes[0] = (pool_size_ratio){VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3};
    sizes[1] = (pool_size_ratio){VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3};
    InitDescriptorPool(gfx->base, &gfx->UI_DescriptorPool, gfx->base->Device, 6, sizes, 2);

    gfx->UI_DescriptorSet = DescriptorSetAllocate(&gfx->UI_DescriptorPool, gfx->base->Device, &gfx->UI_DescriptorLayout);

    descriptor_writer writer = DescriptorWriterInit(2, &gfx->base->TempAllocator);

    WriteImage(
               &writer, 0,
               gfx->UI_TextureImage.ImageView,
               gfx->UI_TextureImage.Sampler,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
               );

    gfx->UniformBuffer = CreateBuffer(
        gfx->base->GPUAllocator,
        sizeof(ui_uniform),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    WriteBuffer(&writer, 1, gfx->UniformBuffer.Buffer, sizeof(ui_uniform), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    UpdateDescriptorSet(&writer, gfx->base->Device, gfx->UI_DescriptorSet);

    SetDescriptorLayout(&p_Build, &gfx->UI_DescriptorLayout, 2);
    gfx->UI_Pipeline = AddPipeline(
                                   gfx->base,
                                   &p_Build,
                                   "./data/ui_render.vert.spv",
                                   "./data/ui_render.frag.spv"
                                   );
}

internal void
UI_CreateRenderDescriptorSet( UI_Graphics* gfx )
{
    vk_descriptor_set builder;
    InitDescriptorSet(&builder, 1, &gfx->base->Allocator);
    AddBindingDescriptorSet(&builder, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    gfx->BG_RenderDescriptorLayout = BuildDescriptorSet(&builder, gfx->base->Device, VK_SHADER_STAGE_FRAGMENT_BIT, NULL, 0);
    gfx->BG_RenderDescriptorSet = DescriptorSetAllocate(&gfx->base->GlobalDescriptorAllocator, gfx->base->Device, &gfx->BG_RenderDescriptorLayout);

    VkSamplerCreateInfo sampler = {0};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;

    vkCreateSampler(gfx->base->Device, &sampler, 0, &gfx->BG_TextureImage.Sampler);

    descriptor_writer Writer = DescriptorWriterInit(1, &gfx->base->Allocator);
    WriteImage(
               &Writer, 0,
               gfx->BG_TextureImage.ImageView,
               gfx->BG_TextureImage.Sampler,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
               );

    UpdateDescriptorSet(&Writer, gfx->base->Device, gfx->BG_RenderDescriptorSet);
}

///////////////////////////////////////////////////////////////////////
// Initialize the pipeline for rendering the computed texture
//
internal void
UI_CreateRenderComputePipeline( UI_Graphics* gfx ) {

    pipeline_builder p_Build = InitPipelineBuilder(2, &gfx->base->Allocator);
    SetInputTopology(&p_Build, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    SetPolygonMode(&p_Build, VK_POLYGON_MODE_FILL);
    SetCullMode(&p_Build, 0, VK_FRONT_FACE_CLOCKWISE);
    EnableBlendingAlphaBlend(&p_Build);
    DisableDepthTest(&p_Build);
    SetMultisamplingNone(&p_Build);

    UI_CreateRenderDescriptorSet(gfx);
    SetDescriptorLayout(&p_Build, &gfx->BG_RenderDescriptorLayout, 1);

    gfx->BG_RenderPipeline = AddPipeline(gfx->base, &p_Build, "./data/ColoredTriangle.vert.spv", "./data/ColoredTriangle.frag.spv");
}

internal void
UI_ExternalRecreateSwapchain( UI_Graphics* gfx )
{
	//vkResetDescriptorPool(gfx->base->Device, gfx->base->GlobalDescriptorAllocator, 0);
	vkDestroyDescriptorSetLayout(gfx->base->Device, gfx->BG_DescriptorLayout, 0);
	vkDestroyImageView(gfx->base->Device, gfx->BG_TextureImage.ImageView, 0);
	vkDestroySampler(gfx->base->Device, gfx->BG_TextureImage.Sampler, 0);
	vmaDestroyImage(gfx->base->GPUAllocator, gfx->BG_TextureImage.Image, gfx->BG_TextureImage.Alloc);
	vkDestroyDescriptorSetLayout(gfx->base->Device, gfx->BG_RenderDescriptorLayout, 0);
	UI_CreateComputeBG_DescriptorSet(gfx);
	UI_CreateRenderDescriptorSet( gfx );
}

internal UI_Graphics
UI_GraphicsInit(vulkan_base* VkBase, u8* Bitmap, u32 Width, u32 Height) {
    UI_Graphics gfx = {0};
    gfx.base = VkBase;

    gfx.ui_buffer[0] = CreateBuffer(gfx.base->GPUAllocator, 24 << 20, (VkBufferUsageFlags)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), VMA_MEMORY_USAGE_GPU_ONLY);
    gfx.ui_buffer[1] = CreateBuffer(gfx.base->GPUAllocator, 24 << 20, (VkBufferUsageFlags)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), VMA_MEMORY_USAGE_GPU_ONLY);

    gfx.ui_buffer_idx[0] = CreateBuffer(gfx.base->GPUAllocator, 24 << 20, (VkBufferUsageFlags)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT), VMA_MEMORY_USAGE_GPU_ONLY);
    gfx.ui_buffer_idx[1] = CreateBuffer(gfx.base->GPUAllocator, 24 << 20, (VkBufferUsageFlags)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT), VMA_MEMORY_USAGE_GPU_ONLY);

    gfx.ui_vertex_staging[0] = CreateBuffer(gfx.base->GPUAllocator, 48 << 20, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    gfx.ui_vertex_staging[1] = CreateBuffer(gfx.base->GPUAllocator, 48 << 20, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    UI_CreateComputePipeline(&gfx);
    UI_CreateUI_Pipeline(&gfx, Bitmap, Width, Height);
    UI_CreateRenderComputePipeline(&gfx);

    return gfx;
}


#endif
