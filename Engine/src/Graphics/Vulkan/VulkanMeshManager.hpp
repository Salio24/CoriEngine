#pragma once
#include "VulkanBuffer.hpp"
#include "VulkanEngine.hpp"
#include "VulkanUploadSubsystem.hpp"
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "AssetManager/AssetLoadStatus.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include <fast_obj.h>

namespace Cori {
	namespace Graphics {

		enum class VertexType : uint32_t {
			eStatic
		};

		class VulkanMeshManager;

		struct Mesh : Core::SecondaryAssetBase {
			using Manager = VulkanMeshManager;
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
				std::variant<std::vector<StaticVertex>> vertexData;
				std::vector<uint32_t> indexData;
				Core::Handle<Mesh> mesh;
				vk::Buffer vertexStorageBuffer;
				uint32_t dataVersion{ 0 };
			};

			struct MeshInTransfer {
				Core::Handle<Mesh> mesh;
				vk::Buffer vertexStorageBuffer;
				vma::VirtualAllocation vertexAllocation;
				vma::VirtualBlock vertexBlock;
				vma::VirtualAllocation indexAllocation;
				uint32_t indexCount{ 0 };
				uint32_t indexOffset{ 0 };
				uint32_t vertexByteSize{ 0 };
				uint32_t vertexByteOffset{ 0 };
				uint32_t dataVersion{ 0 };
			};

			struct InTransferSlot {
				uint64_t ticket{ 0 };
				std::vector<MeshInTransfer> meshesInTransfer;
			};

			struct MeshMetadata {
				vma::VirtualAllocation vertexAllocation;
				vma::VirtualBlock vertexBlock;
				vma::VirtualAllocation indexAllocation;
				Core::AssetID assetID{ 0 };
				Core::AssetDeletionPolicy deletionPolicy{};
				uint32_t dataVersion{ 0 };
				bool placeholderAssigned{ false };
				bool loaded{ false };
			};

		public:
			static void Init();

			static void Shutdown();

			static VulkanMeshManager& Get();
			
			template<typename T> requires std::same_as<Mesh, T>
			static Core::Handle<T> Load(const Core::AssetID id) {
				auto& record = Core::AssetManager2::GetAssetRecord(id);
				const auto& dir = Core::AssetManager2::GetAssetDir();

				auto handle = Get().AllocateHandle();
				Get().m_MeshMetadata[handle.GetIndex()].assetID = id;
				record.rawHandleIndex = handle.GetIndex();
				record.rawHandleVersion = handle.GetVersion();

				auto assetFilePath = dir / record.path;
				std::ifstream file(assetFilePath);
				nlohmann::json json = nlohmann::json::parse(file);
				//FIXME: handle exceptions from json
				if (!json.contains("AssetData")) {
					//error
					Get().AssignPlaceholderMesh(handle);
					return handle;
				}

				auto& data = json["AssetData"];
				if (!(data.contains("Obj"))) {
					//error
					Get().AssignPlaceholderMesh(handle);
					return handle;
				}

				std::filesystem::path objPath = assetFilePath.replace_filename(data["Obj"].get<std::string>());
				std::vector<StaticVertex> vertexData;
				std::vector<uint32_t> indexData;

				LoadObjToEngine(objPath.string().c_str(), vertexData, indexData);

				Get().LoadToMesh(handle, std::move(vertexData), std::move(indexData));
				return handle;
			}

