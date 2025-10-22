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

typedef struct ui_uniform ui_uniform;
struct ui_uniform {
    f32 TextureWidth;
    f32 TextureHeight;
    f32 ScreenWidth;
    f32 ScreenHeight;
};

int main()
{
	vulkan_base VkBase = VulkanInit();
    
    Arena *main_arena = VkBase.Arena;
    Temp arena_temp = TempBegin(main_arena);
    
    Stack_Allocator Allocator;
    Stack_Allocator TempAllocator;
    u8 *Buffer = PushArray(main_arena, u8, gigabyte(1));
    u8 *TempBuffer = PushArray(main_arena, u8, mebibyte(256));
    stack_init(&Allocator, Buffer, gigabyte(1));
    stack_init(&TempAllocator, TempBuffer, mebibyte(256));

	{
		r_render Render;
		R_RenderInit(&Render, &VkBase, &Allocator);
		VkDescriptorType Types[] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
		VkDescriptorSetLayout Layout = R_CreateDescriptorSetLayout(&Render, 1, Types, VK_SHADER_STAGE_VERTEX_BIT);

		r_vertex_input_description VertexDescription;
		VertexDescription.Stride = sizeof(v_2d);
		VertexDescription.Rate = VK_VERTEX_INPUT_RATE_INSTANCE;
		VertexDescription.AttributeCount = 7;
		VertexDescription.Attributes = stack_push(&TempAllocator, r_vertex_attribute, 7);
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

		VkDescriptorSetLayout Layouts[] = { Layout };
		R_Handle Pipeline = R_CreatePipeline(
			&Render, 
			"My Pipeline", 
			"./data/Sample.vert.spv", 
			"./data/Sample.frag.spv",
			&VertexDescription,
			Layouts,
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
		
		v_2d v1 = {
			.LeftCorner   = { 50, 50},
			.Size         = { 200, 200 }, 
			.UV           = { -2, -2 },
			.UVSize       = { 0, 0 },
			.Color        = {(f32)1, (f32)1, (f32)1, (f32)1},
			.CornerRadius = 0.0f,
			.Border       = 0
		};

		u32 idx[6] = { 0, 1, 3, 3, 2, 0};

		ui_uniform UniformData;
		
		while (1) {
			ui_input Input = GetNextEvent(&VkBase.Window);

			UniformData.ScreenWidth   = VkBase.Swapchain.Extent.width;
			UniformData.ScreenHeight  = VkBase.Swapchain.Extent.height;
			UniformData.TextureWidth  = 1000;
			UniformData.TextureHeight = 1000;

			R_Begin(&Render);

			R_SendDataToBuffer(&Render, StagBuffer[Render.VulkanBase->CurrentFrame], &v1, sizeof(v_2d), 0);
			R_SendDataToBuffer(&Render, StagBuffer[Render.VulkanBase->CurrentFrame], idx, 6 * sizeof(u32), sizeof(v_2d));
			
			VkBufferCopy Copy = {
				.srcOffset = 0,
				.dstOffset = 0,
				.size      = sizeof(v_2d)
			};
			R_CopyStageToBuffer(&Render, StagBuffer[Render.VulkanBase->CurrentFrame], VBuffer[Render.VulkanBase->CurrentFrame], Copy);

			Copy = (VkBufferCopy){
				.srcOffset = sizeof(v_2d),
				.dstOffset = 0,
				.size      = 6 * sizeof(u32)
			};
			R_CopyStageToBuffer(&Render, StagBuffer[Render.VulkanBase->CurrentFrame], IBuffer[Render.VulkanBase->CurrentFrame], Copy);
			// Add pipeline barrier before vertex input
			VkMemoryBarrier barrier = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
			};

			vkCmdPipelineBarrier(Render.CurrentCommandBuffer,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
								 0, 1, &barrier, 0, 0, 0, 0);
			R_BeginRenderPass(&Render);

			//R_BindTexture(&Render, "My Texture", 0);
			R_UpdateUniformBuffer(&Render, "Uniform Buffer", 0, &UniformData, sizeof(ui_uniform));
			R_BindVertexBuffer(&Render, VBuffer[Render.VulkanBase->CurrentFrame]);
			R_BindIndexBuffer(&Render, IBuffer[Render.VulkanBase->CurrentFrame]);
			R_SetPipeline(&Render, Pipeline);

			R_DrawIndexed(&Render, 6, 1);

			R_RenderEnd(&Render);
		}
	}
}