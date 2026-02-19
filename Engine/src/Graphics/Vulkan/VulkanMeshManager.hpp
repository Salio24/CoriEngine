#pragma once
#include "VulkanBuffer.hpp"
#include "VulkanEngine.hpp"
#include "VulkanUploadSubsystem.hpp"
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "AssetManager/AssetLoadStatus.hpp"

namespace Cori {
	namespace Graphics {

		enum class VertexType : uint32_t {
			eStatic
		};

		struct Mesh {
			uint32_t indexCount{ 0 };
			uint32_t firstIndex{ 0 };
			uint64_t firstVertexAddress{ 0 };
			uint32_t vertexType{ 0 };
			uint32_t version{ 0 };
		};

		struct StaticVertex {
			glm::vec3 position;
			uint32_t normal; //10_10_10 packed + 2 bits padding
			uint32_t tangent; //10_10_10 packed tangent + 2 bits - bitangent sign
			glm::vec2 uv;
			uint32_t color; //RGBA8
		};

		class VulkanMeshManager {
			struct VertexStorage {
				VulkanBuffer buffer;
				vma::VirtualBlock block;
			};

			struct QueuedUpload {
				VulkanStreamingLine::BufferUpload vertexUpload;
				VulkanStreamingLine::BufferUpload indexUpload;
				std::vector<Byte> vertexData;
				std::vector<uint32_t> indexData;
				Core::Handle<Mesh> mesh;
				vk::Buffer vertexStorageBuffer;
			};

			struct MeshInTransfer {
				Core::Handle<Mesh> mesh;
				vk::Buffer vertexStorageBuffer;
				uint32_t indexCount{ 0 };
				uint32_t indexOffset{ 0 };
				uint32_t vertexByteSize{ 0 };
				uint32_t vertexByteOffset{ 0 };
			};

			struct InTransferSlot {
				uint64_t ticket{ 0 };
				std::vector<MeshInTransfer> meshesInTransfer;
			};

			struct MeshMetadata {
				vma::VirtualAllocation vertexAllocation;
				vma::VirtualBlock vertexBlock;
				vma::VirtualAllocation indexAllocation;
				VertexType vertexType{};
				AssetStatus assetStatus{ AssetStatus::eUnspecified };
			};

		public:
			[[nodiscard]] static Core::Handle<Mesh> CreateMesh() {
				return Get().CreateMeshImpl();
			}

			template<typename VertexT = StaticVertex> requires std::same_as<VertexT, StaticVertex>
			static void LoadToMesh(const Core::Handle<Mesh> handle, const std::vector<VertexT>& vertices, std::vector<uint32_t>&& indices) {
				Get().LoadToMeshImpl(handle, vertices, std::move(indices));
			}

