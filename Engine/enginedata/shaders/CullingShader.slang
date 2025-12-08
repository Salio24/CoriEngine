struct BatchIntermediateInfo {
	Atomic<uint32_t> visibilityCount;
	uint32_t outputOffset;
};

//struct RedirectID {
//	uint32_t objectID;
//};

typealias GroupCommandOffset = uint32_t;

typealias GlobalAtomic = Atomic<uint32_t>;

typealias CompactedObjectID = uint32_t;

struct VkDrawIndexedIndirectCommand {
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
	uint32_t firstInstance;
	uint32_t pad1;
	uint32_t pad2;
	uint32_t pad3;
};

typealias DrawCommandCount = Atomic<uint32_t>;

struct ObjectData {
	float4x4 transform;
	float4 uvOffsets;
	uint32_t materialID;
	uint32_t subBatchID;
	bool valid;
	uint32_t pad4;
};

struct MeshAssetData {
	uint32_t indexCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
	uint32_t padding;
};

struct BatchInfo {
	uint32_t meshID;
	uint32_t ownerGroup;
};

struct PushConstants {
	ObjectData* objectDataBuffer;
	CompactedObjectID* compactedObjectIDBuffer;
	//RedirectID* redirectBuffer;
	BatchIntermediateInfo* bii;

	VkDrawIndexedIndirectCommand* commandBuffer; // write
	DrawCommandCount* commandCountBuffer; // write
	BatchInfo* batchInfos; // read
	MeshAssetData* meshDataBuffer; // read
	GlobalAtomic* globalAtomic; // write
	GroupCommandOffset* commandOffsets; // read


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

	if (obj.valid && IsVisible(obj)) {
		uint32_t batchID = obj.subBatchID;
		pc.bii[batchID].visibilityCount.add(1);
	}
}

[shader("compute")]
[numthreads(64, 1, 1)]
void cmgMain(uint3 threadID : SV_DispatchThreadID) {
	uint32_t batchID = threadID.x;
	if (batchID >= pc.totalBatchCount) { return; }

	uint32_t visibleCount = pc.bii[batchID].visibilityCount.load();

	if (visibleCount > 0) {
		uint32_t startOffset = pc.globalAtomic.add(visibleCount);

		pc.bii[batchID].outputOffset = startOffset;

		BatchInfo batchInfo = pc.batchInfos[batchID];

		uint32_t groupID = batchInfo.ownerGroup;

		MeshAssetData mesh = pc.meshDataBuffer[batchInfo.meshID];

		uint32_t commandIndex = pc.commandCountBuffer[groupID].add(1);

		VkDrawIndexedIndirectCommand diic;
		diic.indexCount = mesh.indexCount;
		diic.instanceCount = visibleCount;
		diic.firstIndex = mesh.firstIndex;
		diic.vertexOffset = mesh.vertexOffset;
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

	if (obj.valid && IsVisible(obj)) {
		uint32_t batchID = obj.subBatchID;

		uint32_t localID = pc.bii[batchID].visibilityCount.sub(1);

		uint32_t finalCompactedID = localID + pc.bii[batchID].outputOffset;
		pc.compactedObjectIDBuffer[finalCompactedID] = unorderedObjectID;
		pc.bii[batchID].outputOffset = 0;
	}
}

bool IsVisible (ObjectData object) {
	return true;
}