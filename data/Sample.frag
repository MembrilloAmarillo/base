#version 450

layout( location = 0 ) in vec4 color;
layout( location = 1 ) in vec2 uv;
layout( location = 2 ) in vec2 in_dst_half_size;
layout( location = 3 ) in vec2 in_dst_center;
layout( location = 4 ) in vec2 in_dst_pos;
layout( location = 5 ) in vec2 in_corner_coord;
layout( location = 6 ) in float in_corner_radius;
layout( location = 7 ) in float in_border_thickess;


layout( location = 0 ) out vec4 out_color;

float RoundedRectSDF(
	vec2 sample_pos,
	vec2 rect_center,
	vec2 rect_half_size,
	float r
) {
 vec2 d2 = (abs(rect_center - sample_pos) - rect_half_size + vec2(r, r));
 return min(max(d2.x, d2.y), 0.0) + length(max(d2, 0.0)) - r;
}

float RectSDF(vec2 sample_pos, vec2 rect_half_size, float r)
{
 return normalize(distance(sample_pos, rect_half_size) - r);
}

void main()
{
	float dist = RoundedRectSDF(in_dst_pos,
						in_dst_center,
						in_dst_half_size,
						in_corner_radius);
	// map distance => a blend factor
	float sdf_factor = 1.f - smoothstep(0, 2, dist);
	float border_factor = 1.f;

	if( in_border_thickess != 0)
	{
	vec2 interior_half_size =
		in_dst_half_size - vec2(in_border_thickess, in_border_thickess);

	float interior_radius_reduce_f =
		min(interior_half_size.x/in_dst_half_size.x,
			interior_half_size.y/in_dst_half_size.y);

	float interior_corner_radius =
	(in_corner_radius *
		interior_radius_reduce_f *
		interior_radius_reduce_f);

	// calculate sample distance from "interior"
	float inside_d = RoundedRectSDF(in_dst_pos,
									in_dst_center,
									interior_half_size,
									interior_corner_radius);

	// map distance => factor
	float inside_f = smoothstep(0, 2, inside_d);
	border_factor = inside_f;
	}

	out_color = color;
	out_color *= sdf_factor * border_factor;

}