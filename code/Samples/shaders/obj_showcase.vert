#version 450

layout(location = 0) in vec3 In_Position;
layout(location = 1) in vec2 In_UV;
layout(location = 2) in vec3 In_Normal;
layout(location = 3) in vec4 In_Color;

layout(location = 0) out vec4 Out_Color;

void main() {

  gl_Position = vec4(In_Position, 1.0f);

  Out_Color = In_Color * vec4(In_UV, 1.f, 1.f);

}