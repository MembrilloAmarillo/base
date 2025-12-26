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

#ifdef __linux__
#include <sys/types.h>
#include <dirent.h>
#else
#endif

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
#include "../DynamicVector.h"
#include "../encoders/obj_encoding.h"

#ifdef USE_FREETYPE
#define INCLUDE_FONT "../load_font_ft2.h"
#else
#define INCLUDE_FONT "../load_font.h"
#endif
#include INCLUDE_FONT
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
#include INCLUDE_FONT

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
#include "../DynamicVector.cpp"
#include "../encoders/obj_encoding.cpp"

#include <chrono>

global bool AppRunning = true;

int main(void) {
  std::chrono::time_point LastFrame = std::chrono::high_resolution_clock::now();

  Arena *Arena = ArenaAllocDefault();
  u8 *BackBuffer = PushArray(Arena, u8, mebibyte(256));
  Stack_Allocator Allocator;
  stack_init(&Allocator, (void*)BackBuffer, mebibyte(256));
  
  const char* path = "C:/Users/sasch/Downloads/kenney_mini-characters/Models/OBJ format/character-female-d.obj";
  obj_instance ObjInstance = OBJ_InstanceInit(path, static_cast<obj_load_flags>(0), Arena);

  if (false) {
    for (i32 i = 0; i < ObjInstance.Vec4Vertices.Length(); i++) {
      vec4 v = ObjInstance.Vec4Vertices.At(i);
      fprintf(stdout, "[OBJ] V: %.6f %.6f %.6f | W: %.2f\n", v.x, v.y, v.z, v.w);
    }
  }

  vulkan_base Base = VulkanInit();
  r_render Renderer;
  R_RenderInit(&Renderer, &Base, &Allocator);

  //VkDescriptorType ObjLoadDescriptorTypes[] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
  //VkDescriptorSetLayout Layout3D = R_CreateDescriptorSetLayout(&Renderer, 1, ObjLoadDescriptorTypes, VK_SHADER_STAGE_VERTEX_BIT);
   
  // Create Pipelines
  R_Handle Obj3D_Pipeline;
  {
    pipeline_builder Obj3dBuilder = InitPipelineBuilder(2, &Allocator);
    SetInputTopology(&Obj3dBuilder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    EnableDepthTest(&Obj3dBuilder);
    SetPolygonMode(&Obj3dBuilder, VK_POLYGON_MODE_FILL);
    SetCullMode(&Obj3dBuilder, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    SetMultisamplingNone(&Obj3dBuilder);
    EnableBlendingAlphaBlend(&Obj3dBuilder);
    SetDepthFormat(&Obj3dBuilder, VK_FORMAT_D32_SFLOAT);
    SetColorAttachmentFormat(&Obj3dBuilder, VK_FORMAT_R32G32B32A32_SFLOAT);

    /// Vertex 3D used right now
    ///
    /// vec3 Position;
    /// vec2 UV;
    /// vec3 Normal;
    /// vec4 Color;
    r_vertex_input_description VertexDescription;
    VertexDescription.Stride = sizeof(v_3d);
    VertexDescription.AttributeCount = 4;
    VertexDescription.Rate = VK_VERTEX_INPUT_RATE_VERTEX;
    VertexDescription.Attributes = stack_push(&Base.TempAllocator, r_vertex_attribute, VertexDescription.AttributeCount);
    VertexDescription.Attributes[0].Location = 0;
    VertexDescription.Attributes[0].Format = R_FORMAT_VEC3;
    VertexDescription.Attributes[0].Offset = 0;
    
    VertexDescription.Attributes[1].Location = 1;
    VertexDescription.Attributes[1].Format = R_FORMAT_VEC2;
    VertexDescription.Attributes[1].Offset = sizeof(v_3d);
    
    VertexDescription.Attributes[2].Location = 2;
    VertexDescription.Attributes[2].Format = R_FORMAT_VEC3;
    VertexDescription.Attributes[2].Offset = sizeof(v_3d) + sizeof(v_2d);
    
    VertexDescription.Attributes[3].Location = 3;
    VertexDescription.Attributes[3].Format = R_FORMAT_VEC4;
    VertexDescription.Attributes[3].Offset = 2*sizeof(v_3d) + sizeof(v_2d);

    Obj3D_Pipeline = R_CreatePipelineFromBuilder(
      &Renderer, 
      "Obj 3D Pipeline", 
      "./code/Samples/shaders/obj_showcase.vert.spv", 
      "./code/Samples/shaders/obj_showcase.frag.spv", 
      &Obj3dBuilder, 
      &VertexDescription, 
      NULL, 
      0
    );

    ClearPipelineBuilder(&Obj3dBuilder);
  }

  R_Handle ObjBuffer;
  R_Handle StagingBuffer;
  /// Vertex buffer
  {
    ObjBuffer     = R_CreateBuffer(&Renderer, "ObjBuffer", ObjInstance.Vec4Vertices.SizeBytes(), R_BUFFER_TYPE_VERTEX);
    StagingBuffer = R_CreateBuffer(&Renderer, "StagingBuffer", ObjInstance.Vec4Vertices.SizeBytes(), R_BUFFER_TYPE_STAGING);

    VkBufferCopy BCopy = {};
    BCopy.srcOffset = 0;
    BCopy.dstOffset = 0;
    BCopy.size = ObjInstance.Vec4Vertices.SizeBytes();

    R_SendDataToBuffer(&Renderer, StagingBuffer, ObjInstance.Vec4Vertices.Data, ObjInstance.Vec4Vertices.SizeBytes(), 0);
    
    VkCommandBuffer ImmCommand = ImmediateSubmitBegin(&Base);
    Renderer.CurrentCommandBuffer = ImmCommand;
    R_CopyStageToBuffer(&Renderer, StagingBuffer, ObjBuffer, BCopy);
    ImmediateSubmitEnd(&Base, ImmCommand);

    Renderer.CurrentCommandBuffer = VK_NULL_HANDLE;
  }

  for (; AppRunning;) {
    ui_input Input = GetNextEvent(&Base.Window);

    if (!(Input & Input_None)) {
      //fprintf(stdout, "[INFO] Input: %ull\n", Input);
    }

    if (Input & Input_Esc || Input & StopUI ) {
      AppRunning = false;
    }

    R_Begin(&Renderer);

    R_ClearScreen(&Renderer, (vec4){0.02, 0.02, 0.02, 1.0f});

    // Render pass
    //
    TransitionImageDefault(
      Renderer.CurrentCommandBuffer,
      Renderer.VulkanBase->Swapchain.Images[Base.SwapchainImageIdx],
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    R_BeginRenderPass(&Renderer);
    {
      //R_BindTexture(&TodoApp.Render, "Fonts Atlas", 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      //R_UpdateUniformBuffer(&TodoApp.Render, "Uniform Buffer", 1, &TodoApp.UniformData, sizeof(ui_uniform));
      //R_BindTexture(&TodoApp.Render, "Icons Atlas", 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      R_BindVertexBuffer(&Renderer, ObjBuffer);
      //R_BindIndexBuffer(&TodoApp.Render, TodoApp.IBuffer[TodoApp.Render.VulkanBase->CurrentFrame]);
      R_SetPipeline(&Renderer, Obj3D_Pipeline);
      R_Draw(&Renderer, ObjInstance.Vec4Vertices.Length(), 1);
    }
    R_RenderPassEnd(&Renderer);

    R_RenderEnd(&Renderer);
  }

  std::chrono::time_point t1 = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration<double, std::milli>(t1 - LastFrame).count();

  printf("Time in s: %.8lf\n", duration / 1000);

  return 0;
}