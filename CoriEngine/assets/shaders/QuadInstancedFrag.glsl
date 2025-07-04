#version 460 core

layout(location = 0) out vec4 FinalColor;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main() {
    vec4 textureColor = texture(u_Texture, v_TexCoord);
    
    if (textureColor.a < 0.1) {
        discard;
    }

    FinalColor = textureColor * v_Color;
}