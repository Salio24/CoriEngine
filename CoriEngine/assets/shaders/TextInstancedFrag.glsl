#version 460 core

layout(location = 0) out vec4 FinalColor;

in vec4 v_TintColor;
in vec2 v_TexturePosition;

uniform sampler2D u_Texture;

void main() {
    vec4 textureColor = texture(u_Texture, v_TexturePosition);

    if (textureColor.a < 0.01) {
        discard;
    }

    FinalColor = textureColor * v_TintColor;
}