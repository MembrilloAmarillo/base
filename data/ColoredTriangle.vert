#version 450

layout(location = 0) out vec2 outUV;

void main() {
    // Generate fullscreen triangle using only vertex index
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),  // bottom-left
        vec2( 3.0, -1.0),  // bottom-right (off-screen)
        vec2(-1.0,  3.0)   // top-left (off-screen)
    );
    
    vec2 uvs[3] = vec2[](
        vec2(0.0, 0.0),  // bottom-left
        vec2(2.0, 0.0),  // bottom-right
        vec2(0.0, 2.0)   // top-left
    );
    
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    outUV = uvs[gl_VertexIndex];
}
