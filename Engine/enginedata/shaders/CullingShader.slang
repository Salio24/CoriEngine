#include "Defines.slang"

//struct RedirectID {
//	uint32_t objectID;
//};

typealias GroupCommandOffset = uint32_t;

typealias GlobalAtomic = Atomic<uint32_t>;

typealias BatchIntermediateInfo = Atomic<uint32_t>;

struct VkDrawIndexedIndirectCommand2 {
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
	uint32_t firstInstance;
	uint32_t pad1;
	uint32_t pad2;
	uint32_t pad3;
};

struct VkDrawIndexedIndirectCommand {
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
	uint32_t firstInstance;
};

typealias DrawCommandCount = Atomic<uint32_t>;

struct PushConstants {
	ObjectData* objectDataBuffer;
	CompactedObjectID* compactedObjectIDBuffer;
	//RedirectID* redirectBuffer;
	BatchIntermediateInfo* bii;

	VkDrawIndexedIndirectCommand* commandBuffer; // write
	DrawCommandCount* commandCountBuffer; // write
	BatchInfo* batchInfos; // read
	Mesh* meshDataBuffer; // read
	GlobalAtomic* globalAtomic; // write
	GroupCommandOffset* commandOffsets; // read
	CullData* cullData;


	uint32_t totalInstanceCount;
	uint32_t totalBatchCount;
};

[[vk::push_constant]]
PushConstants pc;

[shader("compute")]
[numthreads(64, 1, 1)]
void cullMain(uint3 threadID : SV_DispatchThreadID) {
	uint32_t unorderedObjectID = threadID.x;
	if (unorderedObjectID >= pc.totalInstanceCount) { return; }
	//ObjectData obj = pc.objectDataBuffer[pc.redirectBuffer[threadID.x].objectID];
	ObjectData obj = pc.objectDataBuffer[unorderedObjectID];

	if (bool(obj.valid) && IsVisible(obj)) {
		uint32_t batchID = obj.ownerBatch;
		pc.bii[batchID].add(1);
	}
}

[shader("compute")]
[numthreads(64, 1, 1)]
void cmgMain(uint3 threadID : SV_DispatchThreadID) {
	uint32_t batchID = threadID.x;
	if (batchID >= pc.totalBatchCount) { return; }

	uint32_t visibleCount = pc.bii[batchID].load();

	if (visibleCount > 0) {
		uint32_t startOffset = pc.globalAtomic.add(visibleCount);

		pc.bii[batchID].store(startOffset);

		BatchInfo batchInfo = pc.batchInfos[batchID];

		uint32_t groupID = batchInfo.ownerGroup;

		Mesh mesh = pc.meshDataBuffer[batchInfo.mesh.index];

		uint32_t commandIndex = pc.commandCountBuffer[groupID].add(1);

		VkDrawIndexedIndirectCommand diic;
		diic.indexCount = mesh.indexCount;
		diic.instanceCount = visibleCount;
		diic.firstIndex = mesh.firstIndex;
		diic.vertexOffset = 0;
		diic.firstInstance = startOffset;

		pc.commandBuffer[pc.commandOffsets[groupID] + commandIndex] = diic;
	}
}

[shader("compute")]
[numthreads(64, 1, 1)]
void compactMain(uint3 threadID : SV_DispatchThreadID) {
	uint32_t unorderedObjectID = threadID.x;
	if (unorderedObjectID >= pc.totalInstanceCount) { return; }
	//uint32_t objectID = pc.redirectBuffer[threadID.x].objectID;

	ObjectData obj = pc.objectDataBuffer[unorderedObjectID];

	if (bool(obj.valid) && IsVisible(obj)) {
		uint32_t batchID = obj.ownerBatch;

		uint32_t slot = pc.bii[batchID].add(1);
		if (slot >= pc.totalInstanceCount) { return; }
		pc.compactedObjectIDBuffer[slot] = unorderedObjectID;
	}
}

bool IsVisible(ObjectData object) {
	BatchInfo batch = pc.batchInfos[object.ownerBatch];
    Mesh mesh = pc.meshDataBuffer[batch.mesh.index];

	if (mesh.version != batch.mesh.version) {
    	return true;
    }

	float3 center = float3(mesh.bxCenter, mesh.byCenter, mesh.bzCenter);
    float3 extent = float3(mesh.bxExtent, mesh.byExtent, mesh.bzExtent);

	float3 worldCenter = mul(object.transform, float4(center, 1.0)).xyz;
	float3 worldExtent = mul(abs((float3x3)object.transform), extent);

    for (uint32_t i = 0; i < 6; i++) {
    	float4 plane = pc.cullData.planes[i];

    	float distance = dot(plane.xyz, worldCenter) + plane.w;
    	float radius = dot(abs(plane.xyz), worldExtent);

    	if (distance + radius < 0.0) {
    		return false;
    	}
    }

    return true;

}