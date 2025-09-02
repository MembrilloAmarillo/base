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
        {
            Window window_returned;
            int root_x, root_y;
            int win_x, win_y;
            u32 mask_return;
            XQueryPointer(
                gfx.base->Window.Dpy,
                gfx.base->Window.Win,
                &window_returned,
                &window_returned,
                &root_x, &root_y,
                &win_x, &win_y,
                &mask_return
            );
            mu_input_mousemove(UI_Context, win_x, win_y);
        }
        while (XPending(gfx.base->Window.Dpy)) {
            XNextEvent(VkBase.Window.Dpy, &ev);
            if (ev.type == KeyPress && ev.xkey.keycode == XK_Escape ) {
                running = false;
            }
            typedef struct {
                KeySym x_key;
                int mu_key;
            } KeyMap;

            int mouse_button_pressed = 0;
            static KeyMap keys_to_check[] = {
                { XK_BackSpace, MU_KEY_BACKSPACE },
                { XK_Return,    MU_KEY_RETURN },
                { XK_Shift_L,   MU_KEY_SHIFT },
                { XK_Shift_R,   MU_KEY_SHIFT },
                { XK_Control_L, MU_KEY_CTRL },
                { XK_Control_R, MU_KEY_CTRL },
                { XK_Meta_L,    MU_KEY_ALT },
                { XK_Meta_R,    MU_KEY_ALT },
            };

            switch (ev.type) {
                // --- Window Events ---
                case ConfigureNotify: {
                    XConfigureEvent* ce = (XConfigureEvent*)&ev;
                    if (ce->width != window_width || ce->height != window_height) {
                        gfx.base->FramebufferResized = true;
                    }
                    break;
                }

                // --- Mouse Events ---
                case MotionNotify: {
                    XMotionEvent* me = (XMotionEvent*)&ev;
                    // Optional: handle mouse movement
                    break;
                }

                case ButtonPress: {
                    XButtonEvent* be = (XButtonEvent*)&ev;
                    mouse_button_pressed = MU_MOUSE_LEFT;
                    mu_input_mousedown(UI_Context, ev.xbutton.x, ev.xbutton.y, mouse_button_pressed);
                    break;
                }

                case ButtonRelease: {
                    XButtonEvent* be = (XButtonEvent*)&ev;
                    mouse_button_pressed = MU_MOUSE_LEFT;
                    mu_input_mouseup(UI_Context, ev.xbutton.x, ev.xbutton.y, mouse_button_pressed);
                    mouse_button_pressed = 0;
                    break;
                }

                case KeyPress: {
                    char buffer[256];
                    KeySym keysym_from_utf8 = 0;
                    int num_bytes = Xutf8LookupString(gfx.base->Window.xic, &ev.xkey, buffer, sizeof(buffer), &keysym_from_utf8, NULL);

                    int mu_key = 0;
                    for (size_t i = 0; i < ArrayCount(keys_to_check); ++i) {
                        if (keysym_from_utf8 == keys_to_check[i].x_key) {
                            mu_key = keys_to_check[i].mu_key;
                        }
                    }
                    if (mu_key) {
                        mu_input_keydown(UI_Context, mu_key);
                    }

                    if (num_bytes > 0 && !mu_key) {
                        buffer[num_bytes] = '\0';
                        mu_input_text(UI_Context, buffer);   // text input (printable chars)
                    }
                    break;
                }

                case KeyRelease: {
                    KeySym keysym = XkbKeycodeToKeysym(gfx.base->Window.Dpy, ev.xkey.keycode, 0, 0);
                    for (size_t i = 0; i < sizeof(keys_to_check)/sizeof(keys_to_check[0]); ++i) {
                        if (keysym == keys_to_check[i].x_key) {
                            mu_input_keyup(UI_Context, keys_to_check[i].mu_key);
                            break;
                        }
                    }
                    break;
                }

                // --- Quit on window close ---
                case ClientMessage: {
                    //if ((Atom)ev.xclient.data.l[0] == wm_delete_window) {
                    //    //running = false;
                    //}
                    break;
                }

                default:
                    break;
            }
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
        {
            bool do_resize = PrepareFrame(gfx.base);
            if( do_resize ) {
                UI_ExternalRecreateSwapchain(&gfx);
            } else {
                VkCommandBuffer cmd = BeginRender(gfx.base);
                {
                    memcpy(
                        gfx.ui_vertex_staging[gfx.base->CurrentFrame].Info.pMappedData,
                        gfx.ui_rects.data,
                        gfx.ui_rects.offset
                    );

                    void* data = gfx.ui_vertex_staging[gfx.base->CurrentFrame].Info.pMappedData;
                    void* data_offset = (u8*)data + gfx.ui_rects.offset;

                    memcpy(
                        data_offset,
                        gfx.ui_indxs.data,
                        gfx.ui_indxs.offset
                    );

                    VkBufferCopy vertexCopy = {0};
                    vertexCopy.dstOffset = 0;
                    vertexCopy.srcOffset = 0;
                    vertexCopy.size = gfx.ui_rects.offset;

                    vkCmdCopyBuffer(
                        cmd,
                        gfx.ui_vertex_staging[gfx.base->CurrentFrame].Buffer,
                        gfx.ui_buffer[gfx.base->CurrentFrame].Buffer,
                        1,
                        &vertexCopy
                    );

                    VkBufferCopy indexCopy = {0};
                    indexCopy.dstOffset = 0;
                    indexCopy.srcOffset = gfx.ui_rects.offset;
                    indexCopy.size      = gfx.ui_indxs.offset;

                    if( indexCopy.srcOffset + indexCopy.size > gfx.ui_buffer_idx[gfx.base->CurrentFrame].Info.size ) {
                        fprintf(stderr, "[VULKAN_ERROR] Copy size is greater that destination size %d\n", gfx.ui_buffer_idx[gfx.base->CurrentFrame].Info.size);
                    }

                    vkCmdCopyBuffer(
                        cmd,
                        gfx.ui_vertex_staging[gfx.base->CurrentFrame].Buffer,
                        gfx.ui_buffer_idx[gfx.base->CurrentFrame].Buffer,
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
                        gfx.BG_TextureImage.Image,
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
                        gfx.BG_Pipeline.Pipeline
                    );

                    // bind the descriptor set containing the draw image for the compute pipeline
                    vkCmdBindDescriptorSets(
                        cmd,
                        VK_PIPELINE_BIND_POINT_COMPUTE,
                        gfx.BG_Pipeline.Layout,
                        0, 1,
                        &gfx.BG_DescriptorSet,
                        0, 0
                    );

                    VkExtent3D Extent = gfx.BG_TextureImage.Extent;
                    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
                    vkCmdDispatch(
                        cmd,
                        ceil((f32)Extent.width / 32.0),
                        ceil((f32)Extent.height / 32.0),
                        1
                    );

                    TransitionImage(
                        cmd,
                        gfx.BG_TextureImage.Image,
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

                VkExtent2D Extent2d = gfx.base->Swapchain.Extent;
                VkRenderingAttachmentInfo  colorAttachment = AttachmentInfo(gfx.base->Swapchain.ImageViews[gfx.base->SwapchainImageIdx], &clear_value, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                VkRenderingInfo renderInfo                 = RenderingInfo( Extent2d, &colorAttachment, NULL );
                vkCmdBeginRendering(cmd, &renderInfo);
                {
                    vkCmdBindPipeline(
                        cmd,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        gfx.BG_RenderPipeline.Pipeline
                    );
                    vkCmdBindDescriptorSets(
                        cmd,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        gfx.BG_RenderPipeline.Layout,
                        0, 1,
                        &gfx.BG_RenderDescriptorSet,
                        0, 0
                    );
                    VkViewport viewport = {0};
                    viewport.x = 0;
                    viewport.y = 0;
                    viewport.width  = gfx.base->Swapchain.Extent.width;
                    viewport.height = gfx.base->Swapchain.Extent.height;
                    viewport.minDepth = 0;
                    viewport.maxDepth = 1;

                    vkCmdSetViewport(cmd, 0, 1, &viewport);

                    VkRect2D scissor = {
                        .offset = {0, 0},
                        .extent = gfx.base->Swapchain.Extent
                    };
                    vkCmdSetScissor(cmd, 0, 1, &scissor);
                    vkCmdDraw(cmd, 3, 1, 0, 0);
                }

                ///////////////////////////////////////////////////////////
                // UI Rendering
                {
                    VkExtent2D Extent = gfx.base->Swapchain.Extent;
                    vkCmdBindPipeline(
                        cmd,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        gfx.UI_Pipeline.Pipeline
                    );

                    vkCmdBindDescriptorSets(
                        cmd,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        gfx.UI_Pipeline.Layout, 0, 1,
                        &gfx.UI_DescriptorSet, 0, 0
                    );

                    VkBuffer Buffers[] = {gfx.ui_buffer[gfx.base->CurrentFrame].Buffer};
                    VkDeviceSize offsets[] = {0};
                    vkCmdBindVertexBuffers(
                        cmd, 0, 1,
                        Buffers,
                        offsets
                    );
                    vkCmdBindIndexBuffer(
                        cmd,
                        gfx.ui_buffer_idx[gfx.base->CurrentFrame].Buffer,
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
                    vkCmdDrawIndexed(cmd, gfx.ui_indxs.len, 1, 0, 0, 0);
                }

                vkCmdEndRendering(cmd);

                TransitionImageDefault(
                    cmd,
                    gfx.base->Swapchain.Images[gfx.base->SwapchainImageIdx],
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                );

                do_resize = EndRender(gfx.base, cmd);
                if( do_resize ) {
                    UI_ExternalRecreateSwapchain(&gfx);
                }
            }
        }
        stack_free_all(&gfx.base->TempAllocator);
    }

    XCloseDisplay(VkBase.Window.Dpy);

    TempEnd( arena_temp );

    return 0;
}