			static void ProcessUpdates(vk::CommandBuffer cmb) {
				Get().m_BarrierCache.clear();
				auto currentTimelineValue = VulkanStreamingLine::GetTimelineValue();

				for (auto& [ticket, inTransferAssets] : Get().m_MeshesInTransfer) {
					if (currentTimelineValue >= ticket) {
						for (auto& inTransferMesh : inTransferAssets) {

							auto meshRef = Get().m_Meshes[inTransferMesh.mesh];
							meshRef->indexCount = inTransferMesh.indexCount;
							meshRef->firstIndex = inTransferMesh.indexOffset;

							Get().m_BarrierCache.emplace_back(vk::BufferMemoryBarrier2{
								.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
								.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
								.dstStageMask = vk::PipelineStageFlagBits2::eVertexShader,
								.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead,
								.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.buffer = inTransferMesh.vertexStorageBuffer,
								.offset = inTransferMesh.vertexByteOffset,
								.size = inTransferMesh.vertexByteSize
							});

							Get().m_BarrierCache.emplace_back(vk::BufferMemoryBarrier2{
								.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
								.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
								.dstStageMask = vk::PipelineStageFlagBits2::eIndexInput,
								.dstAccessMask = vk::AccessFlagBits2::eIndexRead,
								.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.buffer = Get().m_IndexBuffer.m_Buffer,
								.offset = inTransferMesh.indexOffset * sizeof(uint32_t),
								.size = inTransferMesh.indexCount * sizeof(uint32_t)
							});

							Get().m_MeshMetadata[inTransferMesh.mesh.GetIndex()].assetStatus = AssetStatus::eLoaded;
						}

						inTransferAssets.clear();
					}
				}

				if (!Get().m_BarrierCache.empty()) {
					vk::DependencyInfo depInfo{
						.bufferMemoryBarrierCount = static_cast<uint32_t>(Get().m_BarrierCache.size()),
						.pBufferMemoryBarriers = Get().m_BarrierCache.data()
					};

					cmb.pipelineBarrier2(depInfo);
				}

				while (!Get().m_QueuedUploads.empty()) {
					auto& upload = Get().m_QueuedUploads.front();

					std::array<VulkanStreamingLine::GenericUpload, 2> requests;

					requests[0] = {
						.resourceUpload = upload.vertexUpload,
						.data = upload.vertexData
					};

					requests[1] = {
						.resourceUpload = upload.indexUpload,
						.data = std::span<Byte>(reinterpret_cast<Byte*>(upload.indexData.data()), upload.indexData.size() * sizeof(uint32_t))
					};

					auto result = VulkanStreamingLine::SubmitUploads(requests);

					if (result) {
						Get().FindInTransferSlot(result.value()).emplace_back(upload.mesh, upload.vertexStorageBuffer, static_cast<uint32_t>(upload.indexData.size()), static_cast<uint32_t>(upload.indexUpload.range.offset / sizeof(uint32_t)), static_cast<uint32_t>(upload.vertexData.size()), static_cast<uint32_t>(upload.vertexUpload.range.offset));
						Get().m_MeshMetadata[upload.mesh.GetIndex()].assetStatus = AssetStatus::eLoading;
						Get().m_QueuedUploads.pop();
					} else {
						break;
					}
				}

				Get().m_Meshes.Sync();
			}

			static void DestroyMesh(Core::Handle<Mesh> handle) {
				if (!Get().m_Meshes.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Handle passed to DestroyMesh is invalid, aborting.");
					return;
				}

				if (handle == Get().m_PlaceholderMesh) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Handle passed to DestroyMesh is a placeholder mesh, you can't destroy it, aborting.");
					return;
				}

				auto& meta = Get().m_MeshMetadata[handle.GetIndex()];

				if (meta.assetStatus == AssetStatus::eLoading || meta.assetStatus == AssetStatus::eLoadQueued) {
					CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Handle passed to DestroyMesh points to the mesh that is currently loading, can't destroy a not yet loaded mesh.");
					return;
				}

				if (meta.assetStatus != AssetStatus::eLoadFailed) {
					DeletionQueue::PushVirtualAlloc(meta.vertexAllocation, meta.vertexBlock, VulkanEngine::GetCurrentFrameInFlight());
					DeletionQueue::PushVirtualAlloc(meta.indexAllocation, Get().m_IndexBufferBlock, VulkanEngine::GetCurrentFrameInFlight());
				}

				meta = {};

				Get().m_Meshes.Remove(handle);
			}

			static void Init();

			static void Shutdown();

			static VulkanMeshManager& Get();

			[[nodiscard]] static VulkanBuffer& GetIndexBuffer() {
				return Get().m_IndexBuffer;
			}

			[[nodiscard]] static uint64_t GetMeshAssetBufferBDA() {
				return Get().m_Meshes.GetVulkanBuffer().GetBDA();
			}

			static bool IsHandleValid(const Core::Handle<Mesh> handle) {
				return Get().m_Meshes.IsHandleValid(handle);
			}

