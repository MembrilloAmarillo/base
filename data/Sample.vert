#version 450

// Per-instance data (ALL NORMALIZED)
layout( location = 0 ) in vec2 in_pos;      // NDC [-1, 1] for the top-left corner
layout( location = 1 ) in vec2 in_size;     // NDC [0, 2] for the width and height
layout( location = 2 ) in vec2 in_uv;
layout( location = 3 ) in vec2 in_uv_size;
layout( location = 4 ) in vec4 in_color;
layout( location = 5 ) in float in_corner_radius;
layout( location = 6 ) in float in_border_width;

// Outputs
layout( location = 0 ) out vec4 out_color;
layout( location = 1 ) out vec2 out_uv;
layout( location = 2 ) out vec2 out_dst_half_size;
layout( location = 3 ) out vec2 out_dst_center;
layout( location = 4 ) out vec2 out_dst_pos;
layout( location = 5 ) out vec2 out_corner_coord;
layout( location = 6 ) out float out_corner_radius;
layout( location = 7 ) out float out_border_width;


layout( binding = 0 ) uniform UniformBuffer {
 float AtlasWidth;
 float AtlasHeight;
 float ScreenWidth;
 float ScreenHeight;
} ubo;

#define GAMMA_TO_LINEAR(Gamma) (pow(Gamma, 2.2))

void main() {
     vec2 vertices[4] =
     {
      {-1, -1},
      {-1,  1},
      { 1, -1},
      { 1,  1},
     };
     vec2 pixel_vert[4] =
     {
      { 0, 1},
      { 0, 0},
      { 1, 1},
      { 1, 0},
     };

    out_corner_coord = pixel_vert[gl_VertexIndex];
    out_border_width = in_border_width;

     vec2 top_left  = in_pos;
     vec2 bot_right = in_pos + in_size;

     vec2 tex_top_left  = in_uv;
     vec2 tex_bot_right = in_uv + in_uv_size;

     vec2 dst_half_size = (bot_right - top_left) / 2;
     vec2 dst_center    = (bot_right + top_left) / 2;
     vec2 dst_pos       = (vertices[gl_VertexIndex] * dst_half_size + dst_center);

     vec2 src_half_size = (tex_bot_right - tex_top_left) / 2;
     vec2 src_center    = (tex_bot_right + tex_top_left) / 2;
     vec2 src_pos       = (vertices[gl_VertexIndex] * src_half_size + src_center);

    gl_Position = vec4( 2 * dst_pos.x / ubo.ScreenWidth  - 1,
            2 * dst_pos.y / ubo.ScreenHeight - 1,
            0,
            1);

    // UVs and Color
    if (in_uv.x == -2.0) {
        out_uv = vec2(-2.0, -2.0);
    } else {
        out_uv = vec2(src_pos.x / ubo.AtlasWidth, src_pos.y / ubo.AtlasHeight);
    }

    out_dst_pos            = dst_pos;
    out_dst_center         = dst_center;
    out_dst_half_size      = dst_half_size;
    out_corner_radius      = in_corner_radius;

    out_color.r = GAMMA_TO_LINEAR(in_color.r);
    out_color.g = GAMMA_TO_LINEAR(in_color.g);
    out_color.b = GAMMA_TO_LINEAR(in_color.b);
    out_color.a = in_color.a;
}