			static void Reload(const Core::Handle<Mesh> handle, const Core::AssetID id) {
				auto& record = Core::AssetManager2::GetAssetRecord(id);
				const auto& dir = Core::AssetManager2::GetAssetDir();

				auto assetFilePath = dir / record.path;
				std::ifstream file(assetFilePath);
				nlohmann::json json = nlohmann::json::parse(file);
				if (!json.contains("AssetData")) {
					//error
					Get().AssignPlaceholderMesh(handle);
				}

				auto& data = json["AssetData"];
				if (!(data.contains("Obj"))) {
					//error
					Get().AssignPlaceholderMesh(handle);
				}

				Get().DestroyMesh(handle);
				auto& meta = Get().m_MeshMetadata[handle.GetIndex()];
				if (id != meta.assetID) {
					auto& oldRecord = Core::AssetManager2::GetAssetRecord(meta.assetID);
					oldRecord.status = AssetStatus::eUnloaded;
					oldRecord.rawHandleIndex = UINT32_MAX;
					oldRecord.rawHandleVersion = 0;
				}

				record.rawHandleIndex = handle.GetIndex();
				record.rawHandleVersion = handle.GetVersion();

				std::filesystem::path objPath = assetFilePath.replace_filename(data["Obj"].get<std::string>());
				std::vector<StaticVertex> vertexData;
				std::vector<uint32_t> indexData;


				LoadObjToEngine(objPath.string().c_str(), vertexData, indexData);

				Get().LoadToMesh(handle, std::move(vertexData), std::move(indexData));
			}

			static void Unload(const Core::Handle<Mesh> handle) {
				if (!Get().m_Meshes.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Handle passed to Unload is invalid, aborting.");
					return;
				}

				auto& record = Core::AssetManager2::GetAssetRecord(GetAssetID(handle));
				record.status = AssetStatus::eUnloaded;
				record.rawHandleIndex = UINT32_MAX;
				record.rawHandleVersion = 0;

				Get().DestroyMesh(handle);
				Get().FreeHandle(handle);
			}

			static Core::AssetID GetAssetID(const Core::Handle<Mesh> handle) {
				if (!Get().m_Meshes.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Handle passed to GetAssetID is invalid, returned AssetID = 0.");
					return 0;
				}

				return Get().m_MeshMetadata[handle.GetIndex()].assetID;
			}

			template<typename T> requires std::same_as<Mesh, T>
			static Core::Handle<T> GetPlaceholder() {
				return Get().GetPlaceholderMesh();
			}

			static void AddRef(Core::Handle<Mesh> handle) {
				if (!Get().m_Meshes.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Handle passed to AddRef is invalid, aborting.");
					return;
				}

				Get().m_MeshRefCounts[handle.GetIndex()]++;

			}

			static void RemoveRef(Core::Handle<Mesh> handle) {
				if (!Get().m_Meshes.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Handle passed to RemoveRef is invalid, aborting.");
					return;
				}

				auto count = --Get().m_MeshRefCounts[handle.GetIndex()];
				auto& meta = Get().m_MeshMetadata[handle.GetIndex()];
				if (count == 0 && meta.deletionPolicy == Core::AssetDeletionPolicy::eRefCounted && handle != Get().m_PlaceholderMesh) {
					auto& record = Core::AssetManager2::GetAssetRecord(GetAssetID(handle));
					record.status = AssetStatus::eUnloaded;
					record.rawHandleIndex = UINT32_MAX;
					record.rawHandleVersion = 0;

					Get().DestroyMesh(handle);
					Get().FreeHandle(handle);
				}
			}

