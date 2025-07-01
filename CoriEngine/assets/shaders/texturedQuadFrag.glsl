#version 460 core

layout(location = 0) out vec4 color;

in vec2 v_TexturePosition;

uniform sampler2D u_Texture;

void main() {
	vec4 color_ = texture(u_Texture, v_TexturePosition);
	if (color_.a == 0.0) {
		discard;
	}
	color = color_;
}