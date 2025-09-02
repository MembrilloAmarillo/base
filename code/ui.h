#ifndef _UI_H_
#define _UI_H_

#include "types.h"

typedef struct UI_Graphics UI_Graphics;
struct UI_Graphics {
    vulkan_base* base;
    allocated_buffer ui_buffer[MAX_FRAMES_IN_FLIGHT];   
    allocated_buffer ui_buffer_idx[MAX_FRAMES_IN_FLIGHT];
    allocated_buffer ui_vertex_staging[MAX_FRAMES_IN_FLIGHT];

    vector ui_rects; // type: v_2d
    vector ui_indxs; // type: u32

    // pipeline to render the ui
    //
    vk_pipeline                   UI_Pipeline;
    VkDescriptorSet               UI_DescriptorSet;
    VkDescriptorSetLayout         UI_DescriptorLayout;
    vk_image                      UI_TextureImage;
    VkDescriptorPool              UI_DescriptorPool;

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
};

internal UI_Graphics UI_GraphicsInit(vulkan_base* VkBase, FontCache* Font);

internal void UI_ExternalRecreateSwapchain( UI_Graphics* gfx );
internal void UI_CreateRenderComputePipeline( UI_Graphics* gfx );
internal void UI_CreateRenderDescriptorSet( UI_Graphics* gfx );
internal void UI_CreateUI_Pipeline( UI_Graphics* gfx, FontCache* UI_Font );
internal void UI_CreateComputePipeline( UI_Graphics* gfx );
internal void UI_CreateComputeBG_DescriptorSet( UI_Graphics* gfx );

internal mu_Context* UI_Init(Stack_Allocator* allocator, const char* path);
internal void UI_Begin(mu_Context* UI_Context);
internal void UI_End(UI_Graphics* gfx, mu_Context* UI_Context, Stack_Allocator* Allocator);

#endif 

