#ifndef _UI_RENDER_H_
#define _UI_RENDER_H_

/** LOG -- Information
* 23/10/2024. Re-done ui renderer using the render backed layer abstraction layer
* 08/09/2025. Added a stop condition for gpu rendering and main thread computation while no event for the user is
	being given or an unfocusing event was set.
*/

#include "types.h"
#include "new_ui.h"
#include "render.h"

typedef struct ui_uniform ui_uniform;
struct ui_uniform {
    f32 TextureWidth;
    f32 TextureHeight;
    f32 ScreenWidth;
    f32 ScreenHeight;
	f32 IconTextureWidth;
	f32 IconTextureHeight;
};

typedef struct ucf_gui ucf_gui;
struct ucf_gui {
	ui_context*          Context;
	r_render             Render;
	ui_uniform           UniformData;
	draw_bucket_instance DrawInstance;

	hash_table           Fonts;

	Stack_Allocator      Allocator;
	Stack_Allocator      TempAllocator;

	u8* IconsBitmap;
	vec4 IconsUvCoords[Icon_Details + 1];

	R_Handle PipelineHandle;
	R_Handle ComputePipelineHandle;
	R_Handle FontTextureHandle;
	R_Handle ComputeTextureHandle;
	R_Handle IconsTextureHandle;
	R_Handle VBuffer[MAX_FRAMES_IN_FLIGHT];
	R_Handle IBuffer[MAX_FRAMES_IN_FLIGHT];
	R_Handle StagBuffer[MAX_FRAMES_IN_FLIGHT];
	R_Handle UBuffer;
	u32      IndexOrder[6];
};

fn_internal r_vertex_input_description Vertex2DInputDescription(Stack_Allocator* Allocator);

fn_internal ucf_gui UIRender_Start( vulkan_base* VkBase );

fn_internal void UIRender_Frame( ucf_gui* GUI );

#endif

#ifdef UI_RENDER_IMPL

fn_internal r_vertex_input_description
Vertex2DInputDescription(Stack_Allocator* Allocator)
{
	r_vertex_input_description VertexDescription;
	VertexDescription.Stride = sizeof(v_2d);
	VertexDescription.Rate = VK_VERTEX_INPUT_RATE_INSTANCE;
	VertexDescription.AttributeCount = 9;
	VertexDescription.Attributes = stack_push(Allocator, r_vertex_attribute, 9);
	VertexDescription.Attributes[0].Location = 0;
	VertexDescription.Attributes[0].Format   = R_FORMAT_VEC2;
	VertexDescription.Attributes[0].Offset    = 0;
	
	VertexDescription.Attributes[1].Location = 1;
	VertexDescription.Attributes[1].Format   = R_FORMAT_VEC2;
	VertexDescription.Attributes[1].Offset    = sizeof(vec2);
	
	VertexDescription.Attributes[2].Location = 2;
	VertexDescription.Attributes[2].Format   = R_FORMAT_VEC2;
	VertexDescription.Attributes[2].Offset    = 2 * sizeof(vec2);
	
	VertexDescription.Attributes[3].Location = 3;
	VertexDescription.Attributes[3].Format   = R_FORMAT_VEC2;
	VertexDescription.Attributes[3].Offset    = 3 * sizeof(vec2);
	
	VertexDescription.Attributes[4].Location = 4;
	VertexDescription.Attributes[4].Format   = R_FORMAT_VEC4;
	VertexDescription.Attributes[4].Offset    = 4 * sizeof(vec2);
	
	VertexDescription.Attributes[5].Location = 5;
	VertexDescription.Attributes[5].Format   = R_FORMAT_FLOAT;
	VertexDescription.Attributes[5].Offset    = 4 * sizeof(vec2) + sizeof(vec4);
	
	VertexDescription.Attributes[6].Location = 6;
	VertexDescription.Attributes[6].Format   = R_FORMAT_FLOAT;
	VertexDescription.Attributes[6].Offset    = 4 * sizeof(vec2) + sizeof(vec4) + sizeof(float);
	
	VertexDescription.Attributes[7].Location = 7;
	VertexDescription.Attributes[7].Format   = R_FORMAT_VEC2;
	VertexDescription.Attributes[7].Offset    = 4 * sizeof(vec2) + sizeof(vec4) + 2 * sizeof(float);
	
	VertexDescription.Attributes[8].Location = 8;
	VertexDescription.Attributes[8].Format   = R_FORMAT_VEC2;
	VertexDescription.Attributes[8].Offset    = 5 * sizeof(vec2) + sizeof(vec4) + 2 * sizeof(float);

	return VertexDescription;
}

