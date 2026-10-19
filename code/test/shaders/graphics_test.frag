#version 450
layout(push_constant) uniform Push {
    vec4 color;
    float depth;
}
pc;
layout(set = 0, binding = 3) uniform Tint {
    vec4 tint;
}
material;
layout(location = 0) out vec4 out_color;
void main() {
    out_color = pc.color * material.tint;
}
