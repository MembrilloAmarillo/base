#include <chrono>
#ifdef SDL_USAGE
	#include <SDL3/SDL.h>
	#include <SDL3/SDL_vulkan.h>
#else
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

#include <chrono>

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
#include "../ui_render.h"
#include "../draw.h"
#include "../new_ui.h"
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

#define SP_UI_IMPL
#include "../new_ui.h"

#define UI_RENDER_IMPL
#include "../ui_render.h"

#define DRAW_IMPL
#include "../draw.h"

#include "../render.c"

typedef struct line_vert line_vert;
struct line_vert {
	rect_2d Rect;
	vec4    Color;
	float   BorderWidth;
};

typedef struct screen_ubo screen_ubo;
struct screen_ubo {
	float ScreenWidth;
	float ScreenHeight;
};

typedef struct todo_render todo_render;
struct todo_render {

	vulkan_base VkBase;
	r_render    Render;

	vk_image FontTexture;
	vk_image ComputeImage;

	R_Handle UI_Pipeline;
	R_Handle ComputePipeline;
	R_Handle LinePipeline;

	R_Handle FontTextHandle;
	R_Handle ComputeImageHandle;

	R_Handle LineBuffer[MAX_FRAMES_IN_FLIGHT];
	R_Handle LineIBuffer[MAX_FRAMES_IN_FLIGHT];
	R_Handle VBuffer[MAX_FRAMES_IN_FLIGHT];
	R_Handle StagBuffer[MAX_FRAMES_IN_FLIGHT];
	R_Handle IBuffer[MAX_FRAMES_IN_FLIGHT];
	R_Handle UBuffer;
	R_Handle LineUboBuffer;

	vector LineData;

    ui_uniform UniformData;
	screen_ubo ScreenUbo;

	draw_bucket_instance DrawInstance;

    Stack_Allocator Allocator;
    Stack_Allocator TempAllocator;

	Arena *main_arena;
	Temp   arena_temp;

	bool running;

	u32 idx[6];

	VkDescriptorSetLayout LineLayout;
	VkDescriptorSetLayout Layout;
	VkDescriptorSetLayout CompLayout;

	VkDescriptorType LineTypes[1];
	VkDescriptorType CompTypes[1];
	VkDescriptorType Types[2];

	ui_context* UI_Context;
};


struct todo_list {
	todo_render Render;
};

fn_internal r_vertex_input_description Vertex2DInputDescription(Stack_Allocator* Allocator);
fn_internal r_vertex_input_description Line2DInputDescription(Stack_Allocator* Allocator);

fn_internal r_vertex_input_description
Line2DInputDescription(Stack_Allocator* Allocator)
{
	r_vertex_input_description VertexDescription = {};
	VertexDescription.Stride = sizeof(line_vert);
	VertexDescription.Rate = VK_VERTEX_INPUT_RATE_INSTANCE;
	VertexDescription.AttributeCount = 3;
	VertexDescription.Attributes = stack_push(Allocator, r_vertex_attribute, 3);

	// Rect position (vec2)
	VertexDescription.Attributes[0].Location = 0;
	VertexDescription.Attributes[0].Format   = R_FORMAT_VEC2;
	VertexDescription.Attributes[0].Offset    = 0;

	// Rect size (vec2)
	VertexDescription.Attributes[1].Location = 1;
	VertexDescription.Attributes[1].Format   = R_FORMAT_VEC2;
	VertexDescription.Attributes[1].Offset    = sizeof(vec2);

	// Color (vec4)
	VertexDescription.Attributes[2].Location = 2;
	VertexDescription.Attributes[2].Format   = R_FORMAT_VEC4;
	VertexDescription.Attributes[2].Offset    = 2 * sizeof(vec2);

	return VertexDescription;
}

fn_internal void TodoRenderInit(todo_render* Render);

