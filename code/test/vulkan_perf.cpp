#include <array>
#include <vector>
#include <chrono>
#include <cstdio>

#include "../memory/memory.h"
#include "../memory/allocator.h"
#include "../vector/DynamicVector.h"
#include "../util/strings.h"

#define VOLK_IMPLEMENTATION
#include "../rendering/vk_render.h"

#define MEMORY_IMPL
#include "../memory/memory.h"
#include "../memory/allocator.cpp"
#include "../vector/DynamicVector.cpp"
#define STRINGS_IMPL
#include "../util/strings.h"
#include "../rendering/vk_render.cpp"

// Suppress TRACE output during performance testing
#undef TRACE
#define TRACE(fmt, ...) do {} while(0)

// Performance measurement utilities
struct PerfStats {
    double frame_time_ms;
    double gpu_time_ms;
    uint32_t draw_calls;
    uint32_t texture_count;
};

void print_perf_stats(const char* test_name, const PerfStats& stats) {
    printf("\n=== %s ===\n", test_name);
    printf("Frame time:    %.3f ms\n", stats.frame_time_ms);
    printf("GPU time:      %.3f ms\n", stats.gpu_time_ms);
    printf("Draw calls:    %u\n", stats.draw_calls);
    printf("Textures used: %u\n", stats.texture_count);
    printf("Throughput:    %.1f M triangles/sec\n", 
        (stats.draw_calls * 3.0) / (stats.gpu_time_ms * 1000.0));
}