#define ATLAS_W 4096
#define ATLAS_H 4096
#define ATLAS_BPP 4   // RGBA8

static int atlas_x = 0;
static int atlas_y = 0;
static int atlas_row_h = 0;

bool BuildIconAtlas(u8* atlas, int atlas_w, int atlas_h, const char** paths, int path_count, vec4* out_pixel_uvs)
{
    if (!atlas || !paths || !out_pixel_uvs)
        return false;

    // Clear atlas
    memset(atlas, 0, (size_t)atlas_w * atlas_h * 4);

    int pen_x = 0;
    int pen_y = 0;

    for (int i = 0; i < path_count; ++i) {
        int w, h, ch;
        u8 *data = stbi_load(paths[i], &w, &h, &ch, 4);
        if (!data) {
            printf("Failed to load: %s\n", paths[i]);
            return false;
        }

        // Wrap row
        if (pen_x + w > atlas_w) {
            pen_x = 0;
            pen_y += h;
        }

        // Check vertical space
        if (pen_y + h > atlas_h) {
            printf("Atlas too small for %s\n", paths[i]);
            stbi_image_free(data);
            return false;
        }

        // Correct copy: row by row into 2D buffer
        for (int row = 0; row < h; ++row) {
            u8 *dst = atlas + ((pen_y + row) * atlas_w + pen_x) * 4;
            u8 *src = data + row * w * 4;
            memcpy(dst, src, w * 4);
        }

        // Save pixel location in atlas
        out_pixel_uvs[i] = Vec4New(pen_x, pen_y, w, h);

		printf("first 4: %d %d %d %d\n", data[0], data[1], data[2], data[3]);
        printf("out_pixel_uvs[i]: %d %d %d %d\n", pen_x, pen_y, w, h);

        pen_x += w;
        stbi_image_free(data);
    }

    return true;
}

