#if __linux__
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <unistd.h>
#define VK_USE_PLATFORM_XLIB_KHR

#elif _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <stddef.h>

#include "../third-party/xxhash.h"
#include "../third-party/stb_image.h"
#include "../third-party/stb_truetype.h"

#include "../types.h"
#include "../memory.h"
#include "../allocator.h"
#include "../strings.h"
#include "../vector.h"
#include "../queue.h"
#include "../files.h"
#include "../HashTable.h"

#include "../load_font.h"
#include "../window_creation.h"
#include "../third-party/vk_mem_alloc.h"
#include "../vk_render.h"
#include "../render.h"
#include "../draw.h"

#define MEMORY_IMPL
#include "../memory.h"

#define ALLOCATOR_IMPL
#include "../allocator.h"

#define STRINGS_IMPL
#include "../strings.h"

#define VECTOR_IMPL
#include "../vector.h"

#define QUEUE_IMPL
#include "../queue.h"

#define FILES_IMPL
#include "../files.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../third-party/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third-party/stb_truetype.h"

#define LOAD_FONT_IMPL
#include "../load_font.h"

#define WINDOW_CREATION_IMPL
#include "../window_creation.h"

#define VK_RENDER_IMPL
#include "../vk_render.h"

#include "../HashTable.h"
#include "../HashTable.c"

#include "../render.c"

#define DRAW_IMPL
#include "../draw.h"
typedef struct ui_uniform ui_uniform;
struct ui_uniform {
    f32 TextureWidth;
    f32 TextureHeight;
    f32 ScreenWidth;
    f32 ScreenHeight;
};

fn_internal r_vertex_input_description Vertex2DInputDescription( Stack_Allocator* Allocator );