			~VulkanMeshManager() {
				DeletionQueue::PushVirtualBlock(m_IndexBufferBlock, VulkanEngine::GetCurrentFrameInFlight());

				for (auto& vertexStorage : m_VertexStorages) {
					DeletionQueue::PushBuffer(vertexStorage.buffer, VulkanEngine::GetCurrentFrameInFlight());
					DeletionQueue::PushVirtualBlock(vertexStorage.block, VulkanEngine::GetCurrentFrameInFlight());
				}

				DeletionQueue::PushBuffer(m_IndexBuffer, VulkanEngine::GetCurrentFrameInFlight());
			}

		private:
			VulkanMeshManager() {
				auto& sharingSettings = VulkanEngine::GetBufferSharingSettings(BUFFER_USAGE);

				vk::BufferCreateInfo vkIndexBufferInfo {
					.size = INDEX_STORAGE_SIZE,
					.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
					.sharingMode = sharingSettings.first,
					.queueFamilyIndexCount = static_cast<uint32_t>(sharingSettings.second.size()),
					.pQueueFamilyIndices = sharingSettings.second.data()
				};

				vma::AllocationCreateInfo allocCreateInfo {
					.flags = vma::AllocationCreateFlagBits::eDedicatedMemory,
					.usage = vma::MemoryUsage::eAuto
				};

				VulkanBuffer::CreateInfo indexBufferInfo {
					.bufferCreateInfo = &vkIndexBufferInfo,
					.allocationCreateInfo = &allocCreateInfo,
					.name = "MeshManager index buffer"
				};

				m_IndexBuffer = VulkanBuffer::Create(indexBufferInfo);

				vma::VirtualBlockCreateInfo indexBufferBlockInfo {
					.size = INDEX_STORAGE_SIZE,
				};

				auto [result_, indexBufferBlock] = vma::createVirtualBlock(indexBufferBlockInfo);

				CORI_CORE_ASSERT(result_ == vk::Result::eSuccess, "Failed to create vma index buffer virtual block. Error: {}", vk::to_string(result_));

				m_IndexBufferBlock = indexBufferBlock;

				m_Meshes.Reserve(512);
				m_MeshMetadata.resize(512);

				m_PlaceholderMesh = CreateMeshImpl();
				std::vector<uint32_t> indices = m_PlaceholderIndexData;
				LoadToMeshImpl(m_PlaceholderMesh, m_PlaceholderVertexData, std::move(indices));
			}

			[[nodiscard]] Core::Handle<Mesh> CreateMeshImpl() {
				auto handle = m_Meshes.Emplace();
				m_MeshMetadata.resize(m_Meshes.Capacity());
				m_MeshMetadata[handle.GetIndex()].assetStatus = AssetStatus::eEmpty;
				return handle;
			}

			template<typename VertexT = StaticVertex> requires std::same_as<VertexT, StaticVertex>
			void LoadToMeshImpl(const Core::Handle<Mesh> handle, const std::vector<VertexT>& vertices, std::vector<uint32_t>&& indices) {
				constexpr VertexType vertexType = []{
					if constexpr (std::is_same_v<VertexT, StaticVertex>) {
						return VertexType::eStatic;
					}
				}();

				if (m_MeshMetadata[handle.GetIndex()].assetStatus != AssetStatus::eEmpty) {
					//TODO: maybe add ana ability to change the data of an already loaded mesh, since there is no limitation connected to QFOT like in texture case, since the buffers are concurrent. can be useful for LOD streaming
					return;
				}

				vma::VirtualAllocationCreateInfo indicesAllocInfo {
					.size = indices.size() * sizeof(uint32_t),
					.alignment = alignof(uint32_t),
					.flags = vma::VirtualAllocationCreateFlagBits::eStrategyMinMemory
				};

				vma::VirtualAllocationCreateInfo verticesAllocInfo {
					.size = vertices.size() * sizeof(VertexT),
					.alignment = alignof(VertexT),
					.flags = vma::VirtualAllocationCreateFlagBits::eStrategyMinMemory
				};

				vk::DeviceSize indexOffset;
				auto [result, indexAlloc] = m_IndexBufferBlock.virtualAllocate(indicesAllocInfo, indexOffset);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "VulkanMeshManager failed to allocate memory for new indices, error: {}", vk::to_string(result));