#ifdef UI_IMPL

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
UI_CreateUI_Pipeline( UI_Graphics* gfx, FontCache* UI_Font ) {
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
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        binding_description.stride    = sizeof(v_2d);

        VkVertexInputAttributeDescription attribute_descriptions[3] = {
            // location = 0: pos (vec2)
            {
                .location = 0,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = 0,
            },
            // location = 1: uv (vec2)
            {
                .location = 1,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = sizeof(vec2),
            },
            // location = 2: color (vec4)
            {
                .location = 2,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset   = sizeof(vec2) * 2,
            },
        };
        SetVertexInputAttributeDescription(&p_Build, attribute_descriptions, 3);
        SetVertexInputBindingDescription(&p_Build, &binding_description, 1);

        gfx->UI_TextureImage = CreateImageData(
            gfx->base, 
            UI_Font->BitmapArray, 
            (VkExtent3D){UI_Font->BitmapWidth, UI_Font->BitmapHeight, 1}, 
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
    InitDescriptorSet(&builder, 1, &gfx->base->Allocator);
    AddBindingDescriptorSet(
        &builder, 
        0, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    );
    gfx->UI_DescriptorLayout = BuildDescriptorSet(
                                    &builder, 
                                    gfx->base->Device, 
                                    VK_SHADER_STAGE_FRAGMENT_BIT, 
                                    0, 
                                    0
                                );
    
    pool_size_ratio sizes[1];
    sizes[0] = (pool_size_ratio){VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8};
    InitDescriptorPool(gfx->base, &gfx->UI_DescriptorPool, gfx->base->Device, 24, sizes, 1);

    gfx->UI_DescriptorSet = DescriptorSetAllocate(&gfx->UI_DescriptorPool, gfx->base->Device, &gfx->UI_DescriptorLayout);
    
    descriptor_writer writer = DescriptorWriterInit(1, &gfx->base->TempAllocator);
    WriteImage(
        &writer, 0, 
        gfx->UI_TextureImage.ImageView, 
        gfx->UI_TextureImage.Sampler, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    );
    UpdateDescriptorSet(&writer, gfx->base->Device, gfx->UI_DescriptorSet);

    SetDescriptorLayout(&p_Build, &gfx->UI_DescriptorLayout);
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
    SetDescriptorLayout(&p_Build, &gfx->UI_DescriptorLayout);
    
    gfx->BG_RenderPipeline = AddPipeline(gfx->base, &p_Build, "./data/ColoredTriangle.vert.spv", "./data/ColoredTriangle.frag.spv");
}

internal void
UI_ExternalRecreateSwapchain( UI_Graphics* gfx )
{
	vkResetDescriptorPool(gfx->base->Device, gfx->base->GlobalDescriptorAllocator, 0);
	vkDestroyDescriptorSetLayout(gfx->base->Device, gfx->BG_DescriptorLayout, 0);
	vkDestroyImageView(gfx->base->Device, gfx->BG_TextureImage.ImageView, 0);
	vkDestroySampler(gfx->base->Device, gfx->BG_TextureImage.Sampler, 0);
	vmaDestroyImage(gfx->base->GPUAllocator, gfx->BG_TextureImage.Image, gfx->BG_TextureImage.Alloc);
	vkDestroyDescriptorSetLayout(gfx->base->Device, gfx->BG_RenderDescriptorLayout, 0);
	UI_CreateComputeBG_DescriptorSet(gfx);
	UI_CreateRenderDescriptorSet( gfx );
}

internal UI_Graphics 
UI_GraphicsInit(vulkan_base* VkBase, FontCache* Font) {
    UI_Graphics gfx = {0};
    gfx.base = VkBase;

    gfx.ui_buffer[0] = CreateBuffer(gfx.base->GPUAllocator, 24 << 20, (VkBufferUsageFlags)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), VMA_MEMORY_USAGE_GPU_ONLY);
    gfx.ui_buffer[1] = CreateBuffer(gfx.base->GPUAllocator, 24 << 20, (VkBufferUsageFlags)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), VMA_MEMORY_USAGE_GPU_ONLY);
    
    gfx.ui_buffer_idx[0] = CreateBuffer(gfx.base->GPUAllocator, 24 << 20, (VkBufferUsageFlags)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT), VMA_MEMORY_USAGE_GPU_ONLY);
    gfx.ui_buffer_idx[1] = CreateBuffer(gfx.base->GPUAllocator, 24 << 20, (VkBufferUsageFlags)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT), VMA_MEMORY_USAGE_GPU_ONLY);

    gfx.ui_vertex_staging[0] = CreateBuffer(gfx.base->GPUAllocator, 48 << 20, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    gfx.ui_vertex_staging[1] = CreateBuffer(gfx.base->GPUAllocator, 48 << 20, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    UI_CreateComputePipeline(&gfx);
    UI_CreateUI_Pipeline(&gfx, Font);
    UI_CreateRenderComputePipeline(&gfx);

    return gfx;
}


int MuF_TextWidth(mu_Font fc, const char* str, int str_len) {
    if( str_len == - 1) { str_len = strlen(str); }
    return F_TextWidth((FontCache*) fc, str, str_len);
}

int MuF_TextHeight(mu_Font fc) {
    return F_TextHeight((FontCache*)fc);
}

internal mu_Context* 
UI_Init(Stack_Allocator* allocator, const char* path) {
    mu_Context* UI_Context = stack_push(allocator, mu_Context, 1);
    mu_Style* theme_style = stack_push(allocator, mu_Style, 1);
    mu_Style style = {
        .font = 0,
        .size = { 68, 10 },
        .padding = 5, 
        .spacing = 4, 
        .indent = 24,
        .title_height = 24, 
        .scrollbar_size = 12, 
        .thumb_size = 8,
        .colors = {
            {240, 236, 218, 255}, /* MU_COLOR_TEXT */
            {31, 49, 74, 255}, /* MU_COLOR_BORDER */
            {21, 39, 64, 255}, /* MU_COLOR_WINDOWBG */
            {18, 30, 56, 255}, /* MU_COLOR_TITLEBG */
            {240, 206, 178, 255}, /* MU_COLOR_TITLETEXT */
            {19, 76, 101, 255}, /* MU_COLOR_PANELBG */
            {72, 127, 134, 255}, /* MU_COLOR_BUTTON */
            {19, 76, 101, 255}, /* MU_COLOR_BUTTONHOVER */
            {19, 76, 101, 255}, /* MU_COLOR_BUTTONFOCUS */
            {21, 39, 64, 255}, /* MU_COLOR_BASE */
            {42, 117, 154, 255}, /* MU_COLOR_BASEHOVER */
            {72, 127, 134, 255}, /* MU_COLOR_BASEFOCUS */
            {21, 39, 64, 255}, /* MU_COLOR_SCROLLBASE */
            {72, 127, 134, 255}  /* MU_COLOR_SCROLLTHUMB */
        },
    };
    memcpy(theme_style, &style, sizeof(mu_Style));

    u8* Bitmap = stack_push(allocator, u8, 2160 * 2160); //PushArray(VkBase.Arena, u8, 1200 * 1200);
    FontCache* Font = stack_push(allocator, FontCache, 1);
    FontCache UI_Font = F_BuildFont(22, 2160, 2160, Bitmap, path);
    memcpy(Font, &UI_Font, sizeof(UI_Font));
    //memcpy(Bitmap + (1200 * 1200), Bitmap, 1200 * 1200);
    //memcpy(Bitmap + 2 * (1200 * 1200), Bitmap, 1200 * 1200);
    //memcpy(Bitmap + 3 * (1200 * 1200), Bitmap, 1200 * 1200);
    mu_init(UI_Context);
    UI_Context->style = theme_style;
    UI_Context->style->font = Font;
    UI_Context->text_width  = MuF_TextWidth;
    UI_Context->text_height = MuF_TextHeight;

    return UI_Context;
}
internal void 
UI_Begin(mu_Context* UI_Context) {
    mu_begin(UI_Context);
}
internal void 
UI_End(UI_Graphics* gfx, mu_Context* UI_Context, Stack_Allocator* Allocator) {
    mu_end(UI_Context);

    void* V_Buffer = stack_push(Allocator, v_2d, kibibyte(256));
    gfx->ui_rects   = VectorNew(V_Buffer, 0, kibibyte(256), v_2d);
    void* I_Buffer = stack_push(Allocator, u32, kibibyte(256));
    gfx->ui_indxs   = VectorNew(I_Buffer, 0, kibibyte(256), u32);
    
    mu_Command* cmd = 0;

    F64 window_width  = gfx->base->Swapchain.Extent.width;
    F64 window_height = gfx->base->Swapchain.Extent.height;
    while (mu_next_command(UI_Context, &cmd)) {
        switch (cmd->type) {
            case MU_COMMAND_RECT: {
                u32 BaseVertexIdx = gfx->ui_rects.len;
                u32 idx[6] = {
                    BaseVertexIdx + 0, BaseVertexIdx + 1, BaseVertexIdx + 2, BaseVertexIdx + 2, BaseVertexIdx + 3, BaseVertexIdx + 0
                };
                f32 x_start = (((f32)cmd->rect.rect.x / (f32)window_width  ) * 2.0f) - 1.0f; 
                f32 y_start = (((f32)cmd->rect.rect.y / (f32)window_height ) * 2.0f) - 1.0f; 
                f32 x_end   = ((((f32)cmd->rect.rect.x + (f32)cmd->rect.rect.w) / (f32)window_width  ) * 2.0f) - 1.0f; 
                f32 y_end   = ((((f32)cmd->rect.rect.y + (f32)cmd->rect.rect.h) / (f32)window_height ) * 2.0f) - 1.0f; 
                
                v_2d v1 = {
                    .Position = {x_start, y_start},
                    .UV       = {-1, -1},
                    .Color    = {(f32)cmd->rect.color.r / 255.0, (f32)cmd->rect.color.g / 255.0, (f32)cmd->rect.color.b / 255.0, (f32)cmd->rect.color.a / 255.0},
                };
                v_2d v2 = {
                    .Position = {x_end, y_start},
                    .UV       = {-1, -1},
                    .Color    = {(f32)cmd->rect.color.r / 255.0, (f32)cmd->rect.color.g / 255.0, (f32)cmd->rect.color.b / 255.0, (f32)cmd->rect.color.a / 255.0},
                };
                v_2d v3 = {
                    .Position = {x_end, y_end},
                    .UV       = {-1, -1},
                    .Color    = {(f32)cmd->rect.color.r / 255.0, (f32)cmd->rect.color.g / 255.0, (f32)cmd->rect.color.b / 255.0, (f32)cmd->rect.color.a / 255.0},
                };
                v_2d v4 = {
                    .Position = {x_start, y_end},
                    .UV       = {-1, -1},
                    .Color    = {(f32)cmd->rect.color.r / 255.0, (f32)cmd->rect.color.g / 255.0, (f32)cmd->rect.color.b / 255.0, (f32)cmd->rect.color.a / 255.0},
                };

                if( gfx->ui_rects.len > gfx->ui_rects.capacity - 3 || gfx->ui_indxs.len > gfx->ui_indxs.capacity - 3 ) {
                    stack_free(Allocator, I_Buffer);
                    stack_free(Allocator, V_Buffer);
                    V_Buffer = stack_push(Allocator, v_2d, gfx->ui_rects.capacity * 2);
                    I_Buffer = stack_push(Allocator, u32, gfx->ui_indxs.capacity * 2);
                    VectorResize(&gfx->ui_rects, V_Buffer, gfx->ui_rects.capacity * 2);
                    VectorResize(&gfx->ui_indxs, I_Buffer, gfx->ui_indxs.capacity * 2);
                }

                VectorAppend(&gfx->ui_rects, &v1);
                VectorAppend(&gfx->ui_rects, &v2);
                VectorAppend(&gfx->ui_rects, &v3);
                VectorAppend(&gfx->ui_rects, &v4);
                for(u32 i = 0 ; i < 6; i += 1 ) {
                    VectorAppend(&gfx->ui_indxs, &idx[i]);
                }
            } break;
            case MU_COMMAND_TEXT: {
                vec4 Color = {(f32)cmd->text.color.r / 255.0, (f32)cmd->text.color.g / 255.0, (f32)cmd->text.color.b / 255.0, (f32)cmd->text.color.a / 255.0}; 
                f32 x_start = (((f32)cmd->text.pos.x / (f32)window_width ) * 2.0f) - 1.0f; 
                f32 y_start = (((f32)cmd->text.pos.y / (f32)window_height) * 2.0f) - 1.0f; 
                f32 txt_offset = 0;
                // Use unsigned char pointer to avoid signedness issues
                unsigned char *p = (unsigned char *)cmd->text.str;
                float pen_x = (float)cmd->text.pos.x;
                float pen_y = (float)cmd->text.pos.y; // baseline or top depending on your coordinate convention
                for (; *p; ++p) {
                    // Skip UTF-8 continuation bytes (0b10xxxxxx)
                    if ( (*p & 0xC0) == 0x80 ) continue;

                    unsigned char ch = *p;
                    if (ch < 32 || ch > 126) {
                        // handle fallback or skip
                        continue;
                    }

                    int gi = (int)ch - 32;
                    FontCache* UI_Font = (FontCache*)UI_Context->style->font;
                    f_Glyph g = UI_Font->glyph[gi];

                    // Position in pixel space:
                    // q.xoff / q.yoff are offsets from pen position as returned by stbtt
                    float x0_px = pen_x + g.x_off;
                    float y0_px = pen_y + g.y_off + UI_Font->line_height; // adjust if your pen_y is baseline: you may need -g.y_off etc.
                    float x1_px = x0_px + g.width;
                    float y1_px = y0_px + g.height;

                    // Convert pixel positions to NDC (assuming origin top-left and y increases down)
                    float ndc_x0 = (x0_px / (float)window_width)  * 2.0f - 1.0f;
                    float ndc_x1 = (x1_px / (float)window_width)  * 2.0f - 1.0f;
                    // If your NDC Y is +1 at top use below. If +1 at bottom invert.
                    float ndc_y0 = (y0_px / (float)window_height) * 2.0f - 1.0f;
                    float ndc_y1 = (y1_px / (float)window_height) * 2.0f - 1.0f;

                    // UVs: add half-texel offset to avoid bleeding when sampling with linear filter
                    float _u0 = (g.x) / (float)UI_Font->BitmapWidth ;
                    float _v0 = (g.y) / (float)UI_Font->BitmapHeight;
                    float _u1 = (g.x + g.width) / (float)UI_Font->BitmapWidth ;
                    float _v1 = (g.y + g.height) / (float)UI_Font->BitmapHeight;

                    // Create quad vertices in the same winding you use for indices
                    v_2d v1 = { .Position = { ndc_x0, ndc_y0 }, .UV = { _u0, _v0 }, .Color = Color };
                    v_2d v2 = { .Position = { ndc_x1, ndc_y0 }, .UV = { _u1, _v0 }, .Color = Color };
                    v_2d v3 = { .Position = { ndc_x1, ndc_y1 }, .UV = { _u1, _v1 }, .Color = Color };
                    v_2d v4 = { .Position = { ndc_x0, ndc_y1 }, .UV = { _u0, _v1 }, .Color = Color };

                    // Append verts & indices (use your existing capacity logic)
                    u32 base = gfx->ui_rects.len;
                    VectorAppend(&gfx->ui_rects, &v1);
                    VectorAppend(&gfx->ui_rects, &v2);
                    VectorAppend(&gfx->ui_rects, &v3);
                    VectorAppend(&gfx->ui_rects, &v4);

                    u32 idx[6] = { base+0, base+1, base+2, base+2, base+3, base+0 };
                    for (int k=0;k<6;k++) VectorAppend(&gfx->ui_indxs, &idx[k]);

                    // Advance pen by glyph advance (use xadvance from packing)
                    pen_x += g.advance;
                }
            } break;
            case MU_COMMAND_ICON: {} break;
            case MU_COMMAND_CLIP: {} break;
        }
    }
}

#endif