int main()
{
	vulkan_base VkBase = VulkanInit();

	// Memory Allocation
    //
    Arena *main_arena = VkBase.Arena;
    Temp arena_temp = TempBegin(main_arena);
    
    Stack_Allocator Allocator;
    Stack_Allocator TempAllocator;
    u8 *Buffer = PushArray(main_arena, u8, gigabyte(1));
    u8 *TempBuffer = PushArray(main_arena, u8, gigabyte(1));
    stack_init(&Allocator, Buffer, gigabyte(1));
    stack_init(&TempAllocator, TempBuffer, gigabyte(1));

	// Font Creation
	//
	u8 *BitmapArray = stack_push(&Allocator, u8, 2100 * 1200);
    FontCache DefaultFont =
        F_BuildFont(22, 2100, 60, BitmapArray, "./data/RobotoMono.ttf");
    FontCache TitleFont = F_BuildFont(22, 2100, 60, BitmapArray + (2100 * 60),
                                      "./data/LiterationMono.ttf");
    FontCache TitleFont2 = F_BuildFont(22, 2100, 60, BitmapArray + (2100 * 120),
                                       "./data/TinosNerdFontPropo.ttf");
    FontCache BoldFont = F_BuildFont(18, 2100, 60, BitmapArray + (2100 * 180),
                                     "./data/LiterationMonoBold.ttf");
    FontCache ItalicFont = F_BuildFont(18, 2100, 60, BitmapArray + (2100 * 240),
                                       "./data/LiterationMonoItalic.ttf");
    DefaultFont.BitmapOffset = Vec2Zero();
    TitleFont.BitmapOffset = (vec2){0, 60};
    TitleFont2.BitmapOffset = (vec2){0, 120};
    BoldFont.BitmapOffset = (vec2){0, 180};
    ItalicFont.BitmapOffset = (vec2){0, 240};

	// Renderer creation
	//
	{
		r_render Render;
		R_RenderInit(&Render, &VkBase, &Allocator);
		VkDescriptorType Types[] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
		VkDescriptorSetLayout Layout = R_CreateDescriptorSetLayout(&Render, 2, Types, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		r_vertex_input_description VertexDescription = Vertex2DInputDescription(&Render.PerFrameAllocator);

		VkDescriptorSetLayout Layouts[] = { Layout };
		R_Handle Pipeline = R_CreatePipeline(
			&Render,
			"My Pipeline",
			"./data/ui_render.vert.spv",
			"./data/ui_render.frag.spv",
			&VertexDescription,
			Layouts,
			1
		);

		VkDescriptorType CompTypes[]     = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
		VkDescriptorSetLayout CompLayout = R_CreateDescriptorSetLayout(&Render, 1, CompTypes, VK_SHADER_STAGE_COMPUTE_BIT);
		R_Handle ComputePipeline = R_CreateComputePipeline(
			&Render,
			"Compute Pipe",
			"./data/compute.comp.spv",
			&CompLayout,
			1
		);
		
		R_Handle VBuffer[] = {
			R_CreateBuffer(&Render, "Buffer 1", 12 << 10, R_BUFFER_TYPE_VERTEX),
			R_CreateBuffer(&Render, "Buffer 2", 12 << 10, R_BUFFER_TYPE_VERTEX)
		};
		R_Handle StagBuffer[] =
		{ 
			R_CreateBuffer(&Render, "Stag Buffer 1", 12 << 10, R_BUFFER_TYPE_STAGING),
			R_CreateBuffer(&Render, "Stag Buffer 2", 12 << 10, R_BUFFER_TYPE_STAGING)
		};
		R_Handle IBuffer[] = {
			R_CreateBuffer(&Render, "Index Buffer 1", 126, R_BUFFER_TYPE_INDEX),
			R_CreateBuffer(&Render, "Index Buffer 2", 126, R_BUFFER_TYPE_INDEX)
		};
		R_Handle UBuffer = R_CreateBuffer(&Render, "Uniform Buffer", sizeof(ui_uniform), R_BUFFER_TYPE_UNIFORM);

		u32 idx[6] = { 0, 1, 3, 3, 2, 0};

		vk_image FontTexture;
		R_Handle FontTextHandle;
		// Image atlas creation for texture font
		//
		{
			FontTexture = CreateImageData(&VkBase, BitmapArray, (VkExtent3D) { 2100, 2100, 1 }, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
			VkSamplerCreateInfo sampler = { 0 };
			sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler.magFilter = VK_FILTER_LINEAR;
			sampler.minFilter = VK_FILTER_LINEAR;

			vkCreateSampler(VkBase.Device, &sampler, 0, &FontTexture.Sampler);

			FontTextHandle = R_PushTexture(&Render, "Fonts Atlas", &FontTexture);
		}

		vk_image ComputeImage;
		R_Handle ComputeImageHandle;
		// Image for the compute pipeline
		{
			ComputeImage = CreateImageDefault(
				&VkBase,
				(VkExtent3D){ VkBase.Swapchain.Extent.width, VkBase.Swapchain.Extent.height, 1 },
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				false
			);

			ComputeImageHandle = R_PushTexture(&Render, "Compute Texture", &ComputeImage);
		};

		ui_uniform UniformData;

		draw_bucket_instance DrawInstance = D_DrawInit(&TempAllocator);
		
		while (1) {
			ui_input Input = GetNextEvent(&VkBase.Window);

			f32 WindowWidth  = VkBase.Swapchain.Extent.width; 
			f32 WindowHeight = VkBase.Swapchain.Extent.height; 

			UniformData.ScreenWidth   = VkBase.Swapchain.Extent.width;
			UniformData.ScreenHeight  = VkBase.Swapchain.Extent.height;
			UniformData.TextureWidth  = 2100;
			UniformData.TextureHeight = 2100;

			D_BeginDraw2D(&DrawInstance);
			{
				vec4 background = { 0.8, 0.75, 0.4, 1.f};
				D_DrawRect2D(&DrawInstance, (rect_2d){ { 200, 200}, { 50, 50 } }, 8, 2, background);
				
				U8_String Hello = StringNew("Hello", 5, &Render.PerFrameAllocator);
				D_DrawText2D(&DrawInstance, (rect_2d){ { 100, 100 }, { 50, 50 } }, &Hello, &TitleFont2, background);
			}
			D_EndDraw2D(&DrawInstance);

			R_Begin(&Render);

			if (Render.SetExternalResize) {
				vmaDestroyImage(VkBase.GPUAllocator, ComputeImage.Image, ComputeImage.Alloc);
				ComputeImage = CreateImageDefault(
					&VkBase,
					(VkExtent3D){ VkBase.Swapchain.Extent.width, VkBase.Swapchain.Extent.height, 1 },
					VK_FORMAT_R32G32B32A32_SFLOAT,
					VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
					false
				);
			}

			R_SendDataToBuffer(&Render, StagBuffer[Render.VulkanBase->CurrentFrame], DrawInstance.Current2DBuffer.data, DrawInstance.Current2DBuffer.offset, 0);
			R_SendDataToBuffer(&Render, StagBuffer[Render.VulkanBase->CurrentFrame], idx, 6 * sizeof(u32), DrawInstance.Current2DBuffer.offset);
			
			VkBufferCopy Copy = {
				.srcOffset = 0,
				.dstOffset = 0,
				.size      = DrawInstance.Current2DBuffer.offset
			};
			R_CopyStageToBuffer(&Render, StagBuffer[Render.VulkanBase->CurrentFrame], VBuffer[Render.VulkanBase->CurrentFrame], Copy);

			Copy = (VkBufferCopy){
				.srcOffset = DrawInstance.Current2DBuffer.offset,
				.dstOffset = 0,
				.size      = 6 * sizeof(u32)
			};
			R_CopyStageToBuffer(&Render, StagBuffer[Render.VulkanBase->CurrentFrame], IBuffer[Render.VulkanBase->CurrentFrame], Copy);
			R_SendCopyToGpu(&Render);

			// Compute pipeline 
			//
			{
				TransitionImageDefault(
					Render.CurrentCommandBuffer,
					ComputeImage.Image,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_GENERAL
				);
				R_BindTexture(&Render, "Compute Texture", 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				R_DispatchCompute(&Render, ComputePipeline, ceilf(WindowWidth / 32), ceilf(WindowWidth / 32), 1);
			}

			//R_ClearScreen(&Render, (vec4){0.02, 0.02, 0.02, 1.0f});

			// Render pass
			//
			R_SendImageToSwapchain(&Render, ComputeImageHandle);
			
			R_BeginRenderPass(&Render);
			{
				R_BindTexture(&Render, "Fonts Atlas", 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
				R_UpdateUniformBuffer(&Render, "Uniform Buffer", 1, &UniformData, sizeof(ui_uniform));
				R_BindVertexBuffer(&Render, VBuffer[Render.VulkanBase->CurrentFrame]);
				R_BindIndexBuffer(&Render, IBuffer[Render.VulkanBase->CurrentFrame]);
				R_SetPipeline(&Render, Pipeline);
				R_DrawIndexed(&Render, 6, DrawInstance.Current2DBuffer.len);
			}
			R_RenderPassEnd(&Render);
			R_RenderEnd(&Render);

			if (Render.SetExternalResize) {
				vmaDestroyImage(VkBase.GPUAllocator, ComputeImage.Image, ComputeImage.Alloc);
				ComputeImage = CreateImageDefault(
					&VkBase,
					(VkExtent3D){ VkBase.Swapchain.Extent.width, VkBase.Swapchain.Extent.height, 1 },
					VK_FORMAT_R32G32B32A32_SFLOAT,
					VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
					false
				);
			}

			stack_free_all(&TempAllocator);
		}
	}

	return 0;
}

fn_internal r_vertex_input_description 
Vertex2DInputDescription(Stack_Allocator* Allocator) {
	r_vertex_input_description VertexDescription;
	VertexDescription.Stride = sizeof(v_2d);
	VertexDescription.Rate = VK_VERTEX_INPUT_RATE_INSTANCE;
	VertexDescription.AttributeCount = 7;
	VertexDescription.Attributes = stack_push(Allocator, r_vertex_attribute, 7);
	VertexDescription.Attributes[0] = (r_vertex_attribute){
		.Location = 0,
		.Format   = R_FORMAT_VEC2,
		.Offset    = 0
	};
	VertexDescription.Attributes[1] = (r_vertex_attribute){
		.Location = 1,
		.Format   = R_FORMAT_VEC2,
		.Offset    = sizeof(vec2)
	};
	VertexDescription.Attributes[2] = (r_vertex_attribute){
		.Location = 2,
		.Format   = R_FORMAT_VEC2,
		.Offset    = 2 * sizeof(vec2)
	};
	VertexDescription.Attributes[3] = (r_vertex_attribute){
		.Location = 3,
		.Format   = R_FORMAT_VEC2,
		.Offset    = 3 * sizeof(vec2)
	};
	VertexDescription.Attributes[4] = (r_vertex_attribute){
		.Location = 4,
		.Format   = R_FORMAT_VEC4,
		.Offset    = 4 * sizeof(vec2)
	};
	VertexDescription.Attributes[5] = (r_vertex_attribute){
		.Location = 5,
		.Format   = R_FORMAT_FLOAT,
		.Offset    = 4 * sizeof(vec2) + sizeof(vec4)
	};
	VertexDescription.Attributes[6] = (r_vertex_attribute){
		.Location = 6,
		.Format   = R_FORMAT_FLOAT,
		.Offset    = 4 * sizeof(vec2) + sizeof(vec4) + sizeof(float)
	};

	return VertexDescription;
=======
#if __linux__
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <unistd.h>
#define VK_USE_PLATFORM_XLIB_KHR

#elif _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <stddef.h>

#include "../third-party/xxhash.h"
#include "../third-party/stb_image.h"
#include "../third-party/stb_truetype.h"

#include "../types.h"
#include "../memory.h"
#include "../allocator.h"
#include "../strings.h"
#include "../vector.h"
#include "../queue.h"
#include "../files.h"
#include "../HashTable.h"

#include "../load_font.h"
#include "../window_creation.h"
#include "../third-party/vk_mem_alloc.h"
#include "../vk_render.h"
#include "../render.h"
#include "../draw.h"

#define MEMORY_IMPL
#include "../memory.h"

#define ALLOCATOR_IMPL
#include "../allocator.h"

#define STRINGS_IMPL
#include "../strings.h"

#define VECTOR_IMPL
#include "../vector.h"

#define QUEUE_IMPL
#include "../queue.h"

#define FILES_IMPL
#include "../files.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../third-party/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third-party/stb_truetype.h"

#define LOAD_FONT_IMPL
#include "../load_font.h"

#define WINDOW_CREATION_IMPL
#include "../window_creation.h"

#define VK_RENDER_IMPL
#include "../vk_render.h"

#include "../HashTable.h"
#include "../HashTable.c"

#include "../render.c"

#define DRAW_IMPL
#include "../draw.h"
typedef struct ui_uniform ui_uniform;
struct ui_uniform {
    f32 TextureWidth;
    f32 TextureHeight;
    f32 ScreenWidth;
    f32 ScreenHeight;
};

fn_internal r_vertex_input_description Vertex2DInputDescription( Stack_Allocator* Allocator );

int main()
{
	vulkan_base VkBase = VulkanInit();

	// Memory Allocation
    //
    Arena *main_arena = VkBase.Arena;
    Temp arena_temp = TempBegin(main_arena);
    
    Stack_Allocator Allocator;
    Stack_Allocator TempAllocator;
    u8 *Buffer = PushArray(main_arena, u8, gigabyte(1));
    u8 *TempBuffer = PushArray(main_arena, u8, gigabyte(1));
    stack_init(&Allocator, Buffer, gigabyte(1));
    stack_init(&TempAllocator, TempBuffer, gigabyte(1));

	// Font Creation
	//
	u8 *BitmapArray = stack_push(&Allocator, u8, 2100 * 1200);
    FontCache DefaultFont =
        F_BuildFont(22, 2100, 60, BitmapArray, "./data/RobotoMono.ttf");
    FontCache TitleFont = F_BuildFont(22, 2100, 60, BitmapArray + (2100 * 60),
                                      "./data/LiterationMono.ttf");
    FontCache TitleFont2 = F_BuildFont(22, 2100, 60, BitmapArray + (2100 * 120),
                                       "./data/TinosNerdFontPropo.ttf");
    FontCache BoldFont = F_BuildFont(18, 2100, 60, BitmapArray + (2100 * 180),
                                     "./data/LiterationMonoBold.ttf");
    FontCache ItalicFont = F_BuildFont(18, 2100, 60, BitmapArray + (2100 * 240),
                                       "./data/LiterationMonoItalic.ttf");
    DefaultFont.BitmapOffset = Vec2Zero();
    TitleFont.BitmapOffset = (vec2){0, 60};
    TitleFont2.BitmapOffset = (vec2){0, 120};
    BoldFont.BitmapOffset = (vec2){0, 180};
    ItalicFont.BitmapOffset = (vec2){0, 240};

	// Renderer creation
	//
	{
		r_render Render;
		R_RenderInit(&Render, &VkBase, &Allocator);
		VkDescriptorType Types[] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
		VkDescriptorSetLayout Layout = R_CreateDescriptorSetLayout(&Render, 2, Types, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		
		r_vertex_input_description VertexDescription = Vertex2DInputDescription(&Render.PerFrameAllocator);

		VkDescriptorSetLayout Layouts[] = { Layout };
		R_Handle Pipeline = R_CreatePipeline(
			&Render,
			"My Pipeline",
			"./data/ui_render.vert.spv",
			"./data/ui_render.frag.spv",
			&VertexDescription,
			Layouts,
			1
		);

		VkDescriptorType CompTypes[]     = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
		VkDescriptorSetLayout CompLayout = R_CreateDescriptorSetLayout(&Render, 1, CompTypes, VK_SHADER_STAGE_COMPUTE_BIT);
		R_Handle ComputePipeline = R_CreateComputePipeline(
			&Render,
			"Compute Pipe",
			"./data/compute.comp.spv",
			&CompLayout,
			1
		);
		
		R_Handle VBuffer[] = {
			R_CreateBuffer(&Render, "Buffer 1", 12 << 10, R_BUFFER_TYPE_VERTEX),
			R_CreateBuffer(&Render, "Buffer 2", 12 << 10, R_BUFFER_TYPE_VERTEX)
		};
		R_Handle StagBuffer[] =
		{ 
			R_CreateBuffer(&Render, "Stag Buffer 1", 12 << 10, R_BUFFER_TYPE_STAGING),
			R_CreateBuffer(&Render, "Stag Buffer 2", 12 << 10, R_BUFFER_TYPE_STAGING)
		};
		R_Handle IBuffer[] = {
			R_CreateBuffer(&Render, "Index Buffer 1", 126, R_BUFFER_TYPE_INDEX),
			R_CreateBuffer(&Render, "Index Buffer 2", 126, R_BUFFER_TYPE_INDEX)
		};
		R_Handle UBuffer = R_CreateBuffer(&Render, "Uniform Buffer", sizeof(ui_uniform), R_BUFFER_TYPE_UNIFORM);

		u32 idx[6] = { 0, 1, 3, 3, 2, 0};

		vk_image FontTexture;
		R_Handle FontTextHandle;
		// Image atlas creation for texture font
		//
		{
			FontTexture = CreateImageData(&VkBase, BitmapArray, (VkExtent3D) { 2100, 2100, 1 }, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
			VkSamplerCreateInfo sampler = { 0 };
			sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler.magFilter = VK_FILTER_LINEAR;
			sampler.minFilter = VK_FILTER_LINEAR;

			vkCreateSampler(VkBase.Device, &sampler, 0, &FontTexture.Sampler);

			FontTextHandle = R_PushTexture(&Render, "Fonts Atlas", &FontTexture);
		}

		vk_image ComputeImage;
		R_Handle ComputeImageHandle;
		// Image for the compute pipeline
		{
			ComputeImage = CreateImageDefault(
				&VkBase,
				(VkExtent3D){ VkBase.Swapchain.Extent.width, VkBase.Swapchain.Extent.height, 1 },
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				false
			);

			ComputeImageHandle = R_PushTexture(&Render, "Compute Texture", &ComputeImage);
		};

		ui_uniform UniformData;

		draw_bucket_instance DrawInstance = D_DrawInit(&TempAllocator);
		
		while (1) {
			ui_input Input = GetNextEvent(&VkBase.Window);

			f32 WindowWidth  = VkBase.Swapchain.Extent.width; 
			f32 WindowHeight = VkBase.Swapchain.Extent.height; 

			UniformData.ScreenWidth   = VkBase.Swapchain.Extent.width;
			UniformData.ScreenHeight  = VkBase.Swapchain.Extent.height;
			UniformData.TextureWidth  = 2100;
			UniformData.TextureHeight = 2100;

			D_BeginDraw2D(&DrawInstance);
			{
				vec4 background = { 0.8, 0.75, 0.4, 1.f};
				D_DrawRect2D(&DrawInstance, (rect_2d){ { 200, 200}, { 50, 50 } }, 8, 2, background);
				
				U8_String Hello = StringNew("Hello", 5, &Render.PerFrameAllocator);
				D_DrawText2D(&DrawInstance, (rect_2d){ { 100, 100 }, { 50, 50 } }, &Hello, &TitleFont2, background);
			}
			D_EndDraw2D(&DrawInstance);

			R_Begin(&Render);

			if (Render.SetExternalResize) {
				vmaDestroyImage(VkBase.GPUAllocator, ComputeImage.Image, ComputeImage.Alloc);
				ComputeImage = CreateImageDefault(
					&VkBase,
					(VkExtent3D){ VkBase.Swapchain.Extent.width, VkBase.Swapchain.Extent.height, 1 },
					VK_FORMAT_R32G32B32A32_SFLOAT,
					VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
					false
				);
			}

			R_SendDataToBuffer(&Render, StagBuffer[Render.VulkanBase->CurrentFrame], DrawInstance.Current2DBuffer.data, DrawInstance.Current2DBuffer.offset, 0);
			R_SendDataToBuffer(&Render, StagBuffer[Render.VulkanBase->CurrentFrame], idx, 6 * sizeof(u32), DrawInstance.Current2DBuffer.offset);
			
			VkBufferCopy Copy = {
				.srcOffset = 0,
				.dstOffset = 0,
				.size      = DrawInstance.Current2DBuffer.offset
			};
			R_CopyStageToBuffer(&Render, StagBuffer[Render.VulkanBase->CurrentFrame], VBuffer[Render.VulkanBase->CurrentFrame], Copy);

			Copy = (VkBufferCopy){
				.srcOffset = DrawInstance.Current2DBuffer.offset,
				.dstOffset = 0,
				.size      = 6 * sizeof(u32)
			};
			R_CopyStageToBuffer(&Render, StagBuffer[Render.VulkanBase->CurrentFrame], IBuffer[Render.VulkanBase->CurrentFrame], Copy);
			R_SendCopyToGpu(&Render);

			// Compute pipeline 
			//
			{
				TransitionImageDefault(
					Render.CurrentCommandBuffer,
					ComputeImage.Image,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_GENERAL
				);
				R_BindTexture(&Render, "Compute Texture", 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
				R_DispatchCompute(&Render, ComputePipeline, ceilf(WindowWidth / 32), ceilf(WindowWidth / 32), 1);
			}

			//R_ClearScreen(&Render, (vec4){0.02, 0.02, 0.02, 1.0f});

			// Render pass
			//
			R_SendImageToSwapchain(&Render, ComputeImageHandle);
			
			R_BeginRenderPass(&Render);
			{
				R_BindTexture(&Render, "Fonts Atlas", 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
				R_UpdateUniformBuffer(&Render, "Uniform Buffer", 1, &UniformData, sizeof(ui_uniform));
				R_BindVertexBuffer(&Render, VBuffer[Render.VulkanBase->CurrentFrame]);
				R_BindIndexBuffer(&Render, IBuffer[Render.VulkanBase->CurrentFrame]);
				R_SetPipeline(&Render, Pipeline);
				R_DrawIndexed(&Render, 6, DrawInstance.Current2DBuffer.len);
			}
			R_RenderPassEnd(&Render);
			R_RenderEnd(&Render);

			if (Render.SetExternalResize) {
				vmaDestroyImage(VkBase.GPUAllocator, ComputeImage.Image, ComputeImage.Alloc);
				ComputeImage = CreateImageDefault(
					&VkBase,
					(VkExtent3D){ VkBase.Swapchain.Extent.width, VkBase.Swapchain.Extent.height, 1 },
					VK_FORMAT_R32G32B32A32_SFLOAT,
					VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
					false
				);
			}

			stack_free_all(&TempAllocator);
		}
	}

	return 0;
}

fn_internal r_vertex_input_description 
Vertex2DInputDescription(Stack_Allocator* Allocator) {
	r_vertex_input_description VertexDescription;
	VertexDescription.Stride = sizeof(v_2d);
	VertexDescription.Rate = VK_VERTEX_INPUT_RATE_INSTANCE;
	VertexDescription.AttributeCount = 7;
	VertexDescription.Attributes = stack_push(Allocator, r_vertex_attribute, 7);
	VertexDescription.Attributes[0] = (r_vertex_attribute){
		.Location = 0,
		.Format   = R_FORMAT_VEC2,
		.Offset    = 0
	};
	VertexDescription.Attributes[1] = (r_vertex_attribute){
		.Location = 1,
		.Format   = R_FORMAT_VEC2,
		.Offset    = sizeof(vec2)
	};
	VertexDescription.Attributes[2] = (r_vertex_attribute){
		.Location = 2,
		.Format   = R_FORMAT_VEC2,
		.Offset    = 2 * sizeof(vec2)
	};
	VertexDescription.Attributes[3] = (r_vertex_attribute){
		.Location = 3,
		.Format   = R_FORMAT_VEC2,
		.Offset    = 3 * sizeof(vec2)
	};
	VertexDescription.Attributes[4] = (r_vertex_attribute){
		.Location = 4,
		.Format   = R_FORMAT_VEC4,
		.Offset    = 4 * sizeof(vec2)
	};
	VertexDescription.Attributes[5] = (r_vertex_attribute){
		.Location = 5,
		.Format   = R_FORMAT_FLOAT,
		.Offset    = 4 * sizeof(vec2) + sizeof(vec4)
	};
	VertexDescription.Attributes[6] = (r_vertex_attribute){
		.Location = 6,
		.Format   = R_FORMAT_FLOAT,
		.Offset    = 4 * sizeof(vec2) + sizeof(vec4) + sizeof(float)
	};

	return VertexDescription;
}