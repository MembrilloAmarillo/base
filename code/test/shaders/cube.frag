#version 460
layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 color;
layout(location = 0) out vec4 output_color;
void main() {
    vec3 light_dir = normalize(vec3(0.35, 0.65, 0.72));
    float light = max(dot(normalize(normal), light_dir), 0.0) * 0.72 + 0.28;
    output_color = vec4(color * light, 1.0);
}
