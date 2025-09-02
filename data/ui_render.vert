#version 450

layout( location = 0 ) in vec2 pos;
layout( location = 1 ) in vec2 uv;
layout( location = 2 ) in vec4 color;

layout( location = 0 ) out vec4 out_color;
layout( location = 1 ) out vec2 out_uv;

#define GAMMA_TO_LINEAR(Gamma) ((Gamma) < 0.04045 ? (Gamma) / 12.92 : pow(max((Gamma) + 0.055, 0.0) / 1.055, 2.4))

void main() {
	
#if 1 // I would have to do this if format space is UNORM, but I have put it as SRGB
    out_color.r = GAMMA_TO_LINEAR(color.r);
    out_color.g = GAMMA_TO_LINEAR(color.g);
    out_color.b = GAMMA_TO_LINEAR(color.b);
    out_color.a = 1.0 - GAMMA_TO_LINEAR(1.0 - color.a);

    out_color.r = GAMMA_TO_LINEAR(color.r);
    out_color.g = GAMMA_TO_LINEAR(color.g);
    out_color.b = GAMMA_TO_LINEAR(color.b);
    out_color.a = 1.0 - GAMMA_TO_LINEAR(1.0 - color.a);

    out_color.r = GAMMA_TO_LINEAR(color.r);
    out_color.g = GAMMA_TO_LINEAR(color.g);
    out_color.b = GAMMA_TO_LINEAR(color.b);
    out_color.a = 1.0 - GAMMA_TO_LINEAR(1.0 - color.a);

    out_color.r = GAMMA_TO_LINEAR(color.r);
    out_color.g = GAMMA_TO_LINEAR(color.g);
    out_color.b = GAMMA_TO_LINEAR(color.b);
    out_color.a = 1.0 - GAMMA_TO_LINEAR(1.0 - color.a);
#else 
    out_color = color;
#endif

    gl_Position = vec4(pos, 0.0, 1.0);
    out_uv = uv;
}