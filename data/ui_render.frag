#version 450

layout( location = 0 ) in vec4 color;
layout( location = 1 ) in vec2 uv;
layout( location = 2 ) in vec2 in_dst_half_size;
layout( location = 3 ) in vec2 in_dst_center;
layout( location = 4 ) in vec2 in_dst_pos;
layout( location = 5 ) in vec2 in_corner_coord;
layout( location = 6 ) in float in_corner_radius;
layout( location = 7 ) in float in_border_thickess;
layout( location = 8 ) in vec2 in_icon_uv;
layout( location = 9 ) in float in_is_icon;

layout( location = 0 ) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D display_texture;
//layout(set = 0, binding = 2) uniform sampler2D icons_texture;

float RoundedRectSDF(vec2 p, vec2 center, vec2 half_size, float r) {
    vec2 q = abs(p - center) - (half_size - vec2(r));
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main()
{
    if (uv == vec2(-2.0, -2.0) && in_is_icon == 0.f ) {

        float dist = RoundedRectSDF(in_dst_pos, in_dst_center, in_dst_half_size, in_corner_radius);

        // Antialias (usar fwidth del SDF)
        float aa = fwidth(dist);

        float fill_alpha = 1.0 - smoothstep(0.0, aa, dist);

        float border_alpha = 1.0;
        if (in_border_thickess > 0.0)
        {
            float half_t = in_border_thickess * 0.5;

            // valores en el borde interior y exterior
            float d_in  = dist + half_t;
            float d_out = dist - half_t;

            float a_in  = smoothstep(-aa, aa, d_in);
            float a_out = smoothstep(-aa, aa, d_out);

            border_alpha = a_in * (1.0 - a_out);
        }

        out_color = color * (fill_alpha * border_alpha);

		{
			//vec2 q = abs(in_dst_center - in_dst_pos);
			//float f = length(in_dst_half_size + q);
			//out_color.rgb *= f;
		}

	} else if(  in_is_icon == 0.f  ) {
        float text = texture(display_texture, uv).r;
        out_color = color * text;
	} else {
	    //out_color.rgba = vec4(1, 1, 1, 1);
		//vec4 icon = texture(icons_texture, in_icon_uv).rgba;
        //out_color.rgba = out_color.rgba * icon.rgba;
		//if (out_color.a != 0.f) {
		//	out_color.rgb = vec3(0.1, 0.3, 0.5);
		//}
	}
}