				vk::DeviceSize vertexOffset;
				vma::VirtualAllocation vertexAlloc;
				VertexStorage* vertexStorage = nullptr;

				for (auto& storage : m_VertexStorages) {
					auto [result_, alloc] = storage.block.virtualAllocate(verticesAllocInfo, vertexOffset);
					if (result_ == vk::Result::eSuccess) {
						vertexAlloc = alloc;
						vertexStorage = &storage;
						break;
					}
				}

				if (!vertexStorage) {
					auto& sharingSettings = VulkanEngine::GetBufferSharingSettings(BUFFER_USAGE);
					vk::BufferCreateInfo vkBufferInfo{
						.size = VERTEX_STORAGE_SIZE,
						.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
						.sharingMode = sharingSettings.first,
						.queueFamilyIndexCount = static_cast<uint32_t>(sharingSettings.second.size()),
						.pQueueFamilyIndices = sharingSettings.second.data()
					};

					vma::AllocationCreateInfo bufferAllocInfo{
						.flags = vma::AllocationCreateFlagBits::eDedicatedMemory,
						.usage = vma::MemoryUsage::eAuto
					};

					VulkanBuffer::CreateInfo createInfo {
						.bufferCreateInfo = &vkBufferInfo,
						.allocationCreateInfo = &bufferAllocInfo
					};

					#ifdef DEBUG_BUILD
					std::string name = std::format("Mesh Manager vertex storage buffer {}", vertexStorageBufferCounter++);
					createInfo.name = name.c_str();
					#endif

					vma::VirtualBlockCreateInfo vertexBlockCreateInfo{
						.size = VERTEX_STORAGE_SIZE
					};

					auto [result1, vertexBlock] = vma::createVirtualBlock(vertexBlockCreateInfo);
					CORI_CORE_ASSERT(result1 == vk::Result::eSuccess, "Failed to create vma vertex storage virtual block. Error: {}", vk::to_string(result1));

					auto& storage = m_VertexStorages.emplace_back(VulkanBuffer::Create(createInfo), vertexBlock);
					vertexStorage = &storage;

					auto [result2, alloc] = storage.block.virtualAllocate(verticesAllocInfo, vertexOffset);
					CORI_CORE_ASSERT(result2 == vk::Result::eSuccess, "VulkanMeshManager failed to allocate memory for new vertices in a newly created vertex storage, error: {}", vk::to_string(result2));
					vertexAlloc = alloc;
				}

				VulkanStreamingLine::BufferUpload vertexUpload{
					.resource = vertexStorage->buffer,
					.range = {.offset = vertexOffset, .alignment = alignof(VertexT) },
					.srcPipelineStages = vk::PipelineStageFlagBits2::eTopOfPipe,
					.srcAccessFlags = vk::AccessFlagBits2::eNone
				};

				VulkanStreamingLine::BufferUpload indexUpload{
					.resource = m_IndexBuffer,
					.range = {.offset = indexOffset, .alignment = alignof(VertexT) },
					.srcPipelineStages = vk::PipelineStageFlagBits2::eTopOfPipe,
					.srcAccessFlags = vk::AccessFlagBits2::eNone
				};

				std::array<VulkanStreamingLine::GenericUpload, 2> uploads;

				uploads[0] = {
					.resourceUpload = vertexUpload,
					.data = std::span<const Byte>(reinterpret_cast<const Byte*>(vertices.data()), vertices.size() * sizeof(VertexT))
				};

				uploads[1] = {
					.resourceUpload = indexUpload,
					.data = std::span<const Byte>(reinterpret_cast<const Byte*>(indices.data()), indices.size() * sizeof(uint32_t))
				};

				auto streamingResult = VulkanStreamingLine::SubmitUploads(uploads);

