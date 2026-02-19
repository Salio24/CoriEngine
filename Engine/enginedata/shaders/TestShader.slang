struct Vertex {
	float3 position;
	uint32_t normal;
	uint32_t tangent;
	float2 uvs;
	uint32_t color;
};

struct Handle {
	uint32_t index;
	uint32_t version;
};

struct Mesh {
	uint32_t indexCount;
	uint32_t firstIndex;
	Vertex* firstVertexAddress;
	uint32_t vertexType;
	uint32_t version;
};

struct TextureAsset {
	uint32_t descriptorIndex;
	uint32_t version;
};

struct MaterialData {
	float4 colorFactor;
	Handle albedoTexture;
	uint32_t albedoSampler;
};

struct MaterialCombinedData {
	MaterialData customData;
	uint32_t shaderEffectID;
	uint32_t version;
};

struct ShaderEffectData {
	float4 custom1;
	float4 custom2;
};

[[vk::binding(0, 0)]]
SamplerState globalSamplers[];

[[vk::binding(1, 0)]]
Texture2D globalTextures[];

struct CompactedObjectID {
	uint32_t id;
};

struct ObjectData {
	float4x4 transform;
	float4 uvOffsets;
	Handle material;
	uint32_t ownerBatch;
	uint32_t valid;
};

struct BatchInfo {
	Handle mesh;
	uint32_t ownerGroup;
};

struct PushConstants {
	ObjectData* objectDataBuffer;
	CompactedObjectID* compactedObjectIDBuffer;
	MaterialCombinedData* materialDataBuffer;
	ShaderEffectData* shaderEffectDataBuffer;
	Mesh* meshAssetDataBuffer;
	TextureAsset* textureAssetTable;
	BatchInfo* batchInfoBuffer;
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
VSOutput vertMain(uint32_t instanceID : SV_VulkanInstanceID, uint32_t baseInstance : SV_StartInstanceLocation, uint32_t vertexID : SV_VertexID) {
    VSOutput output;

    uint32_t objectID = pc.compactedObjectIDBuffer[baseInstance + instanceID].id;
    ObjectData obj = pc.objectDataBuffer[objectID];

    MaterialData material = pc.materialDataBuffer[obj.material.index].customData;
    Mesh mesh = pc.meshAssetDataBuffer[pc.batchInfoBuffer[obj.ownerBatch].mesh.index];

    Vertex v = mesh.firstVertexAddress[vertexID];

    output.pos =  mul(obj.transform, float4(v.position, 1.0));
    output.fragColor = v.color * material.colorFactor;
    output.uv = obj.uvOffsets.xy + v.uvs * (obj.uvOffsets.zw - obj.uvOffsets.xy);
    output.albedoTexture = pc.textureAssetTable[material.albedoTexture.index].descriptorIndex;
    output.albedoSampler = material.albedoSampler;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
	float4 color = vertIn.fragColor * globalTextures[vertIn.albedoTexture].Sample(globalSamplers[vertIn.albedoSampler], vertIn.uv);
	return color;
}