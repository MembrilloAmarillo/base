#version 450

layout( location = 0 ) in vec4 color;
layout( location = 1 ) in vec2 uv;
layout( location = 2 ) in vec2 in_dst_half_size;
layout( location = 3 ) in vec2 in_dst_center;
layout( location = 4 ) in vec2 in_dst_pos;
layout( location = 5 ) in vec2 in_corner_coord;
layout( location = 6 ) in float in_corner_radius;

layout( location = 0 ) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D display_texture;

float RoundedRectSDF(
	vec2 sample_pos,
	vec2 rect_center,
	vec2 rect_half_size,
	float r
) {
 vec2 d2 = (abs(rect_center - sample_pos) - rect_half_size + vec2(r, r));
 return min(max(d2.x, d2.y), 0.0) + length(max(d2, 0.0)) - r;
}

void main()
{
	if( uv == vec2(-2.0, -2.0) ) {
		float dist = RoundedRectSDF(in_dst_pos,
                              in_dst_center,
                              in_dst_half_size,
                              in_corner_radius);
		// map distance => a blend factor
		float sdf_factor = 1.f - smoothstep(0, 2, dist);
		out_color = color * sdf_factor;
		
	} else {
		float text = texture(display_texture, uv).r;
		out_color = color * text;
	    float gamma = 1.8;
	    out_color.rgb = pow(out_color.rgb, vec3(1.0f/gamma));
	}
}