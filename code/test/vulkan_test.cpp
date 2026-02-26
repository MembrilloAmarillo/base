#include <cassert>
#include <chrono>
#include <cstdio>

#include "../memory/memory.h"
#include "../memory/allocator.h"
#include "../vector/DynamicVector.h"
#include "../util/strings.h"

#include "../window/window_creation.h"
#include "../rendering/vk_instance.hpp"
#include "../rendering/vk_device.hpp"
#include "../rendering/vk_shader_module.hpp"
#include "../rendering/vk_buffer.h"
#include "../rendering/vk_descriptor.hpp"
#include "../rendering/vk_image.hpp"
#include "../rendering/vk_pipeline.hpp"

#define MEMORY_IMPL
#include "../memory/memory.h"
#include "../memory/allocator.cpp"
#include "../vector/DynamicVector.cpp"

#define STRINGS_IMPL
#include "../util/strings.h"

#include "../rendering/vk_instance.cpp"
#include "../rendering/vk_device.cpp"
#include "../rendering/vk_shader_module.cpp"
#include "../rendering/vk_buffer.cpp"
#include "../rendering/vk_descriptor.cpp"
#include "../rendering/vk_image.cpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../third-party/stb_image.h"


#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

int main() {
    Arena arena = {};
    Arena* arena_ptr = &arena;
    arena_ptr = ArenaAllocDefault();
    Allocator allocator = Mem_Allocator::Make(arena_ptr);

    auto window = SurfaceCreateWindow(800, 600);
    U32 instance_extension_count = 0;
    char const* const* sdl_exts = SDL_Vulkan_GetInstanceExtensions(&instance_extension_count);
    std::vector<const char*> extensions;
	extensions.reserve(instance_extension_count + 1);
	for (U32 i = 0; i < instance_extension_count; ++i) extensions.push_back(sdl_exts[i]);

	auto required_ext = Instance::Get_Required_Extensions(true);
	for( auto& ext : required_ext ) {
	   extensions.push_back(ext);
    }

    Instance::Create_Info create_info {
        .app_name = "Vulkan Test",
        .extensions = extensions,
        .mem_allocator = &allocator
    };

    Instance instance(create_info);

    Check(SDL_Vulkan_CreateSurface(window.Win, instance.Get_Handle(), nullptr, &window.Surface));

    Device_Create_Info device_info {
        .surface = window.Surface,
        .allocator = &allocator
    };

    Device logical_device(instance, device_info);

    Shader_Module slang_shader(logical_device, "data/simple_3d.hlsl");

    Buffer_Create_Info buffer_ci {
        .size = 2 << 20,
        .usage_category = Buffer_Usage::Vertex
    };

    Buffer v_buffer(logical_device, buffer_ci);

    Buffer_Create_Info idx_buffer_ci {
        .size = 2 << 20,
        .usage_category = Buffer_Usage::Index
    };

    Buffer i_buffer(logical_device, idx_buffer_ci);

    auto descriptor_types     = dyn_vector<Descriptor_Type>::Init(&allocator, 4);
    auto max_descriptor_types = dyn_vector<U32>::Init(&allocator, 4);
    auto attachment_types     = dyn_vector<Attachment_Type>::Init(&allocator, 4);
    auto max_attachment_types = dyn_vector<U32>::Init(&allocator, 4);
    auto shader_stages        = dyn_vector<Shader_Stage_Flags>::Init(&allocator, 4);

    descriptor_types =  std::initializer_list<Descriptor_Type>{
        Descriptor_Type::Storage_Image,
        Descriptor_Type::Sampler,
        Descriptor_Type::Sampled_Image,
        Descriptor_Type::Uniform_Buffer
    };

    attachment_types = std::initializer_list<Attachment_Type>{
        Attachment_Type::General,
        Attachment_Type::Present,
        Attachment_Type::Shader_Read_Only,
        Attachment_Type::Depth_Attachment_Stencil_Read_Only
    };

    max_attachment_types = std::initializer_list<U32>{ 16, 16, 12, 12 };
    max_descriptor_types = std::initializer_list<U32>{  4,  4, 16, 18 };

    shader_stages = std::initializer_list<Shader_Stage_Flags>{
        Shader_Stages::Fragment,
        Shader_Stages::Fragment,
        Shader_Stages::Fragment,
        Shader_Stages::Vertex | Shader_Stages::Fragment
    };


    Descriptor_Create_Info desc_ci {
        .descriptor_types     = descriptor_types,
        .max_descriptor_types = max_descriptor_types,
        .attachment_types     = attachment_types,
        .max_attachment_types = max_attachment_types,
        .shader_stages        = shader_stages,
        .allocator            = &allocator
    };

    Descriptor descriptor(&logical_device, desc_ci);

    Image texture(Image_Create_Info{
        .device = &logical_device,
        .allocator = &allocator,
        .gpu_allocator = logical_device.Get_Vma_Allocator(),
        .usage_category = Image_Usage::Sampled,  // Implicitly adds TRANSFER_DST
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {1024, 1024, 1},
    });

    const char* image_path = "./data/icons/archivo.png";
    int x, y, channels_in_file, desired_channels = 4;
    auto image_data = stbi_load(image_path, &x, &y, &channels_in_file, desired_channels);

    texture.Upload_Data_To_Image(image_data, x, y, channels_in_file);


    SDL_Vulkan_DestroySurface(instance.Get_Handle(), window.Surface, nullptr);
}