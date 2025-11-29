#pragma once
#include "VulkanUploadManager.hpp"
#include "glm/vec4.hpp"

namespace Cori {
	namespace Graphics {
		using MeshHandle = uint32_t;

		struct MeshAssetInfo {
			uint32_t indexCount{ 0 };
			uint32_t firstIndex{ 0 };
			int32_t vertexOffset{ 0 };
			uint32_t padding{ 0 };
		};

		enum class MeshStatus {
			Initial,
			Loading,
			Failed,
			Loaded,
			Freed
		};

		struct CompleteMeshInfo {
			struct CPUMeshInfo {
				vma::VirtualAllocation vertexAllocation;
				vma::VirtualAllocation indexAllocation;
				MeshStatus status{ MeshStatus::Initial };
			};

			MeshAssetInfo meshAssetInfo;
			CPUMeshInfo cpuMeshInfo;
		};

		struct Vertex {
			glm::vec3 position;
			float uvX;
			glm::vec3 normal;
			float uvY;
			glm::vec4 color;
		};

		class VulkanMeshManager {
		public:
			static MeshHandle CreateMesh(const void* vertexData, const uint64_t vertexDataSize, const void* indexData, const uint64_t indexDataSize) {
				std::vector<Byte> vertices(vertexDataSize);
				std::vector<Byte> indices(indexDataSize);

				memcpy(vertices.data(), vertexData, vertexDataSize);
				memcpy(indices.data(), indexData, indexDataSize);

				return CreateMesh(std::move(vertices), std::move(indices));
			}

			static MeshHandle CreateMesh(std::vector<Byte>&& vertices, std::vector<Byte>&& indices) {
				uint64_t vertexDataSize = vertices.size();
				uint64_t indexDataSize = indices.size();

				MeshHandle freeHandle;

				if (!Get().m_Holes.empty()) {
					freeHandle = Get().m_Holes.back();
					Get().m_Holes.pop_back();
				} else {
					freeHandle = Get().m_NextMeshHandle++;
					CORI_CORE_ASSERT(freeHandle < MESH_ASSET_COUNT_LIMIT - 1, "VulkanMeshManager out of mesh slots.");
				}

				vma::VirtualAllocationCreateInfo verticesAllocInfo {
					.size = vertexDataSize,
					.alignment = sizeof(Vertex)
				};

				vk::DeviceSize vertexOffset;
				auto [result, vertexAlloc] = Get().m_VertexSSBOBlock.virtualAllocate(verticesAllocInfo, vertexOffset);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "VulkanMeshManager failed to allocate memory for new vertices, error: {}", vk::to_string(result));

				vma::VirtualAllocationCreateInfo indicesAllocInfo {
					.size = indexDataSize,
					.alignment = sizeof(uint32_t)
				};

				vk::DeviceSize indexOffset;
				auto [result_, indexAlloc] = Get().m_IndexBufferBlock.virtualAllocate(indicesAllocInfo, indexOffset);
				CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "VulkanMeshManager failed to allocate memory for new indices, error: {}", vk::to_string(result));

				CompleteMeshInfo& meshInfo = Get().m_MeshAssets[freeHandle];
				meshInfo.cpuMeshInfo.status = MeshStatus::Loading;
				meshInfo.cpuMeshInfo.vertexAllocation = vertexAlloc;
				meshInfo.cpuMeshInfo.indexAllocation = indexAlloc;

				meshInfo.meshAssetInfo.indexCount = indexDataSize / sizeof(uint32_t);
				meshInfo.meshAssetInfo.firstIndex = indexOffset / sizeof(uint32_t);
				meshInfo.meshAssetInfo.vertexOffset = static_cast<int32_t>(vertexOffset / sizeof(Vertex));

				VulkanUploadManager::UploadPart vertexPart{
					.resource = Get().m_VertexSSBO,
					.range = VulkanUploadManager::BufferUploadRange{ vertexOffset, sizeof(Vertex) },
					.data = std::move(vertices)
				};

				VulkanUploadManager::UploadPart indexPart{
					.resource = Get().m_IndexBuffer,
					.range = VulkanUploadManager::BufferUploadRange{ indexOffset, sizeof(uint32_t) },
					.data = std::move(indices)
				};

				VulkanUploadManager::UploadRequest request {
					.uploadParts = std::vector<VulkanUploadManager::UploadPart>{ vertexPart, indexPart },
					.callback = VulkanMeshManager::UpdateLoadedMesh,
					.uploadType = VulkanUploadManager::UploadType::Streaming,
					.userData = reinterpret_cast<void*>(static_cast<uint64_t>(freeHandle))
				};

				VulkanUploadManager::SubmitUploadRequest(std::move(request));

				#if 0
				auto& meshAssetBuffer = VulkanUploadManager::GetAmazingBuffer(m_MeshAssetBufferHandle);
				MeshAssetInfo mtData;

