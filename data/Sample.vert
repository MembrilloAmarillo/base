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
} registers;

layout(location = 0) out vec4 color_out;

void main() {
     vec2 vertices[4] =
     {
      {-1, -1},
      {-1,  1},
      { 1, -1},
      { 1,  1},
     };
     vec2 pixel_vert[4] =
     {
      { 0, 1},
      { 0, 0},
      { 1, 1},
      { 1, 0},
     };
     Position position = registers.references.buffers[gl_VertexIndex];
     gl_Position = vec4(position.pos, 0.0, 1.0);
     color_out = position.color;
}
