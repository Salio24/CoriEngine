#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoords;

out vec2 v_TexCoords;
out vec3 v_Position;
out vec4 v_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_ModelMatrix;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    v_Position = a_Position;
    v_Color = a_Color;
    v_TexCoords = a_TexCoords;
    gl_Position = u_ViewProjection * u_ModelMatrix * vec4(a_Position, 1.0);
}