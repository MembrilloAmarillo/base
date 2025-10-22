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
    vulkan_base VkBase = VulkanInit();
    
    Arena *main_arena = VkBase.Arena;
    Temp arena_temp = TempBegin(main_arena);
    
    Stack_Allocator Allocator;
    Stack_Allocator TempAllocator;
    u8 *Buffer = PushArray(main_arena, u8, gigabyte(1));
    u8 *TempBuffer = PushArray(main_arena, u8, mebibyte(256));
    stack_init(&Allocator, Buffer, gigabyte(1));
    stack_init(&TempAllocator, TempBuffer, mebibyte(256));

    
    ui_context *UI_Context = PushArray(main_arena, ui_context, 1);

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

    UI_Graphics gfx = UI_GraphicsInit(&VkBase, BitmapArray, 2100, 1200);

    UI_Init(UI_Context, &gfx, &Allocator, &TempAllocator);

    rgba bd = HexToRGBA(0x2D2D2DFF);
    rgba ib = HexToRGBA(0x2F2F2FFF);
    rgba fd = HexToRGBA(0x3D3D3DFF);
    rgba fg = RgbaNew(21, 39, 64, 255);
    rgba bg2 = HexToRGBA(0xA13535FF);
    rgba bc = HexToRGBA(0x3B764CFF);

    object_theme DefaultTheme = {.Border = RgbaToNorm(bc),
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
                                   .Font = NULL};

    UI_Context->DefaultTheme = (ui_theme){
        .Window = TitleTheme,
        .Button = ButtonTheme,
        .Panel = PanelTheme,
        .Input = InputTheme,
        .Label = LabelTheme,
        .Scrollbar = ScrollbarTheme
    };

    bool running = true;
    struct timespec ts2, ts1;

    double posix_dur = 0;

    while( running ) {
        //clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts1);
        u32 window_width  = gfx.base->Swapchain.Extent.width;
        u32 window_height = gfx.base->Swapchain.Extent.height;

        UI_Begin(UI_Context);

        ui_input Input = UI_LastEvent(UI_Context);

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

        UI_WindowBegin(
            UI_Context, 
            (rect_2d){{15, 15}, {window_width / 2 - 30, window_height / 2 - 30}}, 
            "Hello Title 2",
            UI_AlignCenter | UI_SetPosPersistent | UI_Drag | UI_Select | UI_Resize
        ); 
        {
            if( UI_BeginTreeNode(UI_Context, "Tree 2") & ActiveObject ) {
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

		UI_WindowBegin(
            UI_Context, 
            (rect_2d){{45, 15}, {window_width / 2 - 30, window_height / 2 - 30}}, 
            "Hello Title 3",
            UI_AlignCenter | UI_SetPosPersistent | UI_Drag | UI_Select | UI_Resize
        ); 
        {
            if( UI_BeginTreeNode(UI_Context, "Tree 2") & ActiveObject ) {
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

        UI_End(UI_Context);

        bool do_resize = PrepareFrame(gfx.base);
        if( do_resize ) {
            UI_ExternalRecreateSwapchain(&gfx);
        } else {
            VkCommandBuffer cmd = BeginRender(gfx.base);

            UI_Render(&gfx, cmd);

            do_resize = EndRender(gfx.base, cmd);

            if( do_resize ) {
                UI_ExternalRecreateSwapchain(&gfx);
            }
        }

        stack_free_all(UI_Context->TempAllocator);
        stack_free_all(&gfx.base->TempAllocator);
        //clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts2);
        //posix_dur = 1000.0 * ts2.tv_sec + 1e-6 * ts2.tv_nsec - (1000.0 * ts1.tv_sec + 1e-6 * ts1.tv_nsec);
    }

    //XCloseDisplay(VkBase.Window.Dpy);

    TempEnd( arena_temp );

    return 0;
}
