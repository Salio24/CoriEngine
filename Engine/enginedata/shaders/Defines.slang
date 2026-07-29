#pragma once

struct Handle {
	uint32_t index;
	uint32_t version;
};

struct Vertex {
	float3 position;
	uint32_t normal;
	uint32_t tangent;
	float2 uvs;
	uint32_t color;
};

struct ObjectData {
	float4x4 transform;
	float4 uvOffsets;
	Handle material;
	uint32_t ownerBatch;
	uint32_t valid;
};

struct Mesh {
	uint32_t indexCount;
	uint32_t firstIndex;
	uint64_t firstVertexAddress;
	uint32_t vertexType;
	uint32_t version;
};

struct BatchInfo {
	Handle mesh;
	uint32_t ownerGroup;
};

struct TextureAsset {
	uint32_t descriptorIndex;
	uint32_t version;
};

struct MaterialData {
	float4 colorFactor;
	Handle albedoTexture;
	uint32_t albedoSampler;
	uint32_t pad;
};

struct MaterialCombinedData {
	MaterialData customData;
	Handle shaderEffect;
	uint32_t version;
	uint32_t pad1;
	uint32_t pad2;
	uint32_t pad3;
	uint32_t pad4;
	uint32_t pad5;
};

struct ShaderEffectData {
	float4 custom1;
	float4 custom2;
};

typealias CompactedObjectID = uint32_t;

struct UniformData {
	float4x4 model;
	float4x4 view;
	float4x4 proj;
};