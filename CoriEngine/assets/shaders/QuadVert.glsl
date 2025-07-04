#version 460 core

layout(location = 0) in vec2 a_WorldPosition;
layout(location = 1) in vec2 a_LocalPosition;
layout(location = 2) in vec2 a_TexturePosition;
layout(location = 3) in vec2 a_Size;
layout(location = 4) in vec4 a_TintColor;
layout(location = 5) in float a_Layer;
layout(location = 6) in float a_Rotation;

out vec2 v_TexturePosition;
out vec4 v_TintColor;

uniform mat4 u_ViewProjection;

void main() {
    float c = cos(a_Rotation);
    float s = sin(a_Rotation);
    
    mat2 rotationMatrix = mat2(c, s, -s, c);
    
    vec2 pos = rotationMatrix * (a_LocalPosition * a_Size);

    pos += a_WorldPosition;

    gl_Position = u_ViewProjection * vec4(pos, a_Layer, 1.0);

    v_TintColor = a_TintColor;
    v_TexturePosition = a_TexturePosition;
}