#version 460
layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec3 vertex_normal;
layout(location = 2) in vec3 vertex_color;
layout(push_constant) uniform Push {
    mat4 mvp;
    vec4 tint;
} pc;
layout(location = 0) out vec3 normal;
layout(location = 1) out vec3 color;

void main() {
    gl_Position = pc.mvp * vec4(vertex_position, 1.0);
    normal = vertex_normal;
    color = vertex_color * pc.tint.rgb;
}
