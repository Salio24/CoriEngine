#include "Defines.slang"

typealias PickResult = Atomic<uint64_t>;

struct PushConstants {
	ObjectData* objectDataBuffer;
	CompactedObjectID* compactedObjectIDBuffer;
	Mesh* meshAssetDataBuffer;
	BatchInfo* batchInfoBuffer;
	UniformData* uniformBuffer;
	PickResult* pickResult;
};

[[vk::push_constant]]
PushConstants pc;

struct VSOutput {
	float4 pos : SV_Position;
	nointerpolation uint32_t entityID;
};

[shader("vertex")]
VSOutput vertMain(uint32_t instanceIndex : SV_VulkanInstanceID, uint32_t vertexID : SV_VertexID) {
	VSOutput output;

	uint32_t objectID = pc.compactedObjectIDBuffer[instanceIndex];
	ObjectData obj = pc.objectDataBuffer[objectID];

	Mesh mesh = pc.meshAssetDataBuffer[pc.batchInfoBuffer[obj.ownerBatch].mesh.index];

	Vertex v = ((Vertex*)mesh.firstVertexAddress)[vertexID];

	output.pos = mul(pc.uniformBuffer.proj, mul(pc.uniformBuffer.view, mul(pc.uniformBuffer.model, mul(obj.transform, float4(v.position, 1.0)))));
	output.entityID = obj.entityID;

	return output;
}

[shader("fragment")]
void fragMain(VSOutput vertIn) {
	uint64_t packed = (uint64_t(asuint(vertIn.pos.z)) << 32) | uint64_t(vertIn.entityID);
	pc.pickResult.max(packed);
}
