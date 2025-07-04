#version 460 core

layout(location = 0) in vec2 a_WorldPosition;
layout(location = 1) in vec2 a_LocalPosition;
layout(location = 2) in vec4 a_TexturePosition;
layout(location = 3) in vec2 a_Size;
layout(location = 4) in vec4 a_TintColor;
layout(location = 5) in float a_Layer;
layout(location = 6) in float a_Rotation;

out vec4 v_Color;
out vec2 v_TexCoord;

uniform mat4 u_ViewProjection;

const vec2 c_CornerOffsets[4] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

const vec2 c_LocalTexcoords[4] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

void main() {
    vec2 quadOrigin = a_WorldPosition + a_LocalPosition;
    vec2 unrotatedPos = quadOrigin + (c_LocalTexcoords[gl_VertexID] * a_Size);

    float c = cos(a_Rotation);
    float s = sin(a_Rotation);
    mat2 rotationMatrix = mat2(c, -s, s, c);
    
    vec2 pivot = quadOrigin + a_Size * 0.5;

    gl_Position = u_ViewProjection * vec4(rotationMatrix * (unrotatedPos - pivot) + pivot, a_Layer, 1.0);

    v_TexCoord = a_TexturePosition.xy + c_LocalTexcoords[gl_VertexID] * (a_TexturePosition.zw - a_TexturePosition.xy);
    v_Color = a_TintColor;
}