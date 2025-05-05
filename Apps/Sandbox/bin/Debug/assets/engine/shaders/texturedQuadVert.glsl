#version 460 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexturePosition;

out vec2 v_TexturePosition;

uniform mat4 u_ViewProjection;
uniform mat4 u_ModelMatrix;

void main() {
    v_TexturePosition = a_TexturePosition;
    gl_Position = u_ViewProjection * u_ModelMatrix * vec4(a_Position, 0.0, 1.0);
}