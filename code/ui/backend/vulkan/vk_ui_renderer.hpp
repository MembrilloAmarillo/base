#ifndef VK_UI_RENDERER_HPP
#define VK_UI_RENDERER_HPP

#include <array>
#include <cstddef>
#include <vector>

#include "../../../rendering/vk_buffer.h"
#include "../../../rendering/vk_device.hpp"
#include "../../ui_draw.hpp"

struct VulkanUiVertex {
    float pos[2];
    float color[4];
    float uv[2];
};

class VulkanUiRenderer {
public:
    static constexpr std::size_t Frames_In_Flight = 2;

    VulkanUiRenderer() = default;

    void Init(Device& device, VkFormat target_format);
    void Render(VkCommandBuffer cmd, const ui::DrawData& draw_data);
    void Shutdown();

private:
    void EnsureCapacity(std::size_t frame_index, std::size_t vertex_count, std::size_t index_count);

    Device* m_device = nullptr;
    VkFormat m_target_format = VK_FORMAT_UNDEFINED;
    std::size_t m_frame_index = 0;
    bool m_initialized = false;

    std::array<Buffer, Frames_In_Flight> m_vertex_buffers{};
    std::array<Buffer, Frames_In_Flight> m_index_buffers{};
    std::array<std::size_t, Frames_In_Flight> m_vertex_capacity{};
    std::array<std::size_t, Frames_In_Flight> m_index_capacity{};

    std::vector<VulkanUiVertex> m_staging_vertices{};
    std::vector<std::uint32_t> m_staging_indices{};
    std::vector<ui::DrawCmd> m_staging_commands{};
};

#endif  // VK_UI_RENDERER_HPP
