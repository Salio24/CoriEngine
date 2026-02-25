#include "Defines.slang"
#include "Helpers.slang"

struct PushConstants {
	ObjectData* objectDataBuffer;
	CompactedObjectID* compactedObjectIDBuffer;
	MaterialCombinedData* materialDataBuffer;
	ShaderEffectData* shaderEffectDataBuffer;
	Mesh* meshAssetDataBuffer;
	TextureAsset* textureAssetTable;
	BatchInfo* batchInfoBuffer;
	UniformData* uniformBuffer;
};

[[vk::push_constant]]
PushConstants pc;

struct VSOutput {
	float4 pos : SV_Position;
};

[shader("vertex")]
VSOutput vertMain(uint32_t instanceID : SV_VulkanInstanceID, uint32_t baseInstance : SV_StartInstanceLocation, uint32_t vertexID : SV_VertexID) {
    VSOutput output;

    uint32_t objectID = pc.compactedObjectIDBuffer[baseInstance + instanceID];
    ObjectData obj = pc.objectDataBuffer[objectID];

    Mesh mesh = pc.meshAssetDataBuffer[pc.batchInfoBuffer[obj.ownerBatch].mesh.index];

    Vertex v = mesh.firstVertexAddress[vertexID];

    output.pos =  mul(pc.uniformBuffer.proj, mul(pc.uniformBuffer.view, mul(pc.uniformBuffer.model, mul(obj.transform, float4(v.position, 1.0)))));

    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
	return pow(float4(1.0, 0.0, 1.0, 1.0), 1 / 2.2);
}