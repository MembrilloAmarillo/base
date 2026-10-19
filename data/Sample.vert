#version 450

#extension GL_EXT_buffer_reference : require

struct Position {
    vec2 pos;
    vec2 uv;
    vec4 color;
};

layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer PositionReferences
{
    Position buffers[];
};

layout(std430, push_constant) uniform Registers
{
    PositionReferences references;
    vec2 viewport_size;
} registers;

layout(location = 0) out vec4 color_out;
layout(location = 1) out vec2 uv_out;

void main() {
     Position position = registers.references.buffers[gl_VertexIndex];
     // Pixel coordinates use a top-left origin with Vulkan's positive viewport height.
     vec2 clip_position = position.pos / registers.viewport_size * 2.0 - 1.0;
     gl_Position = vec4(clip_position, 0.0, 1.0);
     color_out = position.color;
     uv_out = position.uv;
}
