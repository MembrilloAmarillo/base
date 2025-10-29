/**
 * Sample to demonstrate how to use the UI and render it, for now only X11 supported.
 *
 * @author Sascha Paez
 * @version 1.2
 * @since 2025
 *
 */

#include "UI_Sample.h"

internal r_vertex_input_description Vertex2DInputDescription(Stack_Allocator* Allocator);

/**
 * @brief Main function
 * @param[in] argc Number of arguments passed by the command line
 * @param[in] argv The actual arguments passed by the command line
 * @return int status
 */
int
main( int argc, char *argv[] )
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
	
    // @todo: Change how to add fonts this is highly inconvenient
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

	ui_context *UI_Context = stack_push(&Allocator, ui_context, 1);

	Stack_Allocator UI_Allocator;
	Stack_Allocator UI_TempAllocator;
	{
		u8 *Buffer = PushArray(main_arena, u8, gigabyte(1));
		u8 *TempBuffer = PushArray(main_arena, u8, gigabyte(1));
		stack_init(&UI_Allocator, Buffer, gigabyte(1));
		stack_init(&UI_TempAllocator, TempBuffer, gigabyte(1));

		UI_Init(UI_Context, &UI_Allocator, &UI_TempAllocator);
	}
    rgba bd  = HexToRGBA(0x2D2D2DFF);
    rgba ib  = HexToRGBA(0x2F2F2FFF);
    rgba fd  = HexToRGBA(0x3D3D3DFF);
    rgba fg  = RgbaNew(21, 39, 64, 255);
    rgba bg2 = HexToRGBA(0xA13535FF);
    rgba bc  = HexToRGBA(0x3B764CFF);

    object_theme DefaultTheme = {
		.Border = RgbaToNorm(bc),
		.Background = RgbaToNorm(bd),
		.Foreground = RgbaToNorm(fd),
		.Radius = 6,
		.BorderThickness = 2,
		.Font = &DefaultFont};

		object_theme TitleTheme = DefaultTheme;
		TitleTheme.Font = &TitleFont;

		object_theme ButtonTheme = {.Border = RgbaToNorm(bc),
		.Background = RgbaToNorm(bd),
		.Foreground = RgbaToNorm(fd),
		.Radius = 6,
		.BorderThickness = 0,
		.Font = &BoldFont};

		object_theme PanelTheme = {.Border = RgbaToNorm(bc),
		.Background = RgbaToNorm(bg2),
		.Foreground = RgbaToNorm(fd),
		.Radius = 6,
		.BorderThickness = 0,
		.Font = &BoldFont};

		object_theme InputTheme = {.Border = RgbaToNorm(bc),
		.Background = RgbaToNorm(ib),
		.Foreground = RgbaToNorm(fd),
		.Radius = 2,
		.BorderThickness = 2,
		.Font = &DefaultFont};

		object_theme LabelTheme = {.Border = RgbaToNorm(bc),
		.Background = RgbaToNorm(bd),
		.Foreground = RgbaToNorm(fd),
		.Radius = 0,
		.BorderThickness = 0,
		.Font = &ItalicFont};

		object_theme ScrollbarTheme = {.Border = RgbaToNorm(bc),
		.Background = RgbaToNorm(fd),
		.Foreground = RgbaToNorm(bc),
		.Radius = 6,
		.BorderThickness = 2,
		.Font = NULL
	};

    UI_Context->DefaultTheme = (ui_theme){
        .Window = TitleTheme,
        .Button = ButtonTheme,
        .Panel = PanelTheme,
        .Input = InputTheme,
        .Label = LabelTheme,
        .Scrollbar = ScrollbarTheme
    };

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
		R_CreateBuffer(&Render, "Buffer 1", 12 << 20, R_BUFFER_TYPE_VERTEX),
		R_CreateBuffer(&Render, "Buffer 2", 12 << 20, R_BUFFER_TYPE_VERTEX)
	};
	R_Handle StagBuffer[] =
	{ 
		R_CreateBuffer(&Render, "Stag Buffer 1", 13 << 20, R_BUFFER_TYPE_STAGING),
		R_CreateBuffer(&Render, "Stag Buffer 2", 13 << 20, R_BUFFER_TYPE_STAGING)
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
	}

	ui_uniform UniformData;

	draw_bucket_instance DrawInstance = D_DrawInit(&TempAllocator);

    bool running = true;
    struct timespec ts2, ts1;

    double posix_dur = 0;

    while( running ) {
        //clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts1);
        u32 window_width  = VkBase.Swapchain.Extent.width;
        u32 window_height = VkBase.Swapchain.Extent.height;

		UniformData.ScreenWidth   = window_width;
		UniformData.ScreenHeight  = window_height;
		UniformData.TextureWidth  = FontTexture.Width;
		UniformData.TextureHeight = FontTexture.Height;

        UI_Begin(UI_Context);

        ui_input Input = UI_LastEvent(UI_Context, &VkBase.Window);

        if( Input == StopUI ) {
            fprintf(stderr, "[INFO] Finished Application\n");
            running = false;
            break;
        }

        UI_WindowBegin(
            UI_Context, 
            (rect_2d){{5, 5}, {window_width / 2 - 10, window_height / 2 - 10}}, 
            "Hello Title",
            UI_AlignCenter | UI_SetPosPersistent | UI_Drag | UI_Select | UI_Resize
        ); 
        {
            if( UI_BeginTreeNode(UI_Context, "Tree") & ActiveObject ) {
                UI_Label(UI_Context, "This is a label");
                if( UI_Button(UI_Context, "This is a button") & Input_LeftClickPress ) {
					printf("Button clicked");
                }
                UI_TextBox(UI_Context, "This is a textbox");
                UI_Label(UI_Context, "Here below we set a scrollbar view");
                UI_BeginScrollbarView(UI_Context);
                {
                    for( i32 i = 0; i < 100; i += 1 ) {
                        char buf[64] = {0};
                        snprintf(buf, 64, "Label number %d", i);
                        UI_Label(UI_Context, buf);
                    }
                }
                UI_EndScrollbarView(UI_Context);
            }
            UI_EndTreeNode(UI_Context);
        }
        UI_WindowEnd(UI_Context);

		UI_WindowBegin(
            UI_Context, 
            (rect_2d){{5, 5}, {window_width / 2 - 10, window_height / 2 - 10}}, 
            "Hello Title 2",
            UI_AlignCenter | UI_SetPosPersistent | UI_Drag | UI_Select | UI_Resize
        ); 
        {
            if( UI_BeginTreeNode(UI_Context, "Tree") & ActiveObject ) {
                UI_Label(UI_Context, "This is a label");
                if( UI_Button(UI_Context, "This is a button") & Input_LeftClickPress ) {

                }
                UI_TextBox(UI_Context, "This is a textbox");
                UI_Label(UI_Context, "Here below we set a scrollbar view");
                UI_BeginScrollbarView(UI_Context);
                {
                    for( i32 i = 0; i < 20; i += 1 ) {
                        char buf[64] = {0};
                        snprintf(buf, 64, "Label number %d", i);
                        UI_Label(UI_Context, buf);
                    }
                }
                UI_EndScrollbarView(UI_Context);
            }
            UI_EndTreeNode(UI_Context);
        }
        UI_WindowEnd(UI_Context);

		D_BeginDraw2D(&DrawInstance);

        UI_End(UI_Context, &DrawInstance);

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

		R_SendDataToBuffer(
			&Render, 
			StagBuffer[Render.VulkanBase->CurrentFrame], 
			DrawInstance.Current2DBuffer.data, 
			DrawInstance.Current2DBuffer.offset, 0
		);
		R_SendDataToBuffer(
			&Render, 
			StagBuffer[Render.VulkanBase->CurrentFrame], 
			idx, 6 * sizeof(u32), 
			DrawInstance.Current2DBuffer.offset
		);
			
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
			R_DispatchCompute(&Render, ComputePipeline, ceilf(window_width / 32), ceilf(window_height / 32), 1);
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

        stack_free_all(UI_Context->TempAllocator);
        stack_free_all(&VkBase.TempAllocator);
		stack_free_all(&TempAllocator);
        //clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts2);
        //posix_dur = 1000.0 * ts2.tv_sec + 1e-6 * ts2.tv_nsec - (1000.0 * ts1.tv_sec + 1e-6 * ts1.tv_nsec);
    }

    //XCloseDisplay(VkBase.Window.Dpy);

    TempEnd( arena_temp );

    return 0;
}

internal r_vertex_input_description 
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