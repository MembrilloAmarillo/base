/**
 * Sample to demonstrate how to use the UI and render it, for now only X11 supported.
 *
 * @author Sascha Paez
 * @version 1.2
 * @since 2025
 *
 */

#include "UI_Sample.h"

/**
 * @brief Main function
 * @param[in] argc Number of arguments passed by the command line
 * @param[in] argv The actual arguments passed by the command line
 * @return int status
 */
int
main( int argc, char *argv[] )
{
    Arena* main_arena = ArenaAllocDefault();
    Temp   arena_temp = TempBegin( main_arena );

    Stack_Allocator Allocator;
    Stack_Allocator TempAllocator;
    u8* Buffer     = PushArray(main_arena, u8, mebibyte(256));
    u8* TempBuffer = PushArray(main_arena, u8, mebibyte(256));
    stack_init(&Allocator, Buffer, mebibyte(256));
    stack_init(&TempAllocator, TempBuffer, mebibyte(256));

    vulkan_base VkBase = VulkanInit();

    ui_context* UI_Context = PushArray(main_arena, ui_context, 1);

    // @todo: Change how to add fonts this is highly inconvenient
    //
    u8* BitmapArray = stack_push(&Allocator, u8, 2100 * 1200);
    FontCache DefaultFont = F_BuildFont(22, 2100, 60, BitmapArray, "./data/RobotoMono.ttf");
    FontCache TitleFont   = F_BuildFont(26, 2100, 60, BitmapArray + (2100*60), "./data/TinosNerdFontPropo.ttf");
    TitleFont.BitmapOffset = (vec2){0, 60};

    UI_Graphics gfx = UI_GraphicsInit(&VkBase, BitmapArray, 2100, 1200);

    UI_Init(UI_Context, &gfx, &Allocator, &TempAllocator);

    rgba bd  = RgbaNew(72,  127, 134, 255);
    rgba bg  = RgbaNew(240, 236, 218, 255);
    rgba fg  = RgbaNew(21,   39,  64, 255);
    rgba bg2 = RgbaNew(19,  76, 101, 255);

    object_theme DefaultTheme = {
        .Border              = RgbaToNorm(bd),
        .PanelBackground     = RgbaToNorm(bg),
        .WindowBackground    = RgbaToNorm(bg),
        .WindowForeground    = RgbaToNorm(fg),
        .LabelForeground     = RgbaToNorm(fg),
        .ButtonForeground    = RgbaToNorm(bg),
        .ButtonBackground    = RgbaToNorm(bg2),
        .ButtonHoverBackground = RgbaToNorm(bd),
        .ButtonPressBackground = RgbaToNorm(bg),
        .TextInputForeground = RgbaToNorm(fg),
        .TextInputBackground = RgbaToNorm(bg),
        .TextInputCursor     = RgbaToNorm(fg),
        .WindowRadius        = 0,
        .ButtonRadius        = 0,
        .InputTextRadius     = 0,
        .Font = &DefaultFont
    };

    UI_Context->DefaultTheme = DefaultTheme;

    XEvent ev;
    bool running = true;
    struct timespec ts2, ts1;

    double posix_dur = 0;
    while( running ) {
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts1);
        u32 window_width  = gfx.base->Swapchain.Extent.width;
        u32 window_height = gfx.base->Swapchain.Extent.height;

        UI_Begin(UI_Context);

        ui_input Input = UI_LastEvent(UI_Context);
        if( Input == StopUI ) {
            fprintf(stderr, "[INFO] Finished Application\n");
            running = false;
            break;
        }

        UI_SetNextTheme(UI_Context, DefaultTheme);
        UI_PushNextFont(UI_Context, &TitleFont);
        UI_WindowBegin(UI_Context, (rect_2d){ {20, 20}, {400, 400} }, "Window Title", UI_Select | UI_Drag | UI_Resize);
         UI_PopTheme(UI_Context);
         if( UI_Button(UI_Context, "Hello Button 1")  & LeftClickPress  ) {
         }
         UI_Label(UI_Context, "This is my new label");
         if( UI_Button(UI_Context, "Hello Button 2") & LeftClickPress ) {
         }
         if( UI_BeginTreeNode(UI_Context, "Tree Node") & ActiveObject ) {
            UI_Label(UI_Context, "My Tree Node label!!");
         }
         UI_EndTreeNode(UI_Context);
         UI_TextBox(UI_Context, "Input Text");
         UI_TextBox(UI_Context, "Second Input Text");
         u8 buf[126] = {0};
         sprintf(buf, "Refresh Rate (ms): %.2f", posix_dur);
         UI_LabelWithKey(UI_Context, "Refresh Rate", buf);
        UI_WindowEnd(UI_Context);

        UI_End(UI_Context);

        bool do_resize = PrepareFrame(gfx.base);
        if( do_resize ) {
            UI_ExternalRecreateSwapchain(&gfx);
        } else {
            gfx.UniformData.ScreenWidth   = gfx.base->Swapchain.Extent.width;
           	gfx.UniformData.ScreenHeight  = gfx.base->Swapchain.Extent.height;
           	gfx.UniformData.TextureWidth  = gfx.UI_TextureImage.Width;
           	gfx.UniformData.TextureHeight = gfx.UI_TextureImage.Height;
            VkMappedMemoryRange flushRange = {0};
            flushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            // Get the VkDeviceMemory from your buffer's allocation info
            flushRange.memory = gfx.UniformBuffer.Info.deviceMemory;
            flushRange.offset = 0;
            flushRange.size = VK_WHOLE_SIZE; // Or sizeof(ui_uniform)
            vkFlushMappedMemoryRanges(gfx.base->Device, 1, &flushRange);
           	memcpy(
           		gfx.UniformBuffer.Info.pMappedData,
           		&gfx.UniformData,
           		sizeof(ui_uniform)
           	);
            VkCommandBuffer cmd = BeginRender(gfx.base);

            UI_Render(&gfx, cmd);

            do_resize = EndRender(gfx.base, cmd);

            if( do_resize ) {
                UI_ExternalRecreateSwapchain(&gfx);
            }
        }

        stack_free_all(UI_Context->TempAllocator);
        stack_free_all(&gfx.base->TempAllocator);
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts2);
        posix_dur = 1000.0 * ts2.tv_sec + 1e-6 * ts2.tv_nsec - (1000.0 * ts1.tv_sec + 1e-6 * ts1.tv_nsec);
    }

    XCloseDisplay(VkBase.Window.Dpy);

    TempEnd( arena_temp );

    return 0;
}
