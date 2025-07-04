#version 460 core

layout(location = 0) out vec4 color;

in vec2 v_TexturePosition;
in vec4 v_TintColor;

uniform sampler2D u_Texture;

void main() {
	vec4 texColor = texture(u_Texture, v_TexturePosition);
	if (texColor.a == 0.0) {
		discard;
	}
	color = texColor * v_TintColor;
}