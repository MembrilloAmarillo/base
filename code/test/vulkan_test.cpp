#include <array>
#include <vector>

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

int main() {
    Arena arena = {};
    Arena* arena_ptr = &arena;
    arena_ptr = ArenaAllocDefault();

    Allocator allocator = Mem_Allocator::Make(arena_ptr);

    Vk_Render Renderer;
    Renderer.Init(600, 600, &allocator);

    // Load texture using bindless API - automatically registers with bindless table
    Vk_Texture main_tex(&allocator);
    auto [texture_handle, sampler_handle] = main_tex.Load_From_Ktx_Bindless_With_Sampler(
        &Renderer,
        "data/assets/suzanne0.ktx"
    );

    TRACE("Texture loaded with handles: texture=%u, sampler=%u", texture_handle, sampler_handle);

    // Create descriptor set for uniform buffers (still using traditional descriptors for this)
    // Set 1 will hold per-frame data (camera, lighting)
    Vk_Descriptor descriptor_set = Vk_Descriptor::Create(&allocator, Renderer.Get_Device())
        .Uniform_Buffer(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Uniform_Buffer(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

     // Create uniform buffers for shader constants
    // ObjectCB: World matrix + Albedo color
    struct ObjectCB {
        glm::mat4 World;        // object -> world
        glm::vec4 Albedo;       // rgba color
    };

    // Push constants structure for bindless indices
    struct PushConstants {
        uint32_t texture_index;
        uint32_t sampler_index;
        uint32_t padding0;
        uint32_t padding1;
    };

    ObjectCB object_data;
    object_data.World = glm::mat4(1.0f);  // Identity matrix
    object_data.Albedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);  // White color

    U8_String buf_name_0 = StringNew("ObjectCB", CustomStrlen("ObjectCB"), &allocator);
    Vk_Buffer<ObjectCB>::Buffer_Description desc0(
        buf_name_0,
        sizeof(ObjectCB),
        0,
        Vk_Buffer<ObjectCB>::Buffer_Usage::Uniform
    );
    desc0.Set_Data(&object_data, sizeof(ObjectCB));
    Vk_Buffer<ObjectCB> buffer_object(&allocator, &Renderer, desc0);

    // FrameCB: ViewProj matrix + lighting data
    struct FrameCB {
        glm::mat4 ViewProj;     // view * projection
        glm::vec3 LightDir;     // direction TO the light
        float padding0;
        glm::vec3 EyePos;       // world-space camera position
        float Shininess;        // specular exponent
    };

    FrameCB frame_data;
    frame_data.ViewProj  = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 10.0f);  // near=0.1, far=10, camera at Z=5
    frame_data.LightDir  = glm::normalize(glm::vec3(0.0f, 1.0f, 1.0f));
    frame_data.EyePos    = glm::vec3(0.0f, 0.0f, 5.0f);
    frame_data.Shininess = 32.0f;

    U8_String buf_name_1 = StringNew("FrameCB", CustomStrlen("FrameCB"), &allocator);
    Vk_Buffer<FrameCB>::Buffer_Description desc1(
        buf_name_1,
        sizeof(FrameCB),
        0,
        Vk_Buffer<FrameCB>::Buffer_Usage::Uniform
    );
    desc1.Set_Data(&frame_data, sizeof(FrameCB));
    Vk_Buffer<FrameCB> buffer_frame(&allocator, &Renderer, desc1);

    // Write descriptor set for uniform buffers
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
    descriptor_set.Flush();  // Single vkUpdateDescriptorSets call

    TRACE("Descriptor set updated");

    Vk_Pipeline Pipeline( &allocator, Renderer.Get_Slang_Global_Session() );

    Pipeline.Create_Session(Renderer.Get_Slang_Session_Description() );
    Pipeline.Load_Module( Renderer.Get_Device(), "main", "./data/simple_3d.hlsl" );

    Pipeline.Set_Depth();
    Pipeline.Set_Shader_Stages(Shader_Loader::Shader_Stage::VERTEX | Shader_Loader::Shader_Stage::FRAGMENT);
    Pipeline.Push_Dynamic_State(VK_DYNAMIC_STATE_VIEWPORT);
    Pipeline.Push_Dynamic_State(VK_DYNAMIC_STATE_SCISSOR);
    Pipeline.Push_Vertex_Binding(0, sizeof(V_3d), VK_VERTEX_INPUT_RATE_VERTEX);
    Pipeline.Push_Vertex_Attributes(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V_3d, pos));
    Pipeline.Push_Vertex_Attributes(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V_3d, norm));
    Pipeline.Push_Vertex_Attributes(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(V_3d, uv));

    // Add bindless layout at set 0
    Pipeline.Prepend_Bindless_Layout(Renderer.Get_Bindless_Table().Get_Layout());

    // Add traditional descriptor layout at set 1 (for uniform buffers)
    Pipeline.Push_Descriptor_Set_Layout(descriptor_set.Get_Layout());

    // Add push constant range for bindless indices (16 bytes: 2 uint32 + 2 padding)
    VkPushConstantRange push_const_range{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };
    Pipeline.Set_Push_Constant_Range(push_const_range);

    Pipeline.Create(Renderer.Get_Device(), Renderer.Get_Swapchain_Image_Format(), Renderer.Get_Depth_Image_Format() );

    // Create and populate vertex buffer with a simple triangle
    std::array<V_3d, 3> triangle_vertices = {{
        // Position,              Normal,              UV
        {{-0.5f, -0.5f, 0.0f},  {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},  // Bottom-left
        {{ 0.5f, -0.5f, 0.0f},  {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},  // Bottom-right
        {{ 0.0f,  0.5f, 0.0f},  {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}},  // Top
    }};

    U8_String buffer_debug_name = StringNew("Triangle Vertices", CustomStrlen("Triangle Vertices"), &allocator);
    Vk_Buffer<V_3d>::Buffer_Description buffer_desc(buffer_debug_name, triangle_vertices.size() * sizeof(V_3d), sizeof(V_3d), Vk_Buffer<V_3d>::Buffer_Usage::Vertex);
    buffer_desc.Set_Data(triangle_vertices.data(), triangle_vertices.size() * sizeof(V_3d));
    Vk_Buffer<V_3d> buffer_test(&allocator, &Renderer, buffer_desc);

    TRACE("Buffer created: size=%zu bytes, %zu vertices", triangle_vertices.size() * sizeof(V_3d), triangle_vertices.size());

    // Example 1: Simple rendering with callback pattern
    // This is convenient for simple single-frame scenarios
    auto record_frame = [&]() {
        // Use red clear color to debug if rendering is happening
        VkClearColorValue clear_color = {{1.0f, 0.0f, 0.0f, 1.0f}};  // Red
        Renderer.Begin_Rendering(clear_color);
        {
            VkDescriptorSet bindless_set = Renderer.Get_Bindless_Table().Get_Set();
            VkDescriptorSet desc_set = descriptor_set.Get_Set();

            Renderer.Set_Viewport();
            Renderer.Set_Scissor();
            Renderer.Bind_Pipeline(Pipeline.pipeline, Pipeline.pipeline_layout);

            // Bind both descriptor sets: bindless at set 0, uniforms at set 1
            VkDescriptorSet sets[] = { bindless_set, desc_set };
            vkCmdBindDescriptorSets(
                Renderer.Get_Current_Cmd_Buffer(),
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                Pipeline.pipeline_layout,
                0,  // First set
                2,  // Number of sets
                sets,
                0,  // Dynamic offset count
                nullptr  // Dynamic offsets
            );

            Renderer.Bind_Vertex_Buffer(buffer_test.Get_Buffer());

            // Push the texture and sampler indices
            PushConstants pc = {
                .texture_index = texture_handle,
                .sampler_index = sampler_handle,  // Now using actual registered sampler handle
                .padding0 = 0,
                .padding1 = 0
            };

            Renderer.Push_Constants(
                Pipeline.pipeline_layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PushConstants),
                &pc
            );

            Renderer.Draw(3);  // Draw 3 vertices (triangle)
        }
        Renderer.End_Rendering();
    };

    bool exit = false;
    while( !exit ) {
        ui_input input = GetNextEvent(&Renderer.window);
        if( input & Input_Esc ) {
            exit = true;
        }

        Renderer.Record_Frame(record_frame);
    }

    // CRITICAL: Wait for GPU to finish before destroying resources
    // With 2 frames in flight, GPU might still be executing frame 0 commands
    vkDeviceWaitIdle(Renderer.Get_Device());

    // Cleanup
    Pipeline.Destroy(Renderer.Get_Device());

    // descriptor_set destructor automatically cleans up all Vulkan resources

    return 0;
}