fn_internal ucf_gui UIRender_Start(vulkan_base* VkBase) {

	ucf_gui GUI;

	// Memory Allocation
    //
    Arena *main_arena = VkBase->Arena;

    u8 *Buffer = PushArray(main_arena, u8, gigabyte(1));
    u8 *TempBuffer = PushArray(main_arena, u8, gigabyte(1));
    stack_init(&GUI.Allocator, Buffer, gigabyte(1));
    stack_init(&GUI.TempAllocator, TempBuffer, gigabyte(1));

	// Font Creation
    // @todo: Change how to add fonts this is highly inconvenient
    //
	u8 *BitmapArray = stack_push(&GUI.Allocator, u8, 2100 * 1200);
	FontCache *DefaultFont, *TitleFont, *TitleFont2, *BoldFont, *ItalicFont;
	DefaultFont = stack_push(&GUI.Allocator, FontCache, 1);
	TitleFont   = stack_push(&GUI.Allocator, FontCache, 1);
	TitleFont2  = stack_push(&GUI.Allocator, FontCache, 1);
	BoldFont    = stack_push(&GUI.Allocator, FontCache, 1);
	ItalicFont  = stack_push(&GUI.Allocator, FontCache, 1);
	*DefaultFont = F_BuildFont(18, 2100, 60, BitmapArray, "./data/RobotoMono.ttf");
	*TitleFont   = F_BuildFont(22, 2100, 60, BitmapArray + (2100 * 60), "./data/LiterationMono.ttf");
	*TitleFont2  = F_BuildFont(22, 2100, 60, BitmapArray + (2100 * 120), "./data/TinosNerdFontPropo.ttf");
	*BoldFont    = F_BuildFont(15, 2100, 60, BitmapArray + (2100 * 180), "./data/LiterationMonoBold.ttf");
	*ItalicFont  = F_BuildFont(15, 2100, 60, BitmapArray + (2100 * 240), "./data/LiterationMonoItalic.ttf");

	DefaultFont->BitmapOffset = Vec2Zero();
	vec2 _off1 = {0, 60};
	TitleFont->BitmapOffset = _off1;
	vec2 _off2 = {0, 120};
	TitleFont2->BitmapOffset = _off2;
	vec2 _off3 = {0, 180};
	BoldFont->BitmapOffset = _off3;
	vec2 _off4 = {0, 240};
	ItalicFont->BitmapOffset = _off4;

	GUI.IconsBitmap = stack_push(&GUI.Allocator, u8, 4096 * 4096 * 4); //rgba
	memset(GUI.IconsBitmap, 0, 4096 * 4096 * 4);
	const char* icon_paths[] = {
		"./data/icons/archivo.png",
		"./data/icons/carpeta-abierta.png",
		"./data/icons/comunicacion.png",
		"./data/icons/enviar.png",
		"./data/icons/flecha-abajo.png",
		"./data/icons/sonido-de-onda.png",
		"./data/icons/ver-detalles.png"
	};

	if (!BuildIconAtlas(GUI.IconsBitmap, ATLAS_W, ATLAS_H, icon_paths, ArrayCount(icon_paths), GUI.IconsUvCoords)) {
		fprintf(stderr, "Failed to build icon atlas\n");
		// handle failure...
	}

	ui_context *UI_Context = stack_push(&GUI.Allocator, ui_context, 1);

	UI_Init(UI_Context, &GUI.Allocator, &GUI.TempAllocator);

	GUI.Context = UI_Context;
	memcpy(GUI.Context->IconsUvCoords, GUI.IconsUvCoords, (Icon_Details + 1) * sizeof(vec4));

	object_theme DefaultTheme = {};
    DefaultTheme.Border          = RgbaToNorm(HexToRGBA(0x333333FF)); // Subtle graphite border
    DefaultTheme.Background      = RgbaToNorm(HexToRGBA(0x050505FF)); // Deep "OLED" Black
    DefaultTheme.Foreground      = RgbaToNorm(HexToRGBA(0xCCCCCCFF)); // Off-white for secondary text
    DefaultTheme.Radius          = 6.0f;
    DefaultTheme.BorderThickness = 1.0f;
    DefaultTheme.Font            = DefaultFont;

    object_theme TitleTheme = DefaultTheme;
    TitleTheme.Font = TitleFont;
    // Header: Distinct dark slate to separate from content
    TitleTheme.Background = RgbaToNorm(HexToRGBA(0x121214FF)); 
    TitleTheme.Foreground = RgbaToNorm(HexToRGBA(0xFFFFFFFF)); // Pure White

    object_theme ButtonTheme = {};
    ButtonTheme.Border          = RgbaToNorm(HexToRGBA(0x444444FF));
    ButtonTheme.Background      = RgbaToNorm(HexToRGBA(0x222222FF)); // Standard interaction dark grey
    ButtonTheme.Foreground      = RgbaToNorm(HexToRGBA(0xFFFFFFFF)); 
    ButtonTheme.Radius          = 4.0f;
    ButtonTheme.BorderThickness = 1.0f;
    ButtonTheme.Font            = DefaultFont;

    object_theme PanelTheme = {};
    PanelTheme.Border          = RgbaToNorm(HexToRGBA(0x333333FF));
    PanelTheme.Background      = RgbaToNorm(HexToRGBA(0x0B0B0EFF)); // Dark blue-tinted black (Space)
    PanelTheme.Foreground      = RgbaToNorm(HexToRGBA(0xDDDDDDFF)); 
    PanelTheme.Radius          = 10.0f;
    PanelTheme.BorderThickness = 1.0f;
    PanelTheme.Font            = DefaultFont;

    object_theme InputTheme = {};
    InputTheme.Border          = RgbaToNorm(HexToRGBA(0x555555FF)); 
    InputTheme.Background      = RgbaToNorm(HexToRGBA(0x000000FF)); // Pure black for data entry
    InputTheme.Foreground      = RgbaToNorm(HexToRGBA(0xFFFFFFFF)); 
    InputTheme.Radius          = 4.0f;
    InputTheme.BorderThickness = 1.0f;
    InputTheme.Font            = DefaultFont;

    object_theme LabelTheme = {};
    LabelTheme.Border          = RgbaToNorm(HexToRGBA(0x00000000)); 
    LabelTheme.Background      = RgbaToNorm(HexToRGBA(0x00000000)); 
    LabelTheme.Foreground      = RgbaToNorm(HexToRGBA(0x999999FF)); // Dimmed telemetry labels
    LabelTheme.Radius          = 0.0f;
    LabelTheme.BorderThickness = 0.0f;
    LabelTheme.Font            = ItalicFont;

    object_theme ScrollbarTheme = {};
    ScrollbarTheme.Border          = RgbaToNorm(HexToRGBA(0x222222FF));
    ScrollbarTheme.Background      = RgbaToNorm(HexToRGBA(0x000000FF)); 
    ScrollbarTheme.Foreground      = RgbaToNorm(HexToRGBA(0x444444FF)); // Subtle handle
    ScrollbarTheme.Radius          = 2.0f;
    ScrollbarTheme.BorderThickness = 0.0f;
    ScrollbarTheme.Font            = NULL;

    // --- UI Context Assignment ---

    UI_Context->DefaultTheme = {};
    UI_Context->DefaultTheme.Window    = TitleTheme;
    UI_Context->DefaultTheme.Button    = ButtonTheme;
    UI_Context->DefaultTheme.Panel     = PanelTheme;
    UI_Context->DefaultTheme.Input     = InputTheme;
    UI_Context->DefaultTheme.Label     = LabelTheme;
    UI_Context->DefaultTheme.Scrollbar = ScrollbarTheme;

    // Global Color States
    UI_Context->DefaultTheme.OnDefaultBackground = RgbaToNorm(HexToRGBA(0x0B0B0EFF));
    UI_Context->DefaultTheme.OnDefaultForeground = RgbaToNorm(HexToRGBA(0xCCCCCCFF));
    UI_Context->DefaultTheme.OnDefaultBorder     = RgbaToNorm(HexToRGBA(0x333333FF));

    // Hover States (Telemetry Interaction)
    UI_Context->DefaultTheme.OnHoverBackground = RgbaToNorm(HexToRGBA(0x1A1A1DFF)); 
    UI_Context->DefaultTheme.OnHoverForeground = RgbaToNorm(HexToRGBA(0xFFFFFFFF));
    UI_Context->DefaultTheme.OnHoverBorder     = RgbaToNorm(HexToRGBA(0x007AFFFF)); // "Dragon Blue"

    // Success (Flight Stable)
    UI_Context->DefaultTheme.OnSuccessBackground = RgbaToNorm(HexToRGBA(0x00FF001A)); // Faint green tint
    UI_Context->DefaultTheme.OnSuccessForeground = RgbaToNorm(HexToRGBA(0x00FF00FF)); // Vibrant Terminal Green
    UI_Context->DefaultTheme.OnSuccessBorder     = RgbaToNorm(HexToRGBA(0x00FF00FF)); 

    // Error (Abort/Critical)
    UI_Context->DefaultTheme.OnErrorBackground = RgbaToNorm(HexToRGBA(0xFF00001A)); // Faint red tint
    UI_Context->DefaultTheme.OnErrorForeground = RgbaToNorm(HexToRGBA(0xFF3B30FF)); // High-Vis Red
    UI_Context->DefaultTheme.OnErrorBorder     = RgbaToNorm(HexToRGBA(0xFF3B30FF));

    // Warning (Caution)
    UI_Context->DefaultTheme.OnWarning = RgbaToNorm(HexToRGBA(0xFFCC00FF)); // Amber Alert

    // General Border States
    UI_Context->DefaultTheme.BorderPrimary = RgbaToNorm(HexToRGBA(0x333333FF));
    UI_Context->DefaultTheme.BorderMedium  = RgbaToNorm(HexToRGBA(0x555555FF));
    UI_Context->DefaultTheme.BorderHover   = RgbaToNorm(HexToRGBA(0x007AFFFF)); // Dragon Blue

	R_RenderInit(&GUI.Render, VkBase, &GUI.Allocator);
	VkDescriptorType Types[] = {
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
	VkDescriptorSetLayout Layout = R_CreateDescriptorSetLayout(&GUI.Render, ArrayCount(Types), Types, (VkShaderStageFlagBits)(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT));

	r_vertex_input_description VertexDescription = Vertex2DInputDescription(&GUI.Render.PerFrameAllocator);

	VkDescriptorSetLayout Layouts[] = { Layout };
	GUI.PipelineHandle = R_CreatePipeline(
		&GUI.Render,
		"My Pipeline",
		"./data/ui_render.vert.spv",
		"./data/ui_render.frag.spv",
		&VertexDescription,
		Layouts,
		1
	);

	VkDescriptorType CompTypes[]     = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
	VkDescriptorSetLayout CompLayout = R_CreateDescriptorSetLayout(&GUI.Render, 1, CompTypes, VK_SHADER_STAGE_COMPUTE_BIT);
	GUI.ComputePipelineHandle = R_CreateComputePipeline(
		&GUI.Render,
		"Compute Pipe",
		"./data/compute.comp.spv",
		&CompLayout,
		1
	);

	GUI.VBuffer[0] = R_CreateBuffer(&GUI.Render, "Buffer 1", 12 << 20, R_BUFFER_TYPE_VERTEX);
	GUI.VBuffer[1] = R_CreateBuffer(&GUI.Render, "Buffer 2", 12 << 20, R_BUFFER_TYPE_VERTEX);
	GUI.StagBuffer[0] = R_CreateBuffer(&GUI.Render, "Stag Buffer 1", 13 << 20, R_BUFFER_TYPE_STAGING);
	GUI.StagBuffer[1] = R_CreateBuffer(&GUI.Render, "Stag Buffer 2", 13 << 20, R_BUFFER_TYPE_STAGING);

	GUI.IBuffer[0] = R_CreateBuffer(&GUI.Render, "Index Buffer 1", 126, R_BUFFER_TYPE_INDEX);
	GUI.IBuffer[1] = R_CreateBuffer(&GUI.Render, "Index Buffer 2", 126, R_BUFFER_TYPE_INDEX);

	GUI.UBuffer = R_CreateBuffer(&GUI.Render, "Uniform Buffer", sizeof(ui_uniform), R_BUFFER_TYPE_UNIFORM);

	GUI.IndexOrder[0] = 0;
	GUI.IndexOrder[1] = 1;
	GUI.IndexOrder[2] = 3;
	GUI.IndexOrder[3] = 3;
	GUI.IndexOrder[4] = 2;
	GUI.IndexOrder[5] = 0;

	vk_image* FontTexture = stack_push(&GUI.Allocator, vk_image, 1);
	R_Handle FontTextHandle;
	// Image atlas creation for texture font
	//
	{
		*FontTexture = CreateImageData(VkBase, BitmapArray, (VkExtent3D) { 2100, 2100, 1 }, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
		VkSamplerCreateInfo sampler = {};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;

		vkCreateSampler(VkBase->Device, &sampler, 0, &FontTexture->Sampler);

		GUI.FontTextureHandle = R_PushTexture(&GUI.Render, "Fonts Atlas", FontTexture);
	}

	vk_image* ComputeImage = stack_push(&GUI.Allocator, vk_image, 1);
	R_Handle ComputeImageHandle;
	// Image for the compute pipeline
	{
		*ComputeImage = CreateImageDefault(
			VkBase,
			(VkExtent3D){ VkBase->Swapchain.Extent.width, VkBase->Swapchain.Extent.height, 1 },
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			false
		);

		GUI.ComputeTextureHandle = R_PushTexture(&GUI.Render, "Compute Texture", ComputeImage);
	}

	{
		vk_image* IconImage = stack_push(&GUI.Allocator, vk_image, 1);
		*IconImage = CreateImageData(VkBase, GUI.IconsBitmap, (VkExtent3D) { 4096, 4096, 1 }, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT, true);
		VkSamplerCreateInfo sampler = {};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;

		vkCreateSampler(VkBase->Device, &sampler, 0, &IconImage->Sampler);
		GUI.IconsTextureHandle = R_PushTexture(&GUI.Render, "Icon Atlas", IconImage);
	}

	GUI.DrawInstance = D_DrawInit(&GUI.TempAllocator);

	return GUI;
}

fn_internal void UIRender_Frame(ucf_gui* GUI) {
	r_render* Render    = &GUI->Render;
	vulkan_base* VkBase = Render->VulkanBase;
	draw_bucket_instance* DrawInstance = &GUI->DrawInstance;

	vk_image* ComputeImage = (vk_image*)HashTableGet(&Render->Textures, GUI->ComputeTextureHandle, 0);
	vk_image* FontTexture  = (vk_image*)HashTableGet(&Render->Textures, GUI->FontTextureHandle, 0);
	vk_image* IconTexture  = (vk_image*)HashTableGet(&Render->Textures, GUI->IconsTextureHandle, 0);

	u32 window_width  = Render->VulkanBase->Window.Width;
	u32 window_height = Render->VulkanBase->Window.Height;

	GUI->UniformData.ScreenWidth   = window_width;
	GUI->UniformData.ScreenHeight  = window_height;
	GUI->UniformData.TextureWidth  = FontTexture->Width;
	GUI->UniformData.TextureHeight = FontTexture->Height;
	GUI->UniformData.IconTextureWidth  = 4096;
	GUI->UniformData.IconTextureHeight = 4096;


	R_Begin(Render);

	if (Render->SetExternalResize) {
		vmaDestroyImage(VkBase->GPUAllocator, ComputeImage->Image, ComputeImage->Alloc);
		*ComputeImage = CreateImageDefault(
			VkBase,
			(VkExtent3D){ VkBase->Swapchain.Extent.width, VkBase->Swapchain.Extent.height, 1 },
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			false
		);
		return;
	}

	R_SendDataToBuffer(
		Render,
		GUI->StagBuffer[Render->VulkanBase->CurrentFrame],
		DrawInstance->Current2DBuffer.data,
		DrawInstance->Current2DBuffer.offset, 0
	);
	R_SendDataToBuffer(
		Render,
		GUI->StagBuffer[Render->VulkanBase->CurrentFrame],
		GUI->IndexOrder, 6 * sizeof(u32),
		DrawInstance->Current2DBuffer.offset
	);

	VkBufferCopy Copy = {};
	Copy.srcOffset = 0;
	Copy.dstOffset = 0;
	Copy.size = DrawInstance->Current2DBuffer.offset;
	R_CopyStageToBuffer(
		Render,
		GUI->StagBuffer[Render->VulkanBase->CurrentFrame],
		GUI->VBuffer[Render->VulkanBase->CurrentFrame],
		Copy
	);

	Copy = {};
	Copy.srcOffset = DrawInstance->Current2DBuffer.offset;
	Copy.dstOffset = 0;
	Copy.size = 6 * sizeof(u32);
	R_CopyStageToBuffer(
		Render,
		GUI->StagBuffer[Render->VulkanBase->CurrentFrame],
		GUI->IBuffer[Render->VulkanBase->CurrentFrame],
		Copy
	);
	R_SendCopyToGpu(Render);

	// Compute pipeline
	//
	{
		TransitionImageDefault(
			Render->CurrentCommandBuffer,
			ComputeImage->Image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL
		);
		R_BindTexture(Render, "Compute Texture", 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		R_DispatchCompute(Render, GUI->ComputePipelineHandle, ceilf(window_width / 32), ceilf(window_height / 32), 1);
	}

	//R_ClearScreen(Render, (vec4){0.02, 0.02, 0.02, 1.0f});

	// Render pass
	//
	R_SendImageToSwapchain(Render, GUI->ComputeTextureHandle);

	R_BeginRenderPass(Render);
	{
		R_BindTexture(Render, "Fonts Atlas", 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		R_BindTexture(Render, "Icon Atlas", 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		R_UpdateUniformBuffer(Render, "Uniform Buffer", 1, &GUI->UniformData, sizeof(ui_uniform));
		R_BindVertexBuffer(Render, GUI->VBuffer[Render->VulkanBase->CurrentFrame]);
		R_BindIndexBuffer(Render, GUI->IBuffer[Render->VulkanBase->CurrentFrame]);
		R_SetPipeline(Render, GUI->PipelineHandle);
		R_DrawIndexed(Render, 6, DrawInstance->Current2DBuffer.len);
	}
	R_RenderPassEnd(Render);
	R_RenderEnd(Render);

	if (Render->SetExternalResize) {
		vmaDestroyImage(VkBase->GPUAllocator, ComputeImage->Image, ComputeImage->Alloc);
		*ComputeImage = CreateImageDefault(
			VkBase,
			(VkExtent3D){ VkBase->Swapchain.Extent.width, VkBase->Swapchain.Extent.height, 1 },
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			false
		);
	}
}

#endif
