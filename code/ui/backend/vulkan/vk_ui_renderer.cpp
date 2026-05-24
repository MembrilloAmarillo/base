#include "vk_ui_renderer.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {

static VulkanUiVertex ConvertVertexToClipSpace(const ui::Vertex& v, const ui::Vec2 framebuffer_size) {
    const float inv_w = framebuffer_size.x > 0.0f ? (1.0f / framebuffer_size.x) : 0.0f;
    const float inv_h = framebuffer_size.y > 0.0f ? (1.0f / framebuffer_size.y) : 0.0f;

    const float x = v.pos.x * 2.0f * inv_w - 1.0f;
    const float y = v.pos.y * 2.0f * inv_h - 1.0f;

    const std::uint32_t packed = v.color;
    const float r = static_cast<float>((packed >> 0U) & 0xffU) / 255.0f;
    const float g = static_cast<float>((packed >> 8U) & 0xffU) / 255.0f;
    const float b = static_cast<float>((packed >> 16U) & 0xffU) / 255.0f;
    const float a = static_cast<float>((packed >> 24U) & 0xffU) / 255.0f;

    VulkanUiVertex out{};
    out.pos[0] = x;
    out.pos[1] = y;
    out.color[0] = r;
    out.color[1] = g;
    out.color[2] = b;
    out.color[3] = a;
    out.uv[0] = v.uv.x;
    out.uv[1] = v.uv.y;
    return out;
}

static VkRect2D ClampClipRect(const ui::Rect& clip, const ui::Vec2 fb_size) {
    const float fbw = std::max(0.0f, fb_size.x);
    const float fbh = std::max(0.0f, fb_size.y);
    const float x0 = std::clamp(clip.x, 0.0f, fbw);
    const float y0 = std::clamp(clip.y, 0.0f, fbh);
    const float x1 = std::clamp(clip.x + clip.w, x0, fbw);
    const float y1 = std::clamp(clip.y + clip.h, y0, fbh);

    VkRect2D scissor{};
    scissor.offset.x = static_cast<std::int32_t>(x0);
    scissor.offset.y = static_cast<std::int32_t>(y0);
    scissor.extent.width = static_cast<std::uint32_t>(x1 - x0);
    scissor.extent.height = static_cast<std::uint32_t>(y1 - y0);
    return scissor;
}

}  // namespace

void VulkanUiRenderer::Init(Device& device, VkFormat target_format) {
    m_device = &device;
    m_target_format = target_format;
    m_frame_index = 0;
    m_initialized = true;

    m_vertex_capacity.fill(0);
    m_index_capacity.fill(0);
}

void VulkanUiRenderer::EnsureCapacity(std::size_t frame_index, std::size_t vertex_count, std::size_t index_count) {
    if (vertex_count > m_vertex_capacity[frame_index]) {
        Buffer_Create_Info info{};
        info.size = vertex_count * sizeof(VulkanUiVertex);
        info.usage_category = Buffer_Usage::Dynamic_Vertex;
        info.mapped_permanently = true;
        m_vertex_buffers[frame_index] = Buffer(*m_device, info);
        m_vertex_capacity[frame_index] = vertex_count;
    }

    if (index_count > m_index_capacity[frame_index]) {
        Buffer_Create_Info info{};
        info.size = index_count * sizeof(std::uint32_t);
        info.usage_category = Buffer_Usage::Dynamic_Index;
        info.mapped_permanently = true;
        m_index_buffers[frame_index] = Buffer(*m_device, info);
        m_index_capacity[frame_index] = index_count;
    }
}

void VulkanUiRenderer::Render(VkCommandBuffer cmd, const ui::DrawData& draw_data) {
    if (!m_initialized) {
        throw std::runtime_error("VulkanUiRenderer::Render called before Init");
    }

    m_staging_vertices.clear();
    m_staging_indices.clear();
    m_staging_commands.clear();

    for (const ui::DrawList& list : draw_data.lists) {
        const std::uint32_t base_vertex = static_cast<std::uint32_t>(m_staging_vertices.size());
        const std::uint32_t base_index = static_cast<std::uint32_t>(m_staging_indices.size());

        m_staging_vertices.reserve(m_staging_vertices.size() + list.vertices.size());
        for (const ui::Vertex& v : list.vertices) {
            m_staging_vertices.push_back(ConvertVertexToClipSpace(v, draw_data.framebuffer_size));
        }

        const std::size_t old_index_count = m_staging_indices.size();
        m_staging_indices.resize(old_index_count + list.indices.size());
        for (std::size_t i = 0; i < list.indices.size(); ++i) {
            m_staging_indices[old_index_count + i] = list.indices[i] + base_vertex;
        }

        for (const ui::DrawCmd& draw_cmd : list.commands) {
            ui::DrawCmd merged = draw_cmd;
            merged.first_index = base_index + draw_cmd.first_index;
            m_staging_commands.push_back(merged);
        }
    }

    const std::size_t frame_slot = m_frame_index % Frames_In_Flight;
    EnsureCapacity(frame_slot, m_staging_vertices.size(), m_staging_indices.size());

    if (!m_staging_vertices.empty()) {
        void* mapped = m_vertex_buffers[frame_slot].Get_Mapped_Pointer();
        if (mapped == nullptr) {
            mapped = m_vertex_buffers[frame_slot].Map();
        }
        std::memcpy(mapped, m_staging_vertices.data(), m_staging_vertices.size() * sizeof(VulkanUiVertex));
        vmaFlushAllocation(m_device->Get_Vma_Allocator(), m_vertex_buffers[frame_slot].Get_Allocation(), 0, VK_WHOLE_SIZE);
    }
    if (!m_staging_indices.empty()) {
        void* mapped = m_index_buffers[frame_slot].Get_Mapped_Pointer();
        if (mapped == nullptr) {
            mapped = m_index_buffers[frame_slot].Map();
        }
        std::memcpy(mapped, m_staging_indices.data(), m_staging_indices.size() * sizeof(std::uint32_t));
        vmaFlushAllocation(m_device->Get_Vma_Allocator(), m_index_buffers[frame_slot].Get_Allocation(), 0, VK_WHOLE_SIZE);
    }

    if (!m_staging_vertices.empty() && !m_staging_indices.empty() && !m_staging_commands.empty()) {
        VkBuffer vb = m_vertex_buffers[frame_slot].Get_Handle();
        VkDeviceSize vb_offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &vb_offset);
        vkCmdBindIndexBuffer(cmd, m_index_buffers[frame_slot].Get_Handle(), 0, VK_INDEX_TYPE_UINT32);

        VkRect2D full_scissor{};
        full_scissor.offset = {0, 0};
        full_scissor.extent.width = static_cast<std::uint32_t>(std::max(0.0f, draw_data.framebuffer_size.x));
        full_scissor.extent.height = static_cast<std::uint32_t>(std::max(0.0f, draw_data.framebuffer_size.y));
        vkCmdSetScissor(cmd, 0, 1, &full_scissor);

        for (const ui::DrawCmd& draw_cmd : m_staging_commands) {
            if (draw_cmd.index_count == 0) {
                continue;
            }
            vkCmdDrawIndexed(cmd, draw_cmd.index_count, 1, draw_cmd.first_index, 0, 0);
        }
    }

    m_frame_index += 1;
}

void VulkanUiRenderer::Shutdown() {
    m_staging_vertices.clear();
    m_staging_indices.clear();
    m_device = nullptr;
    m_target_format = VK_FORMAT_UNDEFINED;
    m_frame_index = 0;
    m_initialized = false;
    m_vertex_capacity.fill(0);
    m_index_capacity.fill(0);
}