			static void ProcessUpdates(vk::CommandBuffer cmb) {
				Get().m_BarrierCache.clear();
				auto currentTimelineValue = VulkanStreamingLine::GetTimelineValue();

				for (auto& [ticket, inTransferAssets] : Get().m_MeshesInTransfer) {
					if (currentTimelineValue >= ticket) {
						for (auto& inTransferMesh : inTransferAssets) {
							if (!Get().m_Meshes.IsHandleValid(inTransferMesh.mesh)) {
								DeletionQueue::PushVirtualAlloc(inTransferMesh.vertexAllocation, inTransferMesh.vertexBlock, VulkanEngine::GetCurrentFrameInFlight());
								DeletionQueue::PushVirtualAlloc(inTransferMesh.indexAllocation, Get().m_IndexBufferBlock, VulkanEngine::GetCurrentFrameInFlight());
								continue;
							}

							auto& meta = Get().m_MeshMetadata[inTransferMesh.mesh.GetIndex()];
							if (meta.dataVersion != inTransferMesh.dataVersion) {
								DeletionQueue::PushVirtualAlloc(inTransferMesh.vertexAllocation, inTransferMesh.vertexBlock, VulkanEngine::GetCurrentFrameInFlight());
								DeletionQueue::PushVirtualAlloc(inTransferMesh.indexAllocation, Get().m_IndexBufferBlock, VulkanEngine::GetCurrentFrameInFlight());
								continue;
							}

							auto meshData = std::as_const(Get().m_Meshes)[inTransferMesh.mesh];
							meshData.indexCount = inTransferMesh.indexCount;
							meshData.firstIndex = inTransferMesh.indexOffset;

							auto meshRef = Get().m_Meshes[inTransferMesh.mesh];
							meshRef = meshData;

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

							meta.loaded = true;
							Core::AssetManager2::GetAssetRecord(GetAssetID(inTransferMesh.mesh)).status = AssetStatus::eLoaded;
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
					if (!Get().m_Meshes.IsHandleValid(upload.mesh)) {
						Get().m_QueuedUploads.pop();
					}

					auto& meta = Get().m_MeshMetadata[upload.mesh.GetIndex()];

					if (upload.dataVersion != meta.dataVersion) {
						Get().m_QueuedUploads.pop();
					}

					std::array<VulkanStreamingLine::GenericUpload, 2> requests;

					requests[0] = {
						.resourceUpload = upload.vertexUpload,
					};

					uint32_t vertexDataSize = 0;
					if (std::holds_alternative<std::vector<StaticVertex>>(upload.vertexData)) {
						requests[0].data = std::span<Byte>(reinterpret_cast<Byte*>(std::get<std::vector<StaticVertex>>(upload.vertexData).data()), std::get<std::vector<StaticVertex>>(upload.vertexData).size() * sizeof(StaticVertex));
						vertexDataSize = std::get<std::vector<StaticVertex>>(upload.vertexData).size() * sizeof(StaticVertex);
					}

					requests[1] = {
						.resourceUpload = upload.indexUpload,
						.data = std::span<Byte>(reinterpret_cast<Byte*>(upload.indexData.data()), upload.indexData.size() * sizeof(uint32_t))
					};

					auto result = VulkanStreamingLine::SubmitUploads(requests);

					if (result) {
						Get().FindInTransferSlot(result.value()).emplace_back(MeshInTransfer{
							.mesh = upload.mesh,
							.vertexStorageBuffer = upload.vertexStorageBuffer,
							.vertexAllocation = meta.vertexAllocation,
							.vertexBlock = meta.vertexBlock,
							.indexAllocation = meta.indexAllocation,
							.indexCount = static_cast<uint32_t>(upload.indexData.size()),
							.indexOffset = static_cast<uint32_t>(upload.indexUpload.range.offset / sizeof(uint32_t)),
							.vertexByteSize = vertexDataSize,
							.vertexByteOffset = static_cast<uint32_t>(upload.vertexUpload.range.offset),
							.dataVersion = meta.dataVersion,
						});
						Core::AssetManager2::GetAssetRecord(GetAssetID(upload.mesh)).status = AssetStatus::eLoaded;
						Get().m_QueuedUploads.pop();
					} else {
						break;
					}
				}

				Get().m_Meshes.Sync();
			}

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

			static constexpr bool EnableHotReload = true;

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
				m_MeshRefCounts.resize(512);

				m_PlaceholderMesh = AllocateHandle();
				std::vector<uint32_t> indices = m_PlaceholderIndexData;
				std::vector<StaticVertex> vertices = m_PlaceholderVertexData;
				LoadToMesh<StaticVertex, false>(m_PlaceholderMesh, std::move(vertices), std::move(indices));
			}

			[[nodiscard]] Core::Handle<Mesh> GetPlaceholderMesh() const {
				return m_PlaceholderMesh;
			}

			[[nodiscard]] Core::Handle<Mesh> AllocateHandle() {
				auto handle = m_Meshes.Emplace();
				if (handle.GetIndex() >= m_MeshMetadata.size()) {
					m_MeshMetadata.resize(m_MeshMetadata.size() * 1.5f);
				}

				if (handle.GetIndex() >= m_MeshRefCounts.size()) {
					m_MeshRefCounts.resize(m_MeshRefCounts.size() * 1.5f);
				}

				return handle;
			}

			void AssignPlaceholderMesh(const Core::Handle<Mesh> handle) {
				if (!m_Meshes.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Handle passed to AssignPlaceholderMesh is invalid, aborting.");
					return;
				}

				auto placeholderData = std::as_const(m_Meshes)[m_PlaceholderMesh];
				placeholderData.version = handle.GetVersion();
				auto dataRef = m_Meshes[handle];
				dataRef = placeholderData;

				m_MeshMetadata[handle.GetIndex()].placeholderAssigned = true;
			}

			void FreeHandle(const Core::Handle<Mesh> handle) {
				if (!m_Meshes.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Handle passed to FreeHandle is invalid, aborting.");
					return;
				}

				m_MeshMetadata[handle.GetIndex()] = {};
				m_Meshes[handle] = Mesh{};
				m_Meshes.Remove(handle);
			}

			template<typename VertexT = StaticVertex, bool UpdateAssetRecord = true> requires std::same_as<VertexT, StaticVertex>
			void LoadToMesh(const Core::Handle<Mesh> handle, std::vector<VertexT>&& vertices, std::vector<uint32_t>&& indices) {
				constexpr VertexType vertexType = []{
					if constexpr (std::is_same_v<VertexT, StaticVertex>) {
						return VertexType::eStatic;
					}
				}();

				Core::AssetRecord* record = nullptr;

				if constexpr (UpdateAssetRecord) {
					record = &Core::AssetManager2::GetAssetRecord(GetAssetID(handle));
				}

				if constexpr (UpdateAssetRecord) {
					if (record->status != AssetStatus::eUnloaded) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Handle passed to LoadToMesh is pointing to an already loaded mesh, aborting.");
						return;
					}
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
						.vertexType = std::to_underlying(vertexType),
						.version = handle.GetVersion()
					};

					auto dataRef = m_Meshes[handle];
					dataRef = mesh;

					auto& meta = m_MeshMetadata[handle.GetIndex()];

					meta.vertexAllocation = vertexAlloc;
					meta.vertexBlock = vertexStorage->block;
					meta.indexAllocation = indexAlloc;

					if constexpr (UpdateAssetRecord) {
						record->status = AssetStatus::eLoading;
					}

					FindInTransferSlot(streamingResult.value()).emplace_back(MeshInTransfer{
							.mesh = handle,
							.vertexStorageBuffer = vertexStorage->buffer.m_Buffer,
							.vertexAllocation = meta.vertexAllocation,
							.vertexBlock = meta.vertexBlock,
							.indexAllocation = meta.indexAllocation,
							.indexCount = static_cast<uint32_t>(indices.size()),
							.indexOffset = static_cast<uint32_t>(indexOffset / sizeof(uint32_t)),
							.vertexByteSize = static_cast<uint32_t>(vertices.size() * sizeof(VertexT)),
							.vertexByteOffset =  static_cast<uint32_t>(vertexOffset),
							.dataVersion = meta.dataVersion
						});

					return;
				}