				AmazingBuffer::UpdateData patch {
					.offset = freeHandle * sizeof(MeshAssetInfo),
					.alignment = alignof(MeshAssetInfo),
					.data = std::move(std::vector<Byte>(sizeof(MeshAssetInfo))),
					.size = sizeof(MeshAssetInfo),
				};

				memcpy(patch.data.data(), &mtData, sizeof(MeshAssetInfo));

				meshAssetBuffer.SubmitUpdate(std::move(patch));
				#endif

				return freeHandle;
			}

			static void DestroyMesh(MeshHandle handle) {
				CORI_CORE_ASSERT(handle < Get().m_MeshAssets.size(), "Invalid MeshHandle was passed to VulkanMeshManager::DestroyMesh.");
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
				uint32_t prevFrame = (frameIndex + FRAMES_IN_FLIGHT - 1) % FRAMES_IN_FLIGHT;
				Get().m_DestructionQueue[prevFrame].emplace_back(handle);
			}

			static void ProcessDestructionQueue() {
				uint32_t frameIndex = VulkanEngine::GetCurrentFrameInFlight();
				auto& meshAssetBuffer = VulkanUploadManager::GetAmazingBuffer(Get().m_MeshAssetBufferHandle);
				for (auto handle : Get().m_DestructionQueue[frameIndex]) {
					MeshAssetInfo mtData;

					AmazingBuffer::UpdateData patch {
						.offset = handle * sizeof(MeshAssetInfo),
						.alignment = alignof(MeshAssetInfo),
						.data = std::vector<Byte>(sizeof(MeshAssetInfo))
					};

					memcpy(patch.data.data(), &mtData, sizeof(MeshAssetInfo));

					meshAssetBuffer.SubmitUpdate(std::move(patch));

					auto& cpuMesh = Get().m_MeshAssets[handle];
					cpuMesh.cpuMeshInfo.status = MeshStatus::Freed;
					Get().m_VertexSSBOBlock.free(cpuMesh.cpuMeshInfo.vertexAllocation);
					Get().m_IndexBufferBlock.free(cpuMesh.cpuMeshInfo.indexAllocation);
					Get().m_Holes.emplace_back(handle);
				}
			}

			static void Init();

			static void Shutdown();

			static VulkanMeshManager& Get();

			static VulkanBuffer& GetIndexBuffer() {
				return Get().m_IndexBuffer;
			}

			static VulkanBuffer& GetVertexSSBO() {
				return Get().m_VertexSSBO;
			}

			static VulkanBuffer& GetFrameLocal() {
				return VulkanUploadManager::GetAmazingBuffer(Get().m_MeshAssetBufferHandle).GetCurrentFrameLocalBuffer();
			}

			static void UpdateLoadedMesh(void* data) {
				MeshHandle handle = static_cast<MeshHandle>(reinterpret_cast<uint64_t>(data));
				auto& meshAssetBuffer = VulkanUploadManager::GetAmazingBuffer(Get().m_MeshAssetBufferHandle);

				auto& cpuMesh = Get().m_MeshAssets[handle];
				cpuMesh.cpuMeshInfo.status = MeshStatus::Loaded;

				AmazingBuffer::UpdateData patch {
					.offset = handle * sizeof(MeshAssetInfo),
					.alignment = alignof(MeshAssetInfo),
					.data = std::vector<Byte>(sizeof(MeshAssetInfo))
				};

				memcpy(patch.data.data(), &cpuMesh.meshAssetInfo, sizeof(MeshAssetInfo));

				meshAssetBuffer.SubmitUpdate(std::move(patch));
			}

			~VulkanMeshManager() {
				m_VertexSSBOBlock.clearVirtualBlock();
				m_IndexBufferBlock.clearVirtualBlock();

				m_VertexSSBOBlock.destroy();
				m_IndexBufferBlock.destroy();

				m_VertexSSBO.Destroy();
				m_IndexBuffer.Destroy();

				VulkanUploadManager::DestroyAmazingBuffer(m_MeshAssetBufferHandle);
			}

		private:
			VulkanMeshManager() {
				uint32_t graphicsQueueFamilyIndex = VulkanEngine::GetGraphicsQueueFamilyIndex();
				AmazingBuffer::CreateInfo amazingBufferInfo {
					.size = MESH_ASSET_COUNT_LIMIT * sizeof(MeshAssetInfo),
					.createZeroed = true,
					.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
					.queueFamilyIndices = { graphicsQueueFamilyIndex },
					.name = "MeshManager mesh asset buffer"
				};

				uint32_t transferQueueFamilyIndex = VulkanEngine::GetTransferQueueFamilyIndex();

				std::vector<uint32_t> queueFamilyIndices{ VulkanEngine::GetGraphicsQueueFamilyIndex() };

				bool transferQueueInVector = false;
				for (auto familyIndex : queueFamilyIndices) {
					if (familyIndex == transferQueueFamilyIndex) {
						transferQueueInVector = true;
					}
				}

				if (transferQueueInVector && queueFamilyIndices.size() == 1) {
					m_MeshSharingMode = vk::SharingMode::eExclusive;
				} else if (transferQueueInVector && queueFamilyIndices.size() != 1) {
					m_MeshSharingMode = vk::SharingMode::eConcurrent;
					m_QueueFamilyIndices = queueFamilyIndices;
				} else {
					queueFamilyIndices.emplace_back(transferQueueFamilyIndex);
					m_MeshSharingMode = vk::SharingMode::eConcurrent;
					m_QueueFamilyIndices = queueFamilyIndices;
				}

				m_MeshAssetBufferHandle = VulkanUploadManager::CreateAmazingBuffer(amazingBufferInfo);

				vk::BufferCreateInfo vertexSSBOInfo {
					.size = BUFFER_VERTEX_COUNT * sizeof(Vertex),
					.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
					.sharingMode = m_MeshSharingMode,
					.queueFamilyIndexCount = static_cast<uint32_t>(m_QueueFamilyIndices.size()),
					.pQueueFamilyIndices = m_QueueFamilyIndices.data()
				};

				vk::BufferCreateInfo indexBufferInfo {
					.size = static_cast<uint32_t>(BUFFER_VERTEX_COUNT * INDICES_PER_VERTEX * sizeof(uint32_t)),
					.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
					.sharingMode = m_MeshSharingMode,
					.queueFamilyIndexCount = static_cast<uint32_t>(m_QueueFamilyIndices.size()),
					.pQueueFamilyIndices = m_QueueFamilyIndices.data()
				};

				vma::AllocationCreateInfo allocCreateInfo {
					.flags = vma::AllocationCreateFlagBits::eDedicatedMemory,
					.usage = vma::MemoryUsage::eAuto
				};

				VulkanBuffer::CreateInfo vertexSSBOInfo_ {
					.bufferCreateInfo = &vertexSSBOInfo,
					.allocationCreateInfo = &allocCreateInfo,
					.name = "MeshManager vertex SSBO"
				};

				VulkanBuffer::CreateInfo indexBufferInfo_ {
					.bufferCreateInfo = &indexBufferInfo,
					.allocationCreateInfo = &allocCreateInfo,
					.name = "MeshManager index buffer"
				};

				m_VertexSSBO = VulkanBuffer::Create(vertexSSBOInfo_);
				m_IndexBuffer = VulkanBuffer::Create(indexBufferInfo_);

				vma::VirtualBlockCreateInfo vertexSSBOBlockInfo {
					.size = BUFFER_VERTEX_COUNT * sizeof(Vertex),
				};

				vma::VirtualBlockCreateInfo indexBufferBlockInfo {
					.size = static_cast<uint32_t>(BUFFER_VERTEX_COUNT * INDICES_PER_VERTEX * sizeof(uint32_t)),
				};

				auto [result, vertexSSBOBlock] = vma::createVirtualBlock(vertexSSBOBlockInfo);
				auto [result_, indexBufferBlock] = vma::createVirtualBlock(indexBufferBlockInfo);

				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create vma vertex SSBO virtual block. Error: {}", vk::to_string(result));
				CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to create vma index buffer virtual block. Error: {}", vk::to_string(result_));

				m_VertexSSBOBlock = vertexSSBOBlock;
				m_IndexBufferBlock = indexBufferBlock;

				m_MeshAssets.resize(MESH_ASSET_COUNT_LIMIT);
				for (auto& queue : m_DestructionQueue) {
					queue.reserve(64);
				}
			}

			AmazingBufferHandle m_MeshAssetBufferHandle;

			VulkanBuffer m_VertexSSBO;
			VulkanBuffer m_IndexBuffer;

			vma::VirtualBlock m_VertexSSBOBlock;
			vma::VirtualBlock m_IndexBufferBlock;

			std::vector<CompleteMeshInfo> m_MeshAssets;
			std::vector<MeshHandle> m_Holes;

			vk::SharingMode m_MeshSharingMode;
			std::vector<uint32_t> m_QueueFamilyIndices;

			MeshHandle m_NextMeshHandle{ 0 };

			std::array<std::vector<MeshHandle>, FRAMES_IN_FLIGHT> m_DestructionQueue;
			static std::unique_ptr<VulkanMeshManager> s_Instance;

			static constexpr uint32_t BUFFER_VERTEX_COUNT = 1000;
			static constexpr float INDICES_PER_VERTEX = 2;

			static constexpr uint32_t MESH_ASSET_COUNT_LIMIT = 100;
		};
	}
}
