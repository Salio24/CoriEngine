#version 460 core

layout(location = 0) out vec4 color;

in vec2 v_TexCoords;
in vec4 v_Color;

void main() {
	color = v_Color;
}