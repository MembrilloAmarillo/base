#include <array>
#include <vector>

#include "../memory/memory.h"
#include "../memory/allocator.h"
#include "../vector/DynamicVector.h"
#include "../util/strings.h"

#include "../rendering/vk_render.h"

#include "imconfig.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_sdl3.h"

#define MEMORY_IMPL
#include "../memory/memory.h"
#include "../memory/allocator.cpp"
#include "../vector/DynamicVector.cpp"
#define STRINGS_IMPL
#include "../util/strings.h"
#include "../rendering/vk_render.cpp"

int main() {
    Arena arena = {};
    Arena* arena_ptr = &arena;
    arena_ptr = ArenaAllocDefault();

    Allocator allocator = Mem_Allocator::Make(arena_ptr);

    Vk_Render Renderer;
    Renderer.Init(1200, 1200, &allocator);

    // Create ImGui context
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();  // Initialize IO

    // Get framebuffer scale for high DPI displays using SDL3 API
    float scale = SDL_GetWindowDisplayScale((SDL_Window*)Renderer.window.Win);
    printf("[ImGui] Display scale: %.2f\n", scale);
    
    // Set the framebuffer scale for ImGui
    io.DisplayFramebufferScale = ImVec2(scale, scale);

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Initialize SDL3 backend for input
    ImGui_ImplSDL3_InitForVulkan((SDL_Window*)Renderer.window.Win);

    // Prepare ImGui Vulkan init info
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = Renderer.instance;
    init_info.PhysicalDevice = Renderer.physical_device;
    init_info.Device = Renderer.device;
    init_info.QueueFamily = 0;  // You may need to extract this from Vk_Render
    init_info.Queue = Renderer.queue;
    init_info.DescriptorPoolSize = 1000;  // ImGui will create its own descriptor pool
    init_info.MinImageCount = 2;
    init_info.ImageCount = (U32)Renderer.swapchain_images.Capacity();
    init_info.UseDynamicRendering = true;

    // Setup pipeline rendering info for dynamic rendering
    VkFormat imgui_format_color_format{ VK_FORMAT_B8G8R8A8_SRGB };
    VkPipelineRenderingCreateInfoKHR pipeline_rendering_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &imgui_format_color_format,
        .depthAttachmentFormat = Renderer.Get_Depth_Image_Format()  // Match renderer's depth format
    };
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipeline_rendering_info;

    // Initialize Vulkan backend
    ImGui_ImplVulkan_Init(&init_info);

    // Create the main pipeline for dynamic rendering
    ImGui_ImplVulkan_CreateMainPipeline(&init_info.PipelineInfoMain);

    bool exit = false;
    bool show_demo_window = true;

    while( !exit ) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                exit = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID((SDL_Window*)Renderer.window.Win))
                exit = true;
        }

        if (SDL_GetWindowFlags((SDL_Window*)Renderer.window.Win) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // Begin frame - this calls Begin_Command_Buffer() internally
        Renderer.Begin_Frame();

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow(&show_demo_window);

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

        if (!is_minimized)
        {
            // Use a dark clear color for ImGui rendering
            VkClearColorValue clear_color = {{0.03f, 0.02f, 0.02f, 1.0f}};
            Renderer.Begin_Rendering(clear_color);
            {
                // Set viewport and scissor using drawable/framebuffer size (accounts for high DPI)
                float viewport_width = draw_data->DisplaySize.x * draw_data->FramebufferScale.x;
                float viewport_height = draw_data->DisplaySize.y * draw_data->FramebufferScale.y;
                Renderer.Set_Viewport(0.0f, 0.0f, viewport_width, viewport_height);
                Renderer.Set_Scissor(0, 0, (U32)viewport_width, (U32)viewport_height);

                // Process any pending texture updates before rendering
                ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
                if (platform_io.Textures.Size > 0)
                {
                    for (int i = 0; i < platform_io.Textures.Size; i++)
                    {
                        ImTextureData* tex = platform_io.Textures[i];
                        if (tex && tex->Status != ImTextureStatus_OK)
                        {
                            ImGui_ImplVulkan_UpdateTexture(tex);
                        }
                    }
                }
                
                ImGui_ImplVulkan_RenderDrawData(draw_data, Renderer.Get_Current_Cmd_Buffer());
            }
            Renderer.End_Rendering();
        }

        // End frame - this calls End_Command_Buffer() and submits work
        Renderer.End_Frame();
    }

    vkDeviceWaitIdle(Renderer.Get_Device());

    return 0;
}