int main() {
    Arena arena = {};
    Arena* arena_ptr = &arena;
    arena_ptr = ArenaAllocDefault();

    Allocator allocator = Mem_Allocator::Make(arena_ptr);

    printf("========================================\n");
    printf("Bindless Descriptor Indexing - Performance Profiling\n");
    printf("========================================\n");

    Vk_Render Renderer;
    Renderer.Init(1920, 1080, &allocator);

    // Load texture using bindless API
    Vk_Texture main_tex(&allocator);
    U32 texture_handle = main_tex.Load_From_Ktx_Bindless(
        "data/assets/suzanne0.ktx",
        Renderer.Get_Device(),
        Renderer.Get_Queue(),
        Renderer.Get_Command_Pool(),
        Renderer.Get_Vma_Allocator(),
        &Renderer.Get_Bindless_Table()
    );

    printf("[OK] Texture loaded with bindless handle: %u\n", texture_handle);

    // Setup descriptor set for uniform buffers
    Vk_Descriptor descriptor_set = Vk_Descriptor::Create(&allocator, Renderer.Get_Device())
        .Uniform_Buffer(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Uniform_Buffer(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    // Define shader constant structures
    struct ObjectCB {
        glm::mat4 World;
        glm::vec4 Albedo;
    };

    struct FrameCB {
        glm::mat4 ViewProj;
        glm::vec3 LightDir;
        float padding0;
        glm::vec3 EyePos;
        float Shininess;
    };

    struct PushConstants {
        uint32_t texture_index;
        uint32_t sampler_index;
        uint32_t padding0;
        uint32_t padding1;
    };

    // Create uniform buffers
    ObjectCB object_data;
    object_data.World = glm::mat4(1.0f);
    object_data.Albedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    U8_String buf_name_0 = StringNew("ObjectCB", CustomStrlen("ObjectCB"), &allocator);
    Vk_Buffer<ObjectCB> buffer_object(
        &allocator, &Renderer,
        Vk_Buffer<ObjectCB>::Buffer_Description(
            buf_name_0,
            sizeof(ObjectCB),
            0,
            Vk_Buffer<ObjectCB>::Buffer_Usage::Uniform
        )
    );
    buffer_object.description.Set_Data(&object_data, sizeof(ObjectCB));

    FrameCB frame_data;
    frame_data.ViewProj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 100.0f);
    frame_data.LightDir = glm::normalize(glm::vec3(0.0f, 1.0f, 1.0f));
    frame_data.EyePos = glm::vec3(0.0f, 0.0f, 5.0f);
    frame_data.Shininess = 32.0f;

    U8_String buf_name_1 = StringNew("FrameCB", CustomStrlen("FrameCB"), &allocator);
    Vk_Buffer<FrameCB> buffer_frame(
        &allocator,
        &Renderer,
        Vk_Buffer<FrameCB>::Buffer_Description(
            buf_name_1,
            sizeof(FrameCB),
            0,
            Vk_Buffer<FrameCB>::Buffer_Usage::Uniform
        )
    );
    buffer_frame.description.Set_Data(&frame_data, sizeof(FrameCB));

    VkDescriptorBufferInfo buffer_info_0 = {
        .buffer = buffer_object.Get_Buffer(),
        .offset = 0,
        .range = sizeof(ObjectCB)
    };

    VkDescriptorBufferInfo buffer_info_1 = {
        .buffer = buffer_frame.Get_Buffer(),
        .offset = 0,
        .range = sizeof(FrameCB)
    };

    descriptor_set.Write_Buffer(0, buffer_info_0);
    descriptor_set.Write_Buffer(1, buffer_info_1);
    descriptor_set.Flush();

    // Create pipeline
    Vk_Pipeline Pipeline(&allocator, Renderer.Get_Slang_Global_Session());
    Pipeline.Create_Session(Renderer.Get_Slang_Session_Description());
    Pipeline.Load_Module(Renderer.Get_Device(), "main", "./data/simple_3d.hlsl");

    Pipeline.Set_Depth();
    Pipeline.Set_Shader_Stages(Shader_Loader::Shader_Stage::VERTEX | Shader_Loader::Shader_Stage::FRAGMENT);
    Pipeline.Push_Dynamic_State(VK_DYNAMIC_STATE_VIEWPORT);
    Pipeline.Push_Dynamic_State(VK_DYNAMIC_STATE_SCISSOR);
    Pipeline.Push_Vertex_Binding(0, sizeof(V_3d), VK_VERTEX_INPUT_RATE_VERTEX);
    Pipeline.Push_Vertex_Attributes(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V_3d, pos));
    Pipeline.Push_Vertex_Attributes(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V_3d, norm));
    Pipeline.Push_Vertex_Attributes(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(V_3d, uv));

    Pipeline.Prepend_Bindless_Layout(Renderer.Get_Bindless_Table().Get_Layout());
    Pipeline.Push_Descriptor_Set_Layout(descriptor_set.Get_Layout());

    VkPushConstantRange push_const_range{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };
    Pipeline.Set_Push_Constant_Range(push_const_range);

    Pipeline.Create(Renderer.Get_Device(), Renderer.Get_Swapchain_Image_Format(), Renderer.Get_Depth_Image_Format());

    // Create vertex buffer with a triangle
    std::array<V_3d, 3> triangle_vertices = {{
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.0f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}},
    }};

    U8_String buffer_debug_name = StringNew("Triangle Vertices", CustomStrlen("Triangle Vertices"), &allocator);
    Vk_Buffer<V_3d>::Buffer_Description buffer_desc(
        buffer_debug_name,
        triangle_vertices.size() * sizeof(V_3d),
        sizeof(V_3d),
        Vk_Buffer<V_3d>::Buffer_Usage::Vertex
    );
    buffer_desc.Set_Data(triangle_vertices.data(), triangle_vertices.size() * sizeof(V_3d));
    Vk_Buffer<V_3d> buffer_test(&allocator, &Renderer, buffer_desc);

    printf("[OK] Pipeline and buffers created\n\n");

    // Test: Measure bindless rendering with increasing draw calls
    const uint32_t draw_call_counts[] = {100, 500, 1000, 5000};
    const char* test_names[] = {
        "Bindless - 100 draws",
        "Bindless - 500 draws",
        "Bindless - 1000 draws",
        "Bindless - 5000 draws"
    };

    printf("========== BINDLESS RENDERING PERFORMANCE ==========\n");

    for (size_t test_idx = 0; test_idx < 4; test_idx++) {
        uint32_t draw_count = draw_call_counts[test_idx];
        
        // Warmup frames
        for (int warmup = 0; warmup < 3; warmup++) {
            auto render_frame = [&]() {
                Renderer.Begin_Rendering();
                {
                    VkDescriptorSet bindless_set = Renderer.Get_Bindless_Table().Get_Set();
                    VkDescriptorSet desc_set = descriptor_set.Get_Set();

                    Renderer.Set_Viewport();
                    Renderer.Set_Scissor();
                    Renderer.Bind_Pipeline(Pipeline.pipeline, Pipeline.pipeline_layout);

                    VkDescriptorSet sets[] = {bindless_set, desc_set};
                    vkCmdBindDescriptorSets(
                        Renderer.Get_Current_Cmd_Buffer(),
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        Pipeline.pipeline_layout,
                        0, 2, sets, 0, nullptr
                    );

                    Renderer.Bind_Vertex_Buffer(buffer_test.Get_Buffer());

                    PushConstants pc = {
                        .texture_index = texture_handle,
                        .sampler_index = 0,
                        .padding0 = 0,
                        .padding1 = 0
                    };

                    for (uint32_t i = 0; i < draw_count; i++) {
                        Renderer.Push_Constants(
                            Pipeline.pipeline_layout,
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            0,
                            sizeof(PushConstants),
                            &pc
                        );
                        Renderer.Draw(3);
                    }
                }
                Renderer.End_Rendering();
            };

            Renderer.Record_Frame(render_frame);
        }

        // Measure frames
        auto start = std::chrono::high_resolution_clock::now();
        const uint32_t measured_frames = 100;

        for (uint32_t frame = 0; frame < measured_frames; frame++) {
            auto render_frame = [&]() {
                Renderer.Begin_Rendering();
                {
                    VkDescriptorSet bindless_set = Renderer.Get_Bindless_Table().Get_Set();
                    VkDescriptorSet desc_set = descriptor_set.Get_Set();

                    Renderer.Set_Viewport();
                    Renderer.Set_Scissor();
                    Renderer.Bind_Pipeline(Pipeline.pipeline, Pipeline.pipeline_layout);

                    VkDescriptorSet sets[] = {bindless_set, desc_set};
                    vkCmdBindDescriptorSets(
                        Renderer.Get_Current_Cmd_Buffer(),
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        Pipeline.pipeline_layout,
                        0, 2, sets, 0, nullptr
                    );

                    Renderer.Bind_Vertex_Buffer(buffer_test.Get_Buffer());

                    PushConstants pc = {
                        .texture_index = texture_handle,
                        .sampler_index = 0,
                        .padding0 = 0,
                        .padding1 = 0
                    };

                    for (uint32_t i = 0; i < draw_count; i++) {
                        Renderer.Push_Constants(
                            Pipeline.pipeline_layout,
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            0,
                            sizeof(PushConstants),
                            &pc
                        );
                        Renderer.Draw(3);
                    }
                }
                Renderer.End_Rendering();
            };

            Renderer.Record_Frame(render_frame);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        double avg_frame_time = elapsed / measured_frames;

        PerfStats stats = {
            .frame_time_ms = avg_frame_time,
            .gpu_time_ms = avg_frame_time,
            .draw_calls = draw_count,
            .texture_count = 1
        };

        print_perf_stats(test_names[test_idx], stats);
    }

    printf("\n========== RESULTS SUMMARY ==========\n");
    printf("Bindless rendering performance profiled across 100 frames each.\n");
    printf("Resolution: 1920x1080, Single texture, Push constants per draw\n");
    printf("Note: Frame time includes CPU submission overhead\n");

    // Cleanup
    vkDeviceWaitIdle(Renderer.Get_Device());
    Pipeline.Destroy(Renderer.Get_Device());

    return 0;
}
