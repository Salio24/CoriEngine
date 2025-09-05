#version 460 core

layout(location = 0) in mat3 a_Transform;
layout(location = 3) in vec4 a_TexturePosition;
layout(location = 4) in vec4 a_CharQuad;
layout(location = 5) in vec4 a_Color;
layout(location = 6) in float a_Layer;

out vec4 v_Color;
out vec2 v_TexturePosition;

uniform mat4 u_ViewProjection;

const vec2 c_CharQuadMask[4] = vec2[](
vec2(0.0, 0.0),
vec2(1.0, 0.0),
vec2(1.0, 1.0),
vec2(0.0, 1.0)
);

const vec2 c_TextureOffsets[4] = vec2[](
vec2(0.0, 0.0),
vec2(1.0, 0.0),
vec2(1.0, 1.0),
vec2(0.0, 1.0)
);

void main() {
    gl_Position = u_ViewProjection * vec4((a_Transform * vec3(mix(a_CharQuad.xy, a_CharQuad.zw, c_CharQuadMask[gl_VertexID]), 1.0f)).xy, a_Layer, 1.0);

    v_TexturePosition = a_TexturePosition.xy + c_TextureOffsets[gl_VertexID] * (a_TexturePosition.zw - a_TexturePosition.xy);
    v_Color = a_Color;
}