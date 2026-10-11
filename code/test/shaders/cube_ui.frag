#version 460
layout(set = 0, binding = 0) uniform sampler2D font_atlas;
layout(location = 0) in vec4 color;
layout(location = 1) in vec2 uv;
layout(location = 0) out vec4 output_color;
void main() {
    float alpha = color.a;
    if (uv.x >= 0.0 && uv.y >= 0.0) alpha *= texture(font_atlas, uv).r;
    if (alpha <= 0.01) discard;
    output_color = vec4(color.rgb, alpha);
}
