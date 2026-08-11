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

static const float3 kDefaultLightDir = normalize(float3(0.4, 0.6, 0.8));
static const float3 kDefaultLightColor = float3(1.0, 1.0, 1.0);
static const float kDefaultAmbient = 0.15;

float3 UnpackNormal(uint32_t packed) {
	int3 components = int3(uint3(packed, packed >> 10u, packed >> 20u) & 0x3FFu);
	components = (components ^ 0x200) - 0x200;
	return normalize(float3(components) / 511.0);
}

struct VSOutput {
	float4 pos : SV_Position;
	float3 worldPos;
	float2 uv;

	nointerpolation float3 vertexNormal;
	nointerpolation float4 colorFactor;
	nointerpolation uint32_t albedoTexture;
	nointerpolation uint32_t albedoSampler;
	nointerpolation uint32_t shaderEffect;
};

[shader("vertex")]
VSOutput vertMain(uint32_t instanceIndex : SV_VulkanInstanceID, uint32_t vertexID : SV_VertexID) {
	VSOutput output;

	uint32_t objectID = pc.compactedObjectIDBuffer[instanceIndex];
	ObjectData obj = pc.objectDataBuffer[objectID];

	MaterialCombinedData material = pc.materialDataBuffer[obj.material.index];
	Mesh mesh = pc.meshAssetDataBuffer[pc.batchInfoBuffer[obj.ownerBatch].mesh.index];

	Vertex v = ((Vertex*)mesh.firstVertexAddress)[vertexID];

	float4x4 modelMatrix = mul(pc.uniformBuffer.model, obj.transform);
	float4 worldPos = mul(modelMatrix, float4(v.position, 1.0));

	output.pos = mul(pc.uniformBuffer.proj, mul(pc.uniformBuffer.view, worldPos));
	output.worldPos = worldPos.xyz;
	output.uv = obj.uvOffsets.xy + v.uvs * (obj.uvOffsets.zw - obj.uvOffsets.xy);

	// Only disambiguates the sign of the face normal below, so the plain 3x3 is accurate
	// enough here. It never needs to be the real inverse transpose.
	float3x3 rotationScale = float3x3(modelMatrix[0].xyz, modelMatrix[1].xyz, modelMatrix[2].xyz);
	output.vertexNormal = mul(rotationScale, UnpackNormal(v.normal));

	output.colorFactor = material.customData.colorFactor;
	output.albedoTexture = pc.textureAssetTable[material.customData.albedoTexture.index].descriptorIndex;
	output.albedoSampler = material.customData.albedoSampler;
	output.shaderEffect = material.shaderEffect.index;
	return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
	float3 faceNormal = normalize(cross(ddx(vertIn.worldPos), ddy(vertIn.worldPos)));

	faceNormal *= dot(faceNormal, vertIn.vertexNormal) < 0.0 ? -1.0 : 1.0;

	ShaderEffectData effect = pc.shaderEffectDataBuffer[vertIn.shaderEffect];
	float3 lightDir = dot(effect.custom1.xyz, effect.custom1.xyz) > 1e-8 ? normalize(effect.custom1.xyz) : kDefaultLightDir;
	float ambient = effect.custom1.w > 0.0 ? effect.custom1.w : kDefaultAmbient;
	float3 lightColor = any(effect.custom2.rgb > 0.0) ? effect.custom2.rgb : kDefaultLightColor;

	float4 albedo = vertIn.colorFactor * globalTextures[NonUniformResourceIndex(vertIn.albedoTexture)].Sample(globalSamplers[NonUniformResourceIndex(vertIn.albedoSampler)], vertIn.uv);

	float3 lit = albedo.rgb * (lightColor * max(dot(faceNormal, lightDir), 0.0) + ambient);
	return float4(pow(lit, 1.0 / 2.4), albedo.a);
}
