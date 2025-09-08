#version 460 core

layout(location = 0) out vec4 FinalColor;

in vec4 v_Color;
in vec2 v_TexturePosition;
in vec2 v_UnitRange;

uniform sampler2D u_Texture;

float screenPxRange() {
    vec2 screenTexSize = vec2(1.0)/fwidth(v_TexturePosition);
    return max(0.5*dot(v_UnitRange, screenTexSize), 1.0);
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 msd = texture(u_Texture, v_TexturePosition).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange()*(sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);

    FinalColor = vec4(v_Color.rgb, opacity * v_Color.a);
}