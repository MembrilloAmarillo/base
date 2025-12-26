#version 450

layout(location = 0) in vec4 In_Color;

layout(location = 0) out vec4 Out_Color;


void main() {
  Out_Color = In_Color;
}