int main( void ) {
	todo_render TodoApp;
	TodoRenderInit(&TodoApp);

	rgba HardDark    = HexToRGBA(0x050505FF);
	rgba Dark        = HexToRGBA(0x121212FF);
	rgba VioletBord  = HexToRGBA(0x58536DFF);
	rgba EggYellow   = HexToRGBA(0xF0C38EFF);
	rgba LightPink   = HexToRGBA(0xF1AA9BFF);
	rgba BrokenWhite = HexToRGBA(0xEEEEEEFF);

	typedef struct object_stack {
		i64 N;
		i64 Current;
		ui_object** Items;
	}object_stack;

	object_stack ObjStack = {};
	ObjStack.N = 256 << 10;
	ObjStack.Current = 0;
	ObjStack.Items = stack_push(&TodoApp.Allocator, ui_object*, 256 << 10);

	bool FirstFrame = true;

	std::chrono::time_point LastFrame = std::chrono::high_resolution_clock::now();


	while (TodoApp.running) {
        std::chrono::time_point t1 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(t1 - LastFrame).count();
        LastFrame = t1;

		f32 window_width  = TodoApp.VkBase.Window.Width;
		f32 window_height = TodoApp.VkBase.Window.Height;

		TodoApp.UniformData.ScreenWidth   = window_width;
		TodoApp.UniformData.ScreenHeight  = window_height;
		TodoApp.UniformData.TextureWidth  = TodoApp.FontTexture.Width;
		TodoApp.UniformData.TextureHeight = TodoApp.FontTexture.Height;

		TodoApp.ScreenUbo.ScreenWidth   = window_width;
		TodoApp.ScreenUbo.ScreenHeight  = window_height;

		UI_Begin(TodoApp.UI_Context);

		ui_input Input = Input_None;

		if( FirstFrame ) {
            FirstFrame = false;
		} else {
    		Input = UI_LastEvent(TodoApp.UI_Context, &TodoApp.VkBase.Window);
		}

		if (Input & StopUI) {
			TodoApp.running = false;
		}
		if( Input & FrameBufferResized ) {
            TodoApp.VkBase.FramebufferResized = true;
		}

		// UI Creation
		{
			rect_2d PanelRect = {};
			PanelRect.Pos = (vec2){200, 200};
			PanelRect.Size = (vec2){300, 300};
			UI_SetNextParent(TodoApp.UI_Context, &TodoApp.UI_Context->RootObject);
			{
			UI_SetNextTheme(TodoApp.UI_Context, TodoApp.UI_Context->DefaultTheme.Window);
			ui_object* Windows = UI_BuildObject(TodoApp.UI_Context, (const uint8_t*)"Title", NULL, PanelRect, (ui_lay_opt)(UI_DrawRect | UI_DrawBorder | UI_Drag | UI_Resize | UI_Interact | UI_SetPosPersistent) );
			UI_UpdateObjectSize(TodoApp.UI_Context, Windows);
			UI_PopTheme(TodoApp.UI_Context);

			PanelRect = Windows->Rect;

			UI_PushNextLayout(TodoApp.UI_Context, PanelRect, (ui_lay_opt)0);
			UI_PushNextLayoutBoxSize(TodoApp.UI_Context, (vec2) { PanelRect.Size.x, 28 });
			UI_PushNextLayoutPadding(TodoApp.UI_Context, (vec2) { 10, 5 });
			UI_SetNextParent(TodoApp.UI_Context, Windows);

			UI_SetNextTheme(TodoApp.UI_Context, TodoApp.UI_Context->DefaultTheme.Button);
			ui_object* TitleObj = UI_BuildObject(TodoApp.UI_Context, (const uint8_t*)("Title"), (const uint8_t*)("Your ToDo App"), PanelRect, (ui_lay_opt)(UI_DrawText | UI_AlignCenter));
			UI_PopTheme(TodoApp.UI_Context);

			PanelRect.Pos.y += TitleObj->Size.y;
			UI_PushNextLayoutBoxSize(TodoApp.UI_Context, (vec2){PanelRect.Size.x, 4});
			UI_SetNextTheme(TodoApp.UI_Context, TodoApp.UI_Context->DefaultTheme.Panel);
			UI_BuildObject(TodoApp.UI_Context, (const uint8_t*)"TitleDivisor", NULL, PanelRect, (ui_lay_opt)UI_DrawRect);

			UI_PushNextLayoutBoxSize(TodoApp.UI_Context, (vec2) {PanelRect.Size.x, PanelRect.Size.y - 2*TitleObj->Size.y});
			UI_SetNextTheme(TodoApp.UI_Context, TodoApp.UI_Context->DefaultTheme.Label);
			ui_object* NewNoteObj = UI_BuildObject(TodoApp.UI_Context, (const uint8_t*)"+ Add New Note", (const uint8_t*)"+ Add New Note", PanelRect, (ui_lay_opt)(UI_DrawText | UI_AlignCenter | UI_AlignVertical | UI_Interact) );
				UI_PopTheme(TodoApp.UI_Context);

				if (UI_ConsumeEvents(TodoApp.UI_Context, NewNoteObj) & Input_CursorHover) {
					Windows->Theme.Background = RgbaToNorm(LightPink);
					Windows->Theme.BorderThickness = 0;
				}

			StackPop(&TodoApp.UI_Context->Layouts);
		}
		rect_2d _rect_init = {};
		_rect_init.Pos = (vec2){200, 200};
		_rect_init.Size = (vec2){300, 300};
		PanelRect = _rect_init;
		rect_2d _rect_init2 = {};
		_rect_init2.Pos = (vec2){550, 200};
		_rect_init2.Size = (vec2){300, 300};
		PanelRect = _rect_init2;
		UI_SetNextParent(TodoApp.UI_Context, &TodoApp.UI_Context->RootObject);
		{
			UI_SetNextTheme(TodoApp.UI_Context, TodoApp.UI_Context->DefaultTheme.Window);
			ui_object* Windows = UI_BuildObject(TodoApp.UI_Context, (const uint8_t*)"New Title", NULL, PanelRect, (ui_lay_opt)(UI_DrawRect | UI_DrawBorder | UI_Drag | UI_Resize | UI_Interact | UI_SetPosPersistent) );
			UI_UpdateObjectSize(TodoApp.UI_Context, Windows);
			UI_PopTheme(TodoApp.UI_Context);

			PanelRect = Windows->Rect;

			UI_PushNextLayout(TodoApp.UI_Context, PanelRect, (ui_lay_opt)0);
				UI_PushNextLayoutBoxSize(TodoApp.UI_Context, (vec2) { PanelRect.Size.x, 28 });
				UI_PushNextLayoutPadding(TodoApp.UI_Context, (vec2) { 10, 5 });
				UI_SetNextParent(TodoApp.UI_Context, Windows);

			UI_SetNextTheme(TodoApp.UI_Context, TodoApp.UI_Context->DefaultTheme.Button);
			ui_object* TitleObj = UI_BuildObject(TodoApp.UI_Context, (const uint8_t*)"Title", (const uint8_t*)"Example ToDo", PanelRect, (ui_lay_opt)(UI_DrawText | UI_AlignCenter));
			UI_PopTheme(TodoApp.UI_Context);

			PanelRect.Pos.y += TitleObj->Size.y;
			UI_PushNextLayoutBoxSize(TodoApp.UI_Context, (vec2){PanelRect.Size.x, 4});
			UI_SetNextTheme(TodoApp.UI_Context, TodoApp.UI_Context->DefaultTheme.Panel);
			UI_BuildObject(TodoApp.UI_Context, (const uint8_t*)"TitleDivisor", NULL, PanelRect, (ui_lay_opt)UI_DrawRect);

			UI_PushNextLayoutBoxSize(TodoApp.UI_Context, (vec2) {PanelRect.Size.x, TitleObj->Size.y});
			UI_BeginScrollbarView(TodoApp.UI_Context);
			for (i32 i = 0; i < 500; i += 1) {
				char buf[64] = { 0 };
				snprintf(buf, 64, "Tarea Num. %d", i);
				UI_Label(TodoApp.UI_Context, buf);
			}
			UI_EndScrollbarView(TodoApp.UI_Context);

			StackPop(&TodoApp.UI_Context->Layouts);
			}
		}

		// Drawing the UI
		D_BeginDraw2D(&TodoApp.DrawInstance);
		{
			ui_object* Root = &TodoApp.UI_Context->RootObject;

			ObjStack.Current = 0;

			ui_object* Object;
			StackPush(&ObjStack, Root);

			for (; ObjStack.Current > 0;) {

				Object = StackGetFront(&ObjStack);
				StackPop(&ObjStack);

				if (Object->Parent->Type == UI_ScrollbarType && Object->Type != UI_ScrollbarTypeButton) {
					if( Object->Rect.Pos.y < Object->Parent->Rect.Pos.y ) {
						continue;
					}
					if( Object->Rect.Pos.y > Object->Parent->Rect.Pos.y + Object->Parent->Rect.Size.y ) {
						continue;
					} else if( Object->Rect.Pos.y + Object->Rect.Size.y > Object->Parent->Rect.Pos.y + Object->Parent->Rect.Size.y ) {
						continue;
					}
				}

				if (Object->Option & UI_DrawRect) {
					D_DrawRect2D(&TodoApp.DrawInstance, Object->Rect, Object->Theme.Radius, 0, Object->Theme.Background);
				}
				if (Object->Option & UI_DrawBorder) {

					D_DrawRect2D(&TodoApp.DrawInstance, Object->Rect, Object->Theme.Radius, Object->Theme.BorderThickness, Object->Theme.Border);
				}
    			if (Object->Option & UI_DrawText) {
    				rect_2d TextRect = {};
    				TextRect.Pos = Object->Pos;
    				TextRect.Size = Object->Size;
    				D_DrawText2D(&TodoApp.DrawInstance, TextRect, &Object->Text, Object->Theme.Font, Object->Theme.Foreground);
    			}
    			for ( Object = Object->FirstSon; Object != &UI_NULL_OBJECT; Object = Object->Right) {
					// process object
					StackPush(&ObjStack, Object);
				}
			}

			{
			    char buf[64];
				snprintf(buf, 64, "Time in ms: %.8lf", duration);
			    U8_String Text = StringNew(buf, 64, &TodoApp.TempAllocator);
    			D_DrawText2D(&TodoApp.DrawInstance, NewRect2D(20, 20, 200, 40), &Text, TodoApp.UI_Context->DefaultTheme.Window.Font, Vec4New(0.8, 0.6, 0.55, 1.0f));
			}
		}

		D_EndDraw2D(&TodoApp.DrawInstance);

		// --------------- UI_End --------------------

		line_vert LineVert   = {};
		LineVert.Rect.Pos    = (vec2){195, 500};
		LineVert.Rect.Size   = (vec2){0, 200};
		LineVert.Color       = RgbaToNorm(VioletBord);
		LineVert.BorderWidth = 4.f;

		R_Begin(&TodoApp.Render);

		if (TodoApp.Render.SetExternalResize) {
			vmaDestroyImage(TodoApp.VkBase.GPUAllocator, TodoApp.ComputeImage.Image, TodoApp.ComputeImage.Alloc);
			TodoApp.ComputeImage = CreateImageDefault(
				&TodoApp.VkBase,
				(VkExtent3D){ TodoApp.VkBase.Swapchain.Extent.width, TodoApp.VkBase.Swapchain.Extent.height, 1 },
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				false
			);
		}

		R_SendDataToBuffer(
			&TodoApp.Render,
			TodoApp.StagBuffer[TodoApp.Render.VulkanBase->CurrentFrame],
			TodoApp.DrawInstance.Current2DBuffer.data,
			TodoApp.DrawInstance.Current2DBuffer.offset, 0
		);
		R_SendDataToBuffer(
			&TodoApp.Render,
			TodoApp.StagBuffer[TodoApp.Render.VulkanBase->CurrentFrame],
			TodoApp.idx, 6 * sizeof(u32),
			TodoApp.DrawInstance.Current2DBuffer.offset
		);

		R_SendDataToBuffer(
			&TodoApp.Render,
			TodoApp.StagBuffer[TodoApp.Render.VulkanBase->CurrentFrame],
			&LineVert,
			sizeof(line_vert), TodoApp.DrawInstance.Current2DBuffer.offset + (sizeof(u32) * 6)
		);
		R_SendDataToBuffer(
			&TodoApp.Render,
			TodoApp.StagBuffer[TodoApp.Render.VulkanBase->CurrentFrame],
			TodoApp.idx, 6 * sizeof(u32),
			sizeof(line_vert) + TodoApp.DrawInstance.Current2DBuffer.offset + (sizeof(u32) * 6)
		);

		VkBufferCopy Copy = {};
		Copy.srcOffset = 0;
		Copy.dstOffset = 0;
		Copy.size = TodoApp.DrawInstance.Current2DBuffer.offset;
		R_CopyStageToBuffer(
			&TodoApp.Render,
			TodoApp.StagBuffer[TodoApp.Render.VulkanBase->CurrentFrame],
			TodoApp.VBuffer[TodoApp.Render.VulkanBase->CurrentFrame],
			Copy
		);

		Copy = {};
		Copy.srcOffset = TodoApp.DrawInstance.Current2DBuffer.offset;
		Copy.dstOffset = 0;
		Copy.size = 6 * sizeof(u32);
		R_CopyStageToBuffer(&TodoApp.Render, TodoApp.StagBuffer[TodoApp.Render.VulkanBase->CurrentFrame], TodoApp.IBuffer[TodoApp.Render.VulkanBase->CurrentFrame], Copy);


		Copy = {};
		Copy.srcOffset = TodoApp.DrawInstance.Current2DBuffer.offset + 6 * sizeof(u32);
		Copy.dstOffset = 0;
		Copy.size = sizeof(line_vert);
		R_CopyStageToBuffer(&TodoApp.Render, TodoApp.StagBuffer[TodoApp.Render.VulkanBase->CurrentFrame], TodoApp.LineBuffer[TodoApp.Render.VulkanBase->CurrentFrame], Copy);

		Copy = {};
		Copy.srcOffset = sizeof(line_vert) + TodoApp.DrawInstance.Current2DBuffer.offset + 6 * sizeof(u32);
		Copy.dstOffset = 0;
		Copy.size = 6 * sizeof(u32);
		R_CopyStageToBuffer(&TodoApp.Render, TodoApp.StagBuffer[TodoApp.Render.VulkanBase->CurrentFrame], TodoApp.LineIBuffer[TodoApp.Render.VulkanBase->CurrentFrame], Copy);

		R_SendCopyToGpu(&TodoApp.Render);

		// Compute pipeline
		//
		if( false ){
			TransitionImageDefault(
				TodoApp.Render.CurrentCommandBuffer,
				TodoApp.ComputeImage.Image,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL
			);
			R_BindTexture(&TodoApp.Render, "Compute Texture", 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			R_DispatchCompute(&TodoApp.Render, TodoApp.ComputePipeline, ceilf(window_width / 32), ceilf(window_height / 32), 1);
			R_SendImageToSwapchain(&TodoApp.Render, TodoApp.ComputeImageHandle);
		}

		R_ClearScreen(&TodoApp.Render, (vec4){0.02, 0.02, 0.02, 1.0f});

		// Render pass
		//
		TransitionImageDefault(
			TodoApp.Render.CurrentCommandBuffer,
			TodoApp.VkBase.Swapchain.Images[TodoApp.VkBase.SwapchainImageIdx],
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		);

		R_BeginRenderPass(&TodoApp.Render);
		{
			R_BindTexture(&TodoApp.Render, "Fonts Atlas", 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			R_UpdateUniformBuffer(&TodoApp.Render, "Uniform Buffer", 1, &TodoApp.UniformData, sizeof(ui_uniform));
			//R_BindTexture(&TodoApp.Render, "Icons Atlas", 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			R_BindVertexBuffer(&TodoApp.Render, TodoApp.VBuffer[TodoApp.Render.VulkanBase->CurrentFrame]);
			R_BindIndexBuffer(&TodoApp.Render, TodoApp.IBuffer[TodoApp.Render.VulkanBase->CurrentFrame]);
			R_SetPipeline(&TodoApp.Render, TodoApp.UI_Pipeline);
			R_DrawIndexed(&TodoApp.Render, 6, TodoApp.DrawInstance.Current2DBuffer.len);
		}
		R_RenderPassEnd(&TodoApp.Render);
		R_RenderEnd(&TodoApp.Render);

		if (TodoApp.Render.SetExternalResize) {
			vmaDestroyImage(TodoApp.VkBase.GPUAllocator, TodoApp.ComputeImage.Image, TodoApp.ComputeImage.Alloc);
			TodoApp.ComputeImage = CreateImageDefault(
				&TodoApp.VkBase,
				(VkExtent3D){ TodoApp.VkBase.Swapchain.Extent.width, TodoApp.VkBase.Swapchain.Extent.height, 1 },
				VK_FORMAT_R32G32B32A32_SFLOAT,
				VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				false
			);
		}

        stack_free_all(&TodoApp.VkBase.TempAllocator);
		stack_free_all(&TodoApp.TempAllocator);


	}
}

fn_internal void TodoRenderInit(todo_render* TodoRenderer) {
	TodoRenderer->VkBase = VulkanInit();
	TodoRenderer->main_arena = TodoRenderer->VkBase.Arena;
	TodoRenderer->arena_temp = TempBegin(TodoRenderer->main_arena);
	u8 *Buffer = PushArray(TodoRenderer->main_arena, u8, gigabyte(1));
	u8 *TempBuffer = PushArray(TodoRenderer->main_arena, u8, gigabyte(1));
	stack_init(&TodoRenderer->Allocator, Buffer, gigabyte(1));
	stack_init(&TodoRenderer->TempAllocator, TempBuffer, gigabyte(1));

	FontCache *DefaultFont = stack_push(&TodoRenderer->Allocator, FontCache, 1);
	FontCache *TitleFont   = stack_push(&TodoRenderer->Allocator, FontCache, 1);
	FontCache *TitleFont2  = stack_push(&TodoRenderer->Allocator, FontCache, 1);
	FontCache *BoldFont    = stack_push(&TodoRenderer->Allocator, FontCache, 1);
	FontCache *ItalicFont = stack_push(&TodoRenderer->Allocator, FontCache, 1);

	u8 *BitmapArray = stack_push(&TodoRenderer->Allocator, u8, 2100 * 1200);

	*DefaultFont = F_BuildFont(22, 2100, 60, BitmapArray, "./data/RobotoMono.ttf");
    *TitleFont   = F_BuildFont(22, 2100, 60, BitmapArray + (2100 * 60), "./data/LiterationMono.ttf");
    *TitleFont2  = F_BuildFont(22, 2100, 60, BitmapArray + (2100 * 120), "./data/TinosNerdFontPropo.ttf");
    *BoldFont    = F_BuildFont(18, 2100, 60, BitmapArray + (2100 * 180), "./data/LiterationMonoBold.ttf");
    *ItalicFont  = F_BuildFont(18, 2100, 60, BitmapArray + (2100 * 240), "./data/LiterationMonoItalic.ttf");

    DefaultFont->BitmapOffset = Vec2Zero();
    TitleFont->BitmapOffset   = (vec2){0, 60};
    TitleFont2->BitmapOffset  = (vec2){0, 120};
    BoldFont->BitmapOffset    = (vec2){0, 180};
    ItalicFont->BitmapOffset  = (vec2){0, 240};

	R_RenderInit(&TodoRenderer->Render, &TodoRenderer->VkBase, &TodoRenderer->Allocator);
	TodoRenderer->DrawInstance = D_DrawInit(&TodoRenderer->Allocator);

	TodoRenderer->VBuffer[0]     = R_CreateBuffer(&TodoRenderer->Render, "Buffer 1", 12 << 20, R_BUFFER_TYPE_VERTEX);
	TodoRenderer->VBuffer[1]     = R_CreateBuffer(&TodoRenderer->Render, "Buffer 2", 12 << 20, R_BUFFER_TYPE_VERTEX);
	TodoRenderer->LineBuffer[0]  = R_CreateBuffer(&TodoRenderer->Render, "Line Buffer 1", 12 << 20, R_BUFFER_TYPE_VERTEX);
	TodoRenderer->LineBuffer[1]  = R_CreateBuffer(&TodoRenderer->Render, "Line Buffer 2", 12 << 20, R_BUFFER_TYPE_VERTEX);
	TodoRenderer->LineIBuffer[0] = R_CreateBuffer(&TodoRenderer->Render, "Index Buffer 1", 126, R_BUFFER_TYPE_INDEX);
	TodoRenderer->LineIBuffer[1] = R_CreateBuffer(&TodoRenderer->Render, "Index Buffer 2", 126, R_BUFFER_TYPE_INDEX);
	TodoRenderer->StagBuffer[0]  = R_CreateBuffer(&TodoRenderer->Render, "Stag Buffer 1", 13 << 20, R_BUFFER_TYPE_STAGING);
	TodoRenderer->StagBuffer[1]  = R_CreateBuffer(&TodoRenderer->Render, "Stag Buffer 2", 13 << 20, R_BUFFER_TYPE_STAGING);
	TodoRenderer->IBuffer[0]     = R_CreateBuffer(&TodoRenderer->Render, "Index Buffer 1", 126, R_BUFFER_TYPE_INDEX);
	TodoRenderer->IBuffer[1]     = R_CreateBuffer(&TodoRenderer->Render, "Index Buffer 2", 126, R_BUFFER_TYPE_INDEX);

	TodoRenderer->UBuffer = R_CreateBuffer(
		&TodoRenderer->Render,
		"Uniform Buffer",
		sizeof(ui_uniform),
		R_BUFFER_TYPE_UNIFORM
	);

	TodoRenderer->LineUboBuffer = R_CreateBuffer(
		&TodoRenderer->Render,
		"Line Uniform Buffer",
		sizeof(screen_ubo),
		R_BUFFER_TYPE_UNIFORM
	);

	TodoRenderer->idx[0] = 0;
	TodoRenderer->idx[1] = 1;
	TodoRenderer->idx[2] = 3;
	TodoRenderer->idx[3] = 3;
	TodoRenderer->idx[4] = 2;
	TodoRenderer->idx[5] = 0;

	TodoRenderer->Types[0] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	TodoRenderer->Types[1] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	TodoRenderer->CompTypes[0] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	TodoRenderer->LineTypes[0] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	TodoRenderer->running = true;

	TodoRenderer->Layout     = R_CreateDescriptorSetLayout(&TodoRenderer->Render, 2, TodoRenderer->Types, (VkShaderStageFlagBits)(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT));
	TodoRenderer->LineLayout = R_CreateDescriptorSetLayout(&TodoRenderer->Render, 1, TodoRenderer->LineTypes, VK_SHADER_STAGE_VERTEX_BIT);
	TodoRenderer->CompLayout = R_CreateDescriptorSetLayout(&TodoRenderer->Render, 1, TodoRenderer->CompTypes, VK_SHADER_STAGE_COMPUTE_BIT);
	r_vertex_input_description VertexDescription = Vertex2DInputDescription(&TodoRenderer->Render.PerFrameAllocator);
	r_vertex_input_description LineVertDescription = Line2DInputDescription(&TodoRenderer->Render.PerFrameAllocator);


	TodoRenderer->FontTexture = CreateImageData(&TodoRenderer->VkBase, BitmapArray, (VkExtent3D) { 2100, 2100, 1 }, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
	VkSamplerCreateInfo sampler = {  };
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;

	vkCreateSampler(TodoRenderer->VkBase.Device, &sampler, 0, &TodoRenderer->FontTexture.Sampler);

	TodoRenderer->FontTextHandle = R_PushTexture(&TodoRenderer->Render, "Fonts Atlas", &TodoRenderer->FontTexture);

	// Image for the compute pipeline
	TodoRenderer->ComputeImage = CreateImageDefault(
		&TodoRenderer->VkBase,
		(VkExtent3D){ TodoRenderer->VkBase.Swapchain.Extent.width, TodoRenderer->VkBase.Swapchain.Extent.height, 1 },
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		false
	);

	TodoRenderer->ComputeImageHandle = R_PushTexture(&TodoRenderer->Render, "Compute Texture", &TodoRenderer->ComputeImage);

	TodoRenderer->UI_Pipeline = R_CreatePipeline(
		&TodoRenderer->Render,
		"My Pipeline",
		"./data/ui_render.vert.spv",
		"./data/ui_render.frag.spv",
		&VertexDescription,
		&TodoRenderer->Layout,
		1
	);

	TodoRenderer->ComputePipeline = R_CreateComputePipeline(
		&TodoRenderer->Render,
		"Compute Pipe",
		"./data/compute.comp.spv",
		&TodoRenderer->CompLayout,
		1
	);

	TodoRenderer->LineData = VectorNew( stack_push(&TodoRenderer->Allocator, line_vert, 1 << 20), 0, 1 << 20, line_vert );

	TodoRenderer->UI_Context = stack_push(&TodoRenderer->Allocator, ui_context, 1);

	rgba HardDark    = HexToRGBA(0x050505FF);
    rgba Dark        = HexToRGBA(0x121212FF);
	rgba VioletBord  = HexToRGBA(0x58536DFF);
    rgba EggYellow   = HexToRGBA(0xF0C38EFF);
    rgba LightPink   = HexToRGBA(0xF1AA9BFF);
	rgba BrokenWhite = HexToRGBA(0xEEEEEEFF);

	UI_Init(TodoRenderer->UI_Context, &TodoRenderer->Allocator, &TodoRenderer->TempAllocator);
	ui_theme DefaultTheme = {};
	DefaultTheme.Window.Border = RgbaToNorm(LightPink);
	DefaultTheme.Window.Background = RgbaToNorm(BrokenWhite);
	DefaultTheme.Window.Foreground = RgbaToNorm(HardDark);
	DefaultTheme.Window.Radius = 8;
	DefaultTheme.Window.BorderThickness = 4;
	DefaultTheme.Window.Font = TitleFont2;

	DefaultTheme.Button.Border = RgbaToNorm(LightPink);
	DefaultTheme.Button.Background = RgbaToNorm(BrokenWhite);
	DefaultTheme.Button.Foreground = RgbaToNorm(HardDark);
	DefaultTheme.Button.Radius = 2;
	DefaultTheme.Button.BorderThickness = 0;
	DefaultTheme.Button.Font = DefaultFont;

	DefaultTheme.Panel.Border = RgbaToNorm(BrokenWhite);
	DefaultTheme.Panel.Background = RgbaToNorm(VioletBord);
	DefaultTheme.Panel.Foreground = RgbaToNorm(HardDark);
	DefaultTheme.Panel.Radius = 8;
	DefaultTheme.Panel.BorderThickness = 1;
	DefaultTheme.Panel.Font = DefaultFont;

	DefaultTheme.Input.Border = RgbaToNorm(Dark);
	DefaultTheme.Input.Background = RgbaToNorm(HardDark);
	DefaultTheme.Input.Foreground = RgbaToNorm(BrokenWhite);
	DefaultTheme.Input.Radius = 2;
	DefaultTheme.Input.BorderThickness = 1;
	DefaultTheme.Input.Font = DefaultFont;

	DefaultTheme.Label.Border = RgbaToNorm(LightPink);
	DefaultTheme.Label.Background = RgbaToNorm(BrokenWhite);
	DefaultTheme.Label.Foreground = RgbaToNorm(Dark);
	DefaultTheme.Label.Radius = 2;
	DefaultTheme.Label.BorderThickness = 1;
	DefaultTheme.Label.Font = ItalicFont;

	DefaultTheme.Scrollbar.Border = RgbaToNorm(LightPink);
	DefaultTheme.Scrollbar.Background = RgbaToNorm(Dark);
	DefaultTheme.Scrollbar.Foreground = RgbaToNorm(Dark);
	DefaultTheme.Scrollbar.Radius = 8;
	DefaultTheme.Scrollbar.BorderThickness = 1;
	DefaultTheme.Scrollbar.Font = NULL;

	TodoRenderer->UI_Context->DefaultTheme = DefaultTheme;
}