				if (streamingResult) {
					Mesh mesh{
						.indexCount = 0,
						.firstIndex = 0,
						.firstVertexAddress = vertexStorage->buffer.GetBDA() + vertexOffset,
						.vertexType = std::to_underlying(vertexType)
					};

					auto dataRef = m_Meshes[handle];
					dataRef = mesh;

					auto& meta = m_MeshMetadata[handle.GetIndex()];

					meta.vertexAllocation = vertexAlloc;
					meta.vertexBlock = vertexStorage->block;
					meta.indexAllocation = indexAlloc;
					meta.vertexType = vertexType;
					meta.assetStatus = AssetStatus::eLoading;

					FindInTransferSlot(streamingResult.value()).emplace_back(handle, vertexStorage->buffer.m_Buffer, static_cast<uint32_t>(indices.size()), static_cast<uint32_t>(indexOffset / sizeof(uint32_t)), static_cast<uint32_t>(vertices.size() * sizeof(VertexT)), static_cast<uint32_t>(vertexOffset));

					return;
				}

				if (streamingResult.error() == ErrorCode::eInvalidData) {
					vertexStorage->block.free(vertexAlloc);
					m_IndexBufferBlock.free(indexAlloc);
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "VulkanStreamingLine returned with error code eInvalidData during CreateMesh call, using placeholder.");

					auto& placeholderData = std::as_const(m_Meshes)[m_PlaceholderMesh];
					auto dataRef = m_Meshes[handle];
					dataRef = placeholderData;

					m_MeshMetadata[handle.GetIndex()].assetStatus = AssetStatus::eLoadFailed;

					return;
				}

				std::vector<Byte> verticesBytes(vertices.size() * sizeof(VertexT));
				memcpy(verticesBytes.data(), vertices.data(), vertices.size() * sizeof(VertexT));

				Mesh mesh{
					.indexCount = 0,
					.firstIndex = 0,
					.firstVertexAddress = vertexStorage->buffer.GetBDA() + vertexOffset,
					.vertexType = std::to_underlying(vertexType)
				};

				auto dataRef = m_Meshes[handle];
				dataRef = mesh;

				auto& meta = m_MeshMetadata[handle.GetIndex()];

				meta.vertexAllocation = vertexAlloc;
				meta.vertexBlock = vertexStorage->block;
				meta.indexAllocation = indexAlloc;
				meta.vertexType = vertexType;
				meta.assetStatus = AssetStatus::eLoadQueued;

