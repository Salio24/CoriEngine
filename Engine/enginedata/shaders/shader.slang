struct Vertex {
	float3 position;
	float  uv_x;
	float3 normal;
	float  uv_y;
	float4 color;
};

struct MeshAssetData {
	uint32_t indexCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
	uint32_t padding;
};

[[vk::binding(0, 0)]]
Sampler2D globalSamplers[];

[[vk::binding(1, 0)]]
Texture2D globalTextures[];

struct PushConstants {
	Vertex* vertexBuffer;
	MeshAssetData* meshDataBuffer;
};

[[vk::push_constant]]
PushConstants pc;

struct VSOutput {
	float4 pos : SV_Position;
	float4 fragColor;
	float2 fragTexCoord;

	nointerpolation uint32_t textureID : TEXCOORD1;
};

[shader("vertex")]
VSOutput vertMain(uint32_t instanceID : SV_InstanceID, uint32_t vertexID : SV_VertexID) {
    VSOutput output;

    Vertex v = pc.vertexBuffer[vertexID];

    output.pos = float4(v.position, 1.0);
    output.fragColor = v.color;
    output.fragTexCoord = float2(v.uv_x, v.uv_y);
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   return vertIn.fragColor;
}