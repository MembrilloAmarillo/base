#version 450

layout( location = 0 ) in vec4 color;
layout( location = 1 ) in vec2 uv;

layout( location = 0 ) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D display_texture;

void main()
{
	if( uv == vec2(-1, -1) ) {
		out_color = color;
	} else {
		float text = texture(display_texture, uv).r;
		out_color = color * text;
	    float gamma = 1.8;
	    out_color.rgb = pow(out_color.rgb, vec3(1.0f/gamma));
	}
}