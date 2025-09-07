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

    vulkan_base VkBase = VulkanInit();

    mu_Context* UI_Context = UI_Init(&VkBase.Allocator, "./data/RobotoMono.ttf");

    UI_Graphics gfx = UI_GraphicsInit(&VkBase, (FontCache*)UI_Context->style->font);

    XEvent ev;
    bool running = true;
    while( running ) {
        u32 window_width  = gfx.base->Swapchain.Extent.width;
        u32 window_height = gfx.base->Swapchain.Extent.height;

        UI_Input Input = UI_HandleEvents(&gfx, UI_Context);
        if( (Input & StopUI) == StopUI ) {
            running = false;
            break;
        }

        {
            UI_Begin(UI_Context);

            mu_Rect rect = mu_rect(40, 40, 200, 200);
            mu_begin_window_ex(UI_Context, "Window", rect, MU_OPT_NOCLOSE);
            mu_Container* window_cnt = mu_get_current_container(UI_Context);
            mu_layout_row(UI_Context, 1, (const int[]){-1}, 0);
            if(mu_header(UI_Context, "Command Sender") == MU_RES_ACTIVE ) {
                mu_layout_row(UI_Context, 4, (const int[]){window_width / 4, window_height / 3, 200, -1}, 0);
                mu_Rect CurrentRect = mu_layout_next(UI_Context);
                CurrentRect.w = window_cnt->content_size.x;
                mu_draw_rect(UI_Context, CurrentRect, (mu_Color){72, 147, 104, 255});
                mu_layout_set_next(UI_Context, CurrentRect, false);

                mu_label(UI_Context, "File Transfer List");
                static char RemotePath[256];
                if( mu_textbox(UI_Context, RemotePath, sizeof(RemotePath)) & MU_RES_SUBMIT ) {
                    mu_set_focus(UI_Context, UI_Context->last_id);
                }

                mu_label(UI_Context, "Command ACK");
                mu_label(UI_Context, "Waiting...");

                mu_layout_row(UI_Context, 5, (const int[]){window_width / 4, window_height / 3, window_height / 3, 200, -1}, 0);
                CurrentRect = mu_layout_next(UI_Context);
                CurrentRect.w = window_cnt->content_size.x;
                mu_draw_rect(UI_Context, CurrentRect, (mu_Color){72, 147, 104, 255});
                mu_layout_set_next(UI_Context, CurrentRect, false);
                mu_label(UI_Context, "File Transfer Download");
                static bool download_remote_submit = false;
                static bool download_local_submit  = false;
                static char DownloadRemotePath[256];
                static char DownloadLocalPath[256];
                if( mu_textbox(UI_Context, DownloadRemotePath, sizeof(DownloadRemotePath)) & MU_RES_SUBMIT ) {
                    mu_set_focus(UI_Context, UI_Context->last_id);
                    download_remote_submit = true;
                }
                if( mu_textbox(UI_Context, DownloadLocalPath, sizeof(DownloadLocalPath)) & MU_RES_SUBMIT ) {
                    mu_set_focus(UI_Context, UI_Context->last_id);
                    download_local_submit = true;
                }
                if( download_remote_submit && download_local_submit ) {
                    download_remote_submit = download_local_submit = false;
                }
                mu_label(UI_Context, "Command ACK");
                mu_label(UI_Context, "Waiting...");

                mu_layout_row(UI_Context, 4, (const int[]){window_width / 4, window_height / 3, 200, -1}, 0);
                CurrentRect = mu_layout_next(UI_Context);
                CurrentRect.w = window_cnt->content_size.x;
                mu_draw_rect(UI_Context, CurrentRect, (mu_Color){72, 147, 104, 255});
                mu_layout_set_next(UI_Context, CurrentRect, false);
                mu_label(UI_Context, "Download OBC HK");
                if( mu_button(UI_Context, "Download Now") & MU_RES_SUBMIT ) {
                    mu_set_focus(UI_Context, UI_Context->last_id);
                }
                mu_label(UI_Context, "Command ACK");
                mu_label(UI_Context, "Waiting...");
            }
            if (mu_button(UI_Context, "Telemetry Watch") == MU_RES_SUBMIT ) {

            }
            if( mu_button(UI_Context, "Database") == MU_RES_SUBMIT ) {
            }

            mu_end_window(UI_Context);

            UI_End(&gfx, UI_Context, &gfx.base->TempAllocator);
        }

        UI_Render(&gfx);

        stack_free_all(&gfx.base->TempAllocator);
    }

    XCloseDisplay(VkBase.Window.Dpy);

    TempEnd( arena_temp );

    return 0;
}
