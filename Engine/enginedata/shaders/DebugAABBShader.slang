#include "Defines.slang"

struct PushConstants {
	ObjectData* objectDataBuffer;
	BatchInfo* batchInfoBuffer;
	Mesh* meshAssetDataBuffer;
	UniformData* uniformBuffer;
};

[[vk::push_constant]]
PushConstants pc;

static const float3 s_Corners[8] = {
	float3(-1.0, -1.0, -1.0),
	float3( 1.0, -1.0, -1.0),
	float3( 1.0,  1.0, -1.0),
	float3(-1.0,  1.0, -1.0),
	float3(-1.0, -1.0,  1.0),
	float3( 1.0, -1.0,  1.0),
	float3( 1.0,  1.0,  1.0),
	float3(-1.0,  1.0,  1.0)
};

static const uint32_t s_Edges[24] = {
	0, 1, 1, 2, 2, 3, 3, 0,
	4, 5, 5, 6, 6, 7, 7, 4,
	0, 4, 1, 5, 2, 6, 3, 7
};

struct VSOutput {
	float4 pos : SV_Position;
};

[shader("vertex")]
VSOutput vertMain(uint32_t instanceIndex : SV_VulkanInstanceID, uint32_t vertexID : SV_VertexID) {
	VSOutput output;

	ObjectData obj = pc.objectDataBuffer[instanceIndex];

	if (!bool(obj.valid)) {
		output.pos = float4(0.0, 0.0, -1.0, 1.0);
		return output;
	}

	Mesh mesh = pc.meshAssetDataBuffer[pc.batchInfoBuffer[obj.ownerBatch].mesh.index];

	float3 center = float3(mesh.bxCenter, mesh.byCenter, mesh.bzCenter);
	float3 extent = float3(mesh.bxExtent, mesh.byExtent, mesh.bzExtent);

	float3 local = center + s_Corners[s_Edges[vertexID]] * extent;

	output.pos = mul(pc.uniformBuffer.proj, mul(pc.uniformBuffer.view, mul(pc.uniformBuffer.model, mul(obj.transform, float4(local, 1.0)))));

	return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
	return float4(0.2, 1.0, 0.3, 1.0);
}
