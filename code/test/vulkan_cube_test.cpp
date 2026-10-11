#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <SDL3/SDL.h> // Input stays separate from the rendering API.

#include "../util/types.h"
#include "../io/files.h"

#include "../rendering/graphics.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../third-party/stb_truetype.h"

#define FILES_IMPL
#include "../io/files.h"

/**
 * @note The data should be freed by the user
 */
u32* get_spirv_data(const char *file, u32 *size_out) {
    f_file f = F_OpenFile(file, RDONLY);

    i32 len = F_FileLength(&f);

    u32 *data = (u32*)calloc(len, sizeof(u32));

    F_SetFileData(&f, data);
    F_FileRead(&f);
    
    F_CloseFile(&f);

    return data;
}

int main(int argc, char** argv) {

    Graphics_Device_t tDevice = {0};
    Graphics_Result tResult = graphics_init(&tDevice);
    if (tResult != Graphics_NONE) {
        std::fprintf(stderr, "Graphics initialization failed: %d\n", tResult);
        SDL_Quit();
        return 1;
    }

    uint32_t n_descriptors = 8;
    Gpu_Resource_t tResource = {0};
    tResult = graphics_create_resources(&tDevice, n_descriptors, &tResource);
    assert(tResult == Graphics_NONE);
        
    int exit_code = 0;
    std::fprintf(stderr, "Graphics resource creation failed: %d\n", tResult);
    exit_code = 1;
        
    u32 *vert_spirv = NULL;
    u32 *frag_spirv = NULL;
    u32 vert_size = 0;
    u32 frag_size = 0;
    vert_spirv = get_spirv_data("./shaders/Sample.vert.spv", &vert_size);
    frag_spirv = get_spirv_data("./shaders/Sample.frag.spv", &frag_size);
    Gpu_PSO_Desc_t pso_desc = graphics_pso_desc(
        .first_stage_spirv = vert_spirv,
        .first_stage_spirv_size = vert_size,
        .fragment_spirv = frag_spirv,
        .fragment_spirv_size = frag_size,
        .set_layout = true
        );
    Gpu_Pipeline_t rect_pipeline = {0};
    tResult = graphics_create_pso(&tDevice, &rect_pipeline, &pso_desc);

    assert(tResult == Graphics_NONE);

    Gpu_Buffer_t rect_buffer;
    tResult = graphics_alloc(&tDevice, &rect_buffer, (VkDeviceSize)(1 << 20), CPU_VISIBLE);
    assert(tResult == Graphics_NONE);

    const SDL_WindowID window_id = SDL_GetWindowID(tDevice.window);
    bool running = true;
    while (running) {
        SDL_Event event;
        if (!SDL_WaitEvent(&event)) {
            std::fprintf(stderr, "SDL_WaitEvent failed: %s\n", SDL_GetError());
            exit_code = 1;
            break;
        }

        if (event.type == SDL_EVENT_QUIT ||
            (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
             event.window.windowID == window_id)) {
            running = false;
        }
    }

    SDL_DestroyWindow(tDevice.window);
    SDL_Quit();
    return exit_code;
}