				if (streamingResult.error() == ErrorCode::eInvalidData) {
					vertexStorage->block.free(vertexAlloc);
					m_IndexBufferBlock.free(indexAlloc);
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "VulkanStreamingLine returned with error code eInvalidData during CreateMesh call, using placeholder.");

					auto placeholderData = std::as_const(m_Meshes)[m_PlaceholderMesh];
					auto dataRef = m_Meshes[handle];
					dataRef = placeholderData;

					auto& meta = m_MeshMetadata[handle.GetIndex()];
					meta.placeholderAssigned = true;

					if constexpr (UpdateAssetRecord) {
						record->status = AssetStatus::eLoadFailed;
					}

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

				if constexpr (UpdateAssetRecord) {
					record->status = AssetStatus::eLoadQueued;
				}

				m_QueuedUploads.emplace(vertexUpload, indexUpload, std::move(vertices), std::move(indices), handle, vertexStorage->buffer.m_Buffer, meta.dataVersion);
			}

			void DestroyMesh(Core::Handle<Mesh> handle) {
				if (!m_Meshes.IsHandleValid(handle)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Handle passed to DestroyMesh is invalid, aborting.");
					return;
				}

				if (handle == m_PlaceholderMesh) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "Handle passed to DestroyMesh is a placeholder mesh, you can't destroy it, aborting.");
					return;
				}

				auto& meta = m_MeshMetadata[handle.GetIndex()];

				if (!meta.placeholderAssigned && meta.loaded) {
					DeletionQueue::PushVirtualAlloc(meta.vertexAllocation, meta.vertexBlock, VulkanEngine::GetCurrentFrameInFlight());
					DeletionQueue::PushVirtualAlloc(meta.indexAllocation, Get().m_IndexBufferBlock, VulkanEngine::GetCurrentFrameInFlight());
				}

				meta.loaded = false;
				meta.placeholderAssigned = false;
				meta.dataVersion++;

				auto meshData = std::as_const(m_Meshes)[handle];
				meshData.indexCount = 0;
				meshData.firstIndex = 0;
				meshData.firstVertexAddress = 0;
				meshData.vertexType = 0;

				auto dataRef = m_Meshes[handle];
				dataRef = meshData;
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

			static uint32_t PackNormal(const glm::vec3& n) {
				glm::vec3 norm = glm::clamp(n, -1.0f, 1.0f);
				uint32_t x = (static_cast<int32_t>(std::round(norm.x * 511.0f)) & 0x3FF);
				uint32_t y = (static_cast<int32_t>(std::round(norm.y * 511.0f)) & 0x3FF);
				uint32_t z = (static_cast<int32_t>(std::round(norm.z * 511.0f)) & 0x3FF);
				return x | (y << 10) | (z << 20);
			}

			static uint32_t PackTangent(const glm::vec3& t, float bitangentSign) {
				glm::vec3 norm = glm::clamp(t, -1.0f, 1.0f);
				uint32_t x = (static_cast<int32_t>(std::round(norm.x * 511.0f)) & 0x3FF);
				uint32_t y = (static_cast<int32_t>(std::round(norm.y * 511.0f)) & 0x3FF);
				uint32_t z = (static_cast<int32_t>(std::round(norm.z * 511.0f)) & 0x3FF);
				uint32_t w = (bitangentSign < 0.0f) ? 0 : 1;
				return x | (y << 10) | (z << 20) | (w << 30);
			}

			static uint32_t PackColor(const glm::vec3& c) {
				uint32_t r = static_cast<uint32_t>(glm::clamp(c.r, 0.0f, 1.0f) * 255.0f);
				uint32_t g = static_cast<uint32_t>(glm::clamp(c.g, 0.0f, 1.0f) * 255.0f);
				uint32_t b = static_cast<uint32_t>(glm::clamp(c.b, 0.0f, 1.0f) * 255.0f);
				uint32_t a = 255;
				return r | (g << 8) | (b << 16) | (a << 24);
			}

			struct FastObjIndexHash {
				size_t operator()(const fastObjIndex& idx) const {
					uint64_t hash = 0;
					Utility::HashCombine(hash, idx.p);
					Utility::HashCombine(hash, idx.t);
					Utility::HashCombine(hash, idx.n);
					return hash;
				}
			};

			struct FastObjIndexEq {
				bool operator()(const fastObjIndex& a, const fastObjIndex& b) const {
					return a.p == b.p && a.t == b.t && a.n == b.n;
				}
			};

			//temporary and writen by gemini. quick and dirty and does its job, will move to gltf and engine specific asset types once i have an editor
			static bool LoadObjToEngine(const char* filepath, std::vector<StaticVertex>& outVertices, std::vector<uint32_t>& outIndices) {
				fastObjMesh* mesh = fast_obj_read(filepath);
				if (!mesh) {
					std::cerr << "Failed to load obj: " << filepath << std::endl;
					return false;
				}

				std::unordered_map<fastObjIndex, uint32_t, FastObjIndexHash, FastObjIndexEq> uniqueVertices;

				// We store raw/unpacked normals temporarily to compute proper tangents later
				std::vector<glm::vec3> tempNormals;

				size_t index_offset = 0;

				// Phase 1: Vertex deduplication and triangulation
				for (unsigned int i = 0; i < mesh->face_count; ++i) {
					unsigned int num_vertices = mesh->face_vertices[i];

					// Triangulate n-gons (quads, etc.) into triangles using a triangle fan
					for (unsigned int j = 1; j < num_vertices - 1; ++j) {
						fastObjIndex objIndices[3] = {
								mesh->indices[index_offset],
								mesh->indices[index_offset + j],
								mesh->indices[index_offset + j + 1]
							};

						for (int k = 0; k < 3; ++k) {
							const fastObjIndex& idx = objIndices[k];

							if (uniqueVertices.count(idx) == 0) {
								StaticVertex vertex{};
								glm::vec3 normal(0.0f, 1.0f, 0.0f); // Default normal

								// Positions (fast_obj places a dummy at index 0, so valid indices start at 1)
								vertex.position = {
										mesh->positions[idx.p * 3 + 0],
										mesh->positions[idx.p * 3 + 1],
										mesh->positions[idx.p * 3 + 2]
									};

								// Normals
								if (idx.n) {
									normal = {
											mesh->normals[idx.n * 3 + 0],
											mesh->normals[idx.n * 3 + 1],
											mesh->normals[idx.n * 3 + 2]
										};
								}
								vertex.normal = PackNormal(normal);
								tempNormals.push_back(normal); // Save raw normal for tangent math

								// UVs
								if (idx.t) {
									vertex.uv = {
											mesh->texcoords[idx.t * 2 + 0],
											1.0f - mesh->texcoords[idx.t * 2 + 1] // Vulkan/D3D usually flips V
										};
								}
								else {
									vertex.uv = {0.0f, 0.0f};
								}

								// Colors
								glm::vec3 color(1.0f);
								if (mesh->colors && mesh->color_count > idx.p * 3 + 2) {
									color = {
											mesh->colors[idx.p * 3 + 0],
											mesh->colors[idx.p * 3 + 1],
											mesh->colors[idx.p * 3 + 2]
										};
								}
								vertex.color = PackColor(color);
								vertex.tangent = 0; // Filled in Phase 2

								uint32_t newVertexIndex = static_cast<uint32_t>(outVertices.size());
								outVertices.push_back(vertex);
								uniqueVertices[idx] = newVertexIndex;
							}

							outIndices.push_back(uniqueVertices[idx]);
						}
					}
					index_offset += num_vertices;
				}

				// Phase 2: Compute Tangents and Bitangents
				std::vector<glm::vec3> tangents(outVertices.size(), glm::vec3(0.0f));
				std::vector<glm::vec3> bitangents(outVertices.size(), glm::vec3(0.0f));

				for (size_t i = 0; i < outIndices.size(); i += 3) {
					uint32_t i0 = outIndices[i];
					uint32_t i1 = outIndices[i + 1];
					uint32_t i2 = outIndices[i + 2];

					const glm::vec3& p0 = outVertices[i0].position;
					const glm::vec3& p1 = outVertices[i1].position;
					const glm::vec3& p2 = outVertices[i2].position;

					const glm::vec2& uv0 = outVertices[i0].uv;
					const glm::vec2& uv1 = outVertices[i1].uv;
					const glm::vec2& uv2 = outVertices[i2].uv;

					glm::vec3 edge1 = p1 - p0;
					glm::vec3 edge2 = p2 - p0;
					glm::vec2 deltaUV1 = uv1 - uv0;
					glm::vec2 deltaUV2 = uv2 - uv0;

					float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

					// Prevent division by zero on degenerate UV maps
					if (std::isinf(f) || std::isnan(f)) f = 1.0f;

					glm::vec3 tangent(
						f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
						f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
						f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z)
					);

					glm::vec3 bitangent(
						f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x),
						f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y),
						f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z)
					);

					tangents[i0] += tangent;
					tangents[i1] += tangent;
					tangents[i2] += tangent;

					bitangents[i0] += bitangent;
					bitangents[i1] += bitangent;
					bitangents[i2] += bitangent;
				}

				// Phase 3: Orthogonalize Tangents (Gram-Schmidt) and Pack
				for (size_t i = 0; i < outVertices.size(); ++i) {
					const glm::vec3& n = tempNormals[i];
					const glm::vec3& t = tangents[i];
					const glm::vec3& b = bitangents[i];

					// Gram-Schmidt orthogonalize: t = t - (n * dot(n, t))
					glm::vec3 orthoTangent = glm::normalize(t - n * glm::dot(n, t));

					// Calculate bitangent sign mapping (determines handedness of the tangent space)
					float bitangentSign = (glm::dot(glm::cross(n, t), b) < 0.0f) ? -1.0f : 1.0f;

					// Fallback for flat surfaces where tangent might generate zero length
					if (glm::length(orthoTangent) < 0.001f) orthoTangent = glm::vec3(1.0f, 0.0f, 0.0f);

					outVertices[i].tangent = PackTangent(orthoTangent, bitangentSign);
				}

				fast_obj_destroy(mesh);
				return true;
			}

			std::vector<uint32_t> m_MeshRefCounts;
			std::queue<Core::Handle<Mesh>> m_QueuedDeletes;

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
				{{ 1,  1, -1}, 0x0007FC00, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
				{{-1,  1, -1}, 0x0007FC00, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
				{{-1,  1,  1}, 0x0007FC00, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
				{{ 1,  1,  1}, 0x0007FC00, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},

				// +Z
				{{ 1, -1,  1}, 0x1FF00000, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
				{{ 1,  1,  1}, 0x1FF00000, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
				{{-1,  1,  1}, 0x1FF00000, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
				{{-1, -1,  1}, 0x1FF00000, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},

				// -X
				{{-1, -1,  1}, 0x000003FF, 0x0007FC00, {1.0f, 0.0f}, 0xFFFFFFFF},
				{{-1,  1,  1}, 0x000003FF, 0x0007FC00, {1.0f, 1.0f}, 0xFFFFFFFF},
				{{-1,  1, -1}, 0x000003FF, 0x0007FC00, {0.0f, 1.0f}, 0xFFFFFFFF},
				{{-1, -1, -1}, 0x000003FF, 0x0007FC00, {0.0f, 0.0f}, 0xFFFFFFFF},

				// -Y
				{{-1, -1, -1}, 0x00000000, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
				{{ 1, -1, -1}, 0x00000000, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
				{{ 1, -1,  1}, 0x00000000, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
				{{-1, -1,  1}, 0x00000000, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},

				// +X
				{{ 1, -1, -1}, 0x000FFC00, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
				{{ 1,  1, -1}, 0x000FFC00, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
				{{ 1,  1,  1}, 0x000FFC00, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
				{{ 1, -1,  1}, 0x000FFC00, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},

				// -Z
				{{-1, -1, -1}, 0x3FF00000, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
				{{-1,  1, -1}, 0x3FF00000, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
				{{ 1,  1, -1}, 0x3FF00000, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
				{{ 1, -1, -1}, 0x3FF00000, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},
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

	namespace Core {
		CORI_ADD_ASSET_TRAITS(Mesh, Graphics);
	}
}
