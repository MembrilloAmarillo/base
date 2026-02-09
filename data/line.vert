#version 450

layout( location = 0 ) in vec2 in_pos;      
layout( location = 1 ) in vec2 in_size;    
layout( location = 2 ) in vec4 in_color;
layout( location = 3 ) in float in_border_width;

// Outputs
layout( location = 0 ) out vec4 out_color;
layout( location = 1 ) out vec2 out_dst_top_left;
layout( location = 2 ) out vec2 out_dst_bottom_right;
layout( location = 3 ) out vec2 out_dst_pos;
layout( location = 4 ) out float out_border_width;


layout( binding = 0 ) uniform UniformBuffer {
	float ScreenWidth;
	float ScreenHeight;
} Screen;

#define GAMMA_TO_LINEAR(Gamma) (pow(Gamma, 2.2))

void main() {
	vec2 vertices[4] =
	{
		{-1, -1},
		{-1,  1},
		{ 1, -1},
		{ 1,  1},
	};

    out_border_width = in_border_width;

	vec2 top_left  = in_pos;
	vec2 bot_right = in_pos + in_size;

	vec2 dst_half_size = (bot_right - top_left) / 2;
	vec2 dst_center    = (bot_right + top_left) / 2;
	vec2 dst_pos       = (vertices[gl_VertexIndex] * dst_half_size + dst_center);

    gl_Position = vec4( 2 * dst_pos.x / Screen.ScreenWidth  - 1,
					   2 * dst_pos.y / Screen.ScreenHeight - 1,
					   0,
					   1);
	
    out_dst_pos            = dst_pos;
    out_dst_top_left       = top_left;
    out_dst_bottom_right   = bot_right;

    out_color.r = GAMMA_TO_LINEAR(in_color.r);
    out_color.g = GAMMA_TO_LINEAR(in_color.g);
    out_color.b = GAMMA_TO_LINEAR(in_color.b);
    out_color.a = in_color.a;
}