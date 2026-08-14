#include "Defines.slang"
#include "Helpers.slang"


struct PushConstants {
	ObjectData* objectDataBuffer;
	Mesh* meshAssetDataBuffer;
	BatchInfo* batchInfoBuffer;
	UniformData* uniformBuffer;

	uint32_t objectIndex;
	uint32_t color;
	float2 offset;
};

[[vk::push_constant]]
PushConstants pc;

static const float2 s_Offsets[9] = {
	float2( 0.0,       0.0),
	float2( 1.0,       0.0),
	float2(-1.0,       0.0),
	float2( 0.0,       1.0),
	float2( 0.0,      -1.0),
	float2( 0.7071,    0.7071),
	float2(-0.7071,    0.7071),
	float2( 0.7071,   -0.7071),
	float2(-0.7071,   -0.7071)
};

struct VSOutput {
	float4 pos : SV_Position;
};

[shader("vertex")]
VSOutput vertMain(uint32_t instanceIndex : SV_VulkanInstanceID, uint32_t vertexID : SV_VertexID) {
	VSOutput output;

	ObjectData obj = pc.objectDataBuffer[pc.objectIndex];

	Mesh mesh = pc.meshAssetDataBuffer[pc.batchInfoBuffer[obj.ownerBatch].mesh.index];

	Vertex v = ((Vertex*)mesh.firstVertexAddress)[vertexID];

	float4 pos = mul(pc.uniformBuffer.proj, mul(pc.uniformBuffer.view, mul(pc.uniformBuffer.model, mul(obj.transform, float4(v.position, 1.0)))));

	pos.xy += s_Offsets[instanceIndex] * pc.offset * pos.w;

	if (instanceIndex == 0) {
		pos.z = pos.w;
	}

	output.pos = pos;

	return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
	return ConvertColor(pc.color);
}
