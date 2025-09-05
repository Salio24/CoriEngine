#version 460 core

layout(location = 0) out vec4 FinalColor;

in vec4 v_Color;
in vec2 v_TexturePosition;

uniform sampler2D u_Texture;

// FIXME: dont compute screenPxRange for every fragment, instead pass it as a vertex param and comput in on the CPU in the draw func

float screenPxRange() {
    vec2 unitRange = vec2(2.0)/vec2(textureSize(u_Texture, 0));
    vec2 screenTexSize = vec2(1.0)/fwidth(v_TexturePosition);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
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