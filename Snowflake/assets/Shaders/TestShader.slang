#include "../../../Engine/enginedata/shaders/Defines.slang"
#include "../../../Engine/enginedata/shaders/Helpers.slang"

[[vk::binding(0, 0)]]
SamplerState globalSamplers[];

[[vk::binding(1, 0)]]
Texture2D globalTextures[];

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
	float4 fragColor;
	float2 uv;

	nointerpolation uint32_t albedoTexture;
	nointerpolation uint32_t albedoSampler;
};

[shader("vertex")]
VSOutput vertMain(uint32_t instanceIndex : SV_VulkanInstanceID, uint32_t vertexID : SV_VertexID) {
    VSOutput output;

    uint32_t objectID = pc.compactedObjectIDBuffer[instanceIndex];
    ObjectData obj = pc.objectDataBuffer[objectID];

    MaterialData material = pc.materialDataBuffer[obj.material.index].customData;
    Mesh mesh = pc.meshAssetDataBuffer[pc.batchInfoBuffer[obj.ownerBatch].mesh.index];

    Vertex v = ((Vertex*)mesh.firstVertexAddress)[vertexID];

    output.pos =  mul(pc.uniformBuffer.proj, mul(pc.uniformBuffer.view, mul(pc.uniformBuffer.model, mul(obj.transform, float4(v.position, 1.0)))));

    output.fragColor = ConvertColor(v.color) * material.colorFactor;
    output.uv = obj.uvOffsets.xy + v.uvs * (obj.uvOffsets.zw - obj.uvOffsets.xy);
    output.albedoTexture = pc.textureAssetTable[material.albedoTexture.index].descriptorIndex;
    output.albedoSampler = material.albedoSampler;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
	float4 color = vertIn.fragColor * globalTextures[NonUniformResourceIndex(vertIn.albedoTexture)].Sample(globalSamplers[NonUniformResourceIndex(vertIn.albedoSampler)], vertIn.uv);
	return pow(color, 1 / 2.2);
}