				m_QueuedUploads.emplace(vertexUpload, indexUpload, std::move(verticesBytes), std::move(indices), handle, vertexStorage->buffer.m_Buffer);
			}

			std::vector<MeshInTransfer>& FindInTransferSlot(const uint64_t value) {
				InTransferSlot* free = nullptr;
				InTransferSlot* bestPick = nullptr;

				for (auto& pair : m_MeshesInTransfer) {
					if (pair.ticket == value) {
						bestPick = &pair;
						break;
					}

					if (pair.meshesInTransfer.empty()) {
						free = &pair;
					}
				}

				if (bestPick) {
					bestPick->ticket = value;
					return bestPick->meshesInTransfer;
				}

				free->ticket = value;
				return free->meshesInTransfer;
			}

			VulkanFlatSlotMap<Mesh> m_Meshes{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Mesh assets data buffer" };

			std::vector<MeshMetadata> m_MeshMetadata;

			std::vector<VertexStorage> m_VertexStorages;

			vma::VirtualBlock m_IndexBufferBlock;
			VulkanBuffer m_IndexBuffer;

			std::array<InTransferSlot, TRANSFERS_IN_FLIGHT + 1> m_MeshesInTransfer;
			std::queue<QueuedUpload> m_QueuedUploads;

			Core::Handle<Mesh> m_PlaceholderMesh;

			std::vector<vk::BufferMemoryBarrier2> m_BarrierCache;

			uint32_t vertexStorageBufferCounter{ 0 }; //used for giving debug names

			static std::unique_ptr<VulkanMeshManager> s_Instance;

			std::vector<StaticVertex> m_PlaceholderVertexData =
			{
				// +Y
				{{ 1,  1, -1}, 0x0007FC00, 0x000003FF, {0.625f,0.500f}, 0xFFFFFFFF},
				{{-1,  1, -1}, 0x0007FC00, 0x000003FF, {0.875f,0.500f}, 0xFFFFFFFF},
				{{-1,  1,  1}, 0x0007FC00, 0x000003FF, {0.875f,0.750f}, 0xFFFFFFFF},
				{{ 1,  1,  1}, 0x0007FC00, 0x000003FF, {0.625f,0.750f}, 0xFFFFFFFF},

				// +Z
				{{ 1, -1,  1}, 0x1FF00000, 0x000003FF, {0.375f,0.750f}, 0xFFFFFFFF},
				{{ 1,  1,  1}, 0x1FF00000, 0x000003FF, {0.625f,1.000f}, 0xFFFFFFFF},
				{{-1,  1,  1}, 0x1FF00000, 0x000003FF, {0.375f,1.000f}, 0xFFFFFFFF},
				{{-1, -1,  1}, 0x1FF00000, 0x000003FF, {0.375f,0.750f}, 0xFFFFFFFF},

				// -X
				{{-1, -1,  1}, 0x000003FF, 0x0007FC00, {0.375f,0.000f}, 0xFFFFFFFF},
				{{-1,  1,  1}, 0x000003FF, 0x0007FC00, {0.625f,0.000f}, 0xFFFFFFFF},
				{{-1,  1, -1}, 0x000003FF, 0x0007FC00, {0.625f,0.250f}, 0xFFFFFFFF},
				{{-1, -1, -1}, 0x000003FF, 0x0007FC00, {0.375f,0.250f}, 0xFFFFFFFF},

				// -Y
				{{-1, -1, -1}, 0x00000000, 0x000003FF, {0.125f,0.500f}, 0xFFFFFFFF},
				{{ 1, -1, -1}, 0x00000000, 0x000003FF, {0.375f,0.500f}, 0xFFFFFFFF},
				{{ 1, -1,  1}, 0x00000000, 0x000003FF, {0.125f,0.750f}, 0xFFFFFFFF},
				{{-1, -1,  1}, 0x00000000, 0x000003FF, {0.375f,0.750f}, 0xFFFFFFFF},

				// +X
				{{ 1, -1, -1}, 0x000FFC00, 0x000003FF, {0.375f,0.250f}, 0xFFFFFFFF},
				{{ 1,  1, -1}, 0x000FFC00, 0x000003FF, {0.625f,0.250f}, 0xFFFFFFFF},
				{{ 1,  1,  1}, 0x000FFC00, 0x000003FF, {0.625f,0.500f}, 0xFFFFFFFF},
				{{ 1, -1,  1}, 0x000FFC00, 0x000003FF, {0.375f,0.500f}, 0xFFFFFFFF},

				// -Z
				{{-1, -1, -1}, 0x3FF00000, 0x000003FF, {0.625f,0.000f}, 0xFFFFFFFF},
				{{-1,  1, -1}, 0x3FF00000, 0x000003FF, {0.875f,0.000f}, 0xFFFFFFFF},
				{{ 1,  1, -1}, 0x3FF00000, 0x000003FF, {0.875f,0.250f}, 0xFFFFFFFF},
				{{ 1, -1, -1}, 0x3FF00000, 0x000003FF, {0.625f,0.250f}, 0xFFFFFFFF},
			};

			std::vector<uint32_t> m_PlaceholderIndexData =
			{
				0,1,2, 0,2,3,        // +Y
				4,5,6, 4,6,7,        // +Z
				8,9,10, 8,10,11,     // -X
				12,13,14, 12,14,15,  // -Y
				16,17,18, 16,18,19,  // +X
				20,21,22, 20,22,23   // -Z
			};

			static constexpr QueueUsageFlags BUFFER_USAGE{ QueueUsageFlagBits::eGraphics | QueueUsageFlagBits::eTransfer };

			static constexpr uint32_t VERTEX_STORAGE_SIZE{ 1024 * 1024 * 64 }; //64mb

			static constexpr uint32_t INDEX_STORAGE_SIZE{ 1024 * 1024 * 128 }; //128mb
		};
	}
}
