#pragma once
#include "VulkanBuffer.hpp"
#include "VulkanEngine.hpp"
#include "VulkanUploadSubsystem.hpp"
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "AssetManager/AssetLoadStatus.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "Core/AssetManager/AssetHandleAllocator.hpp"
#include "Graphics/RenderThreadCommandQueue.hpp"
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
			class VertexStorage {
			public:
				VertexStorage() = delete;

				VertexStorage(VulkanBuffer buffer, vma::VirtualBlock virtBlock) : m_Buffer(std::move(buffer)), m_Block(virtBlock) {
					m_Mutex = std::make_shared<std::mutex>();
					AddStrongRef();
					AddWeakRef();
					std::atomic_thread_fence(std::memory_order_release);
				}

				bool DiscoverStrong() {
					uint64_t cur = m_RefCountCombined.load(std::memory_order_relaxed);
					while (Strong(cur) != 0) {
						if (m_RefCountCombined.compare_exchange_weak(cur, cur + s_Strong, std::memory_order_acquire, std::memory_order_relaxed)) {
							return true;
						}
					}

					return false;
				}

				bool DiscoverWeak() {
					uint64_t cur = m_RefCountCombined.load(std::memory_order_relaxed);
					while (Weak(cur) != 0) {
						if (m_RefCountCombined.compare_exchange_weak(cur, cur + s_Weak, std::memory_order_acquire, std::memory_order_relaxed)) {
							return true;
						}
					}

					return false;
				}

				bool DiscoverBoth() {
					uint64_t cur = m_RefCountCombined.load(std::memory_order_relaxed);
					while (!(Strong(cur) == 0 || Weak(cur) == 0)) {
						if (m_RefCountCombined.compare_exchange_weak(cur, cur + s_Strong + s_Weak, std::memory_order_acquire, std::memory_order_relaxed)) {
							return true;
						}
					}

					return false;
				}

				void AddStrongRef() {
					m_RefCountCombined.fetch_add(s_Strong, std::memory_order_relaxed);
				}

				void AddWeakRef() {
					m_RefCountCombined.fetch_add(s_Weak, std::memory_order_relaxed);
				}

				void RemoveStrongRef() {
					m_RefCountCombined.fetch_sub(s_Strong, std::memory_order_release);
				}

				void RemoveWeakRef() {
					m_RefCountCombined.fetch_sub(s_Weak, std::memory_order_release);
				}

				[[nodiscard]] uint32_t GetStrongRefCount() const {
					return Strong(m_RefCountCombined.load(std::memory_order_acquire));
				}

				[[nodiscard]] uint32_t GetWeakRefCount() const {
					return Weak(m_RefCountCombined.load(std::memory_order_acquire));
				}

				VulkanBuffer m_Buffer;
				vma::VirtualBlock m_Block;
				std::shared_ptr<std::mutex> m_Mutex;

			private:
				static uint32_t Strong(const uint64_t value) {
					return static_cast<uint32_t>(value >> 32);
				}

				static uint32_t Weak(const uint64_t value) {
					return static_cast<uint32_t>(value);
				}

				std::atomic<uint64_t> m_RefCountCombined;

				static constexpr uint64_t s_Strong = 1ull << 32;
				static constexpr uint64_t s_Weak   = 1ull;
			};

			struct CompleteVertexAllocation {
				vk::DeviceSize offset{};
				vma::VirtualAllocation allocation;
				VertexStorage* storage{ nullptr };
			};

			struct QueuedUpload {
				VulkanStreamingLine::BufferUpload vertexUpload;
				VulkanStreamingLine::BufferUpload indexUpload;
				std::variant<std::vector<StaticVertex>> vertexData;
				std::vector<uint32_t> indexData;
				Core::Handle<Mesh> mesh;
				VertexStorage* vertexStorage;
				vma::VirtualAllocation indexAlloc;
				vma::VirtualAllocation vertexAlloc;
				uint32_t loadGen{ 0 };
				//vk::Buffer vertexStorageBuffer;
				//std::weak_ptr<std::mutex> vertexBlockMutex;
				//vma::VirtualAllocation vertexAlloc;
				//vma::VirtualBlock vertexBlock;
			};

			struct MeshInTransfer {
				Core::Handle<Mesh> mesh;
				vk::Buffer vertexStorageBuffer;
				vma::VirtualAllocation vertexAllocation;
				VertexStorage* vertexStorage;
				vma::VirtualAllocation indexAllocation;
				uint32_t indexCount{ 0 };
				uint32_t indexOffset{ 0 };
				uint32_t vertexByteSize{ 0 };
				uint32_t vertexByteOffset{ 0 };
				uint32_t loadGen{ 0 };
			};

			struct InTransferSlot {
				uint64_t ticket{ 0 };
				std::vector<MeshInTransfer> meshesInTransfer;
			};

			struct MeshMetadata {
				vma::VirtualAllocation vertexAllocation;
				VertexStorage* vertexStorage{ nullptr };
				//vma::VirtualBlock vertexBlock;
				vma::VirtualAllocation indexAllocation;
				bool placeholderAssigned{ false };
				bool loaded{ false };
			};

			struct JsonAssetData {
				std::string obj;
			};

			struct JsonAssetDataCombined {
				glz::skip Metadata;
				JsonAssetData AssetData;
			};

			class WorkerPayload {
			public:
				WorkerPayload() = delete;
				WorkerPayload(std::variant<std::vector<StaticVertex>>&& vertexData, std::vector<uint32_t>&& indexData, const CompleteVertexAllocation& completeVertexAllocation) : m_VertexData(std::move(vertexData)), m_IndexData(std::move(indexData)), m_CompleteVertexAlloc(completeVertexAllocation) {}

				~WorkerPayload() {
					if (m_CompleteVertexAlloc.allocation && m_CompleteVertexAlloc.storage) {
						{
							std::lock_guard lk(*m_CompleteVertexAlloc.storage->m_Mutex);
							m_CompleteVertexAlloc.storage->m_Block.free(m_CompleteVertexAlloc.allocation);
						}
						m_CompleteVertexAlloc.storage->RemoveStrongRef();
						m_CompleteVertexAlloc.storage->RemoveWeakRef();
						m_CompleteVertexAlloc = {};
					}
				}

				WorkerPayload(const WorkerPayload& other) = delete;
				WorkerPayload& operator=(const WorkerPayload& other) = delete;

				WorkerPayload(WorkerPayload&& other) noexcept {
					m_VertexData = std::move(other.m_VertexData);
					m_IndexData = std::move(other.m_IndexData);
					m_CompleteVertexAlloc = other.m_CompleteVertexAlloc;

					other.Release();
				}

				WorkerPayload& operator=(WorkerPayload&& other) noexcept {
					m_VertexData = std::move(other.m_VertexData);
					m_IndexData = std::move(other.m_IndexData);

					if (m_CompleteVertexAlloc.allocation && m_CompleteVertexAlloc.storage) {
						{
							std::lock_guard lk(*m_CompleteVertexAlloc.storage->m_Mutex);
							m_CompleteVertexAlloc.storage->m_Block.free(m_CompleteVertexAlloc.allocation);
						}
						m_CompleteVertexAlloc.storage->RemoveStrongRef();
						m_CompleteVertexAlloc.storage->RemoveWeakRef();
					}

					m_CompleteVertexAlloc = other.m_CompleteVertexAlloc;

					other.Release();
					return *this;
				}

				void Release() {
					if (m_CompleteVertexAlloc.storage) {
						//m_CompleteVertexAlloc.storage->RemoveStrongRef();
						m_CompleteVertexAlloc = {};
					}
				}

				std::variant<std::vector<StaticVertex>> m_VertexData;
				std::vector<uint32_t> m_IndexData;
				CompleteVertexAllocation m_CompleteVertexAlloc;
			};


		public:
			static void Init();

			static void Shutdown();

			static VulkanMeshManager& Get();

			static void RegisterAtSlot(const Core::Handle<Mesh> handle);

			static void Load(const Core::Handle<Mesh> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name = "");

			static void Unload(const Core::Handle<Mesh> handle) {
				CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", handle.GetIndex(), handle.GetVersion());
				CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] UNLOAD (destroying mesh, freeing handle)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion());
				CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to Unload is invalid.");
				CORI_CORE_ASSERT(handle != Get().m_PlaceholderMesh, "Placeholder mesh handle was passed, can't unload it.")

				Core::AssetID id = Get().m_HandleAllocator.GetBoundAssetID(handle);
				{
					auto& mutex = Core::AssetManager2::GetMutex();
					std::lock_guard lk(mutex);
					CORI_PROFILER_LOCK_MARK(mutex);
					auto& record = Core::AssetManager2::GetAssetRecord(id);
					if (record.rawHandleIndex == handle.GetIndex() && record.rawHandleVersion == handle.GetVersion()) {
						record.rawHandleIndex = UINT32_MAX;
						record.rawHandleVersion = 0;
					}
				}

				Get().m_HandleAllocator.Free(handle);

				if (!Get().m_Meshes.IsIndexOccupied(handle.GetIndex())) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, no mesh slot was occupied");
					return;
				}

				Get().DestroyMesh(handle);
				Get().m_Meshes.RemoveAt(handle.GetIndex());
				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle freed, mesh destroyed and slot released");
			}

			static Core::AssetID GetAssetID(const Core::Handle<Mesh> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Handle passed to GetAssetID in VulkanMeshManager is invalid.");
				return Get().m_HandleAllocator.GetBoundAssetID(handle);
			}

			template<typename T> requires std::same_as<Mesh, T>
			static Core::Handle<T> GetPlaceholder() {
				return Get().m_PlaceholderMesh;
			}

			static uint32_t BumpGeneration(const Core::Handle<Mesh> handle) {
				return Get().m_HandleAllocator.BumpGeneration(handle);
			}

			static void BindAsset(const Core::Handle<Mesh> handle, const Core::AssetID id, const uint32_t vectorKey) {
				Get().m_HandleAllocator.BindAsset(handle, id, vectorKey);
			}


			static bool TryAddRef(const Core::Handle<Mesh> handle) {
				return Get().m_HandleAllocator.TryAddRef(handle);
			}

			static void AddRef(Core::Handle<Mesh> handle) {
				Get().m_HandleAllocator.AddRef(handle);
			}

			static void RemoveRef(Core::Handle<Mesh> handle) {
				Get().m_HandleAllocator.RemoveRef(handle);
			}

			static void QueueUnload(const Core::Handle<Mesh> handle) {
				RenderThreadCommandQueue::Push([handle]{ Unload(handle); });
			}

			static void ProcessUpdates(vk::CommandBuffer cmb) {
				CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Process);
				Get().m_BarrierCache.clear();
				auto currentTimelineValue = VulkanStreamingLine::GetTimelineValue();

				for (auto& [ticket, inTransferAssets] : Get().m_MeshesInTransfer) {
					if (currentTimelineValue >= ticket) {
						for (auto& inTransferMesh : inTransferAssets) {
							CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "ProcessUpdates: Mesh transfer complete", Cori::ProfileColors::Loaded);
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", inTransferMesh.mesh.GetIndex(), inTransferMesh.mesh.GetVersion());
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Ticket=%llu, Current timeline value=%llu", static_cast<unsigned long long>(ticket), static_cast<unsigned long long>(currentTimelineValue));
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Indices=%u @ firstIndex=%u, vertices=%u bytes @ storage=%p +%u", inTransferMesh.indexCount, inTransferMesh.indexOffset, inTransferMesh.vertexByteSize, static_cast<void*>(inTransferMesh.vertexStorage), inTransferMesh.vertexByteOffset);

							if (!IsHandleValid(inTransferMesh.mesh)) {
								CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, freed vertex/index allocations");
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] transfer done but handle invalid, freed vertex/index allocations (ticket %llu)", CORI_CLEAN_TYPE_NAME(Mesh), inTransferMesh.mesh.GetIndex(), inTransferMesh.mesh.GetVersion(), static_cast<unsigned long long>(ticket));
								DeletionQueue::PushVirtualAlloc(inTransferMesh.vertexAllocation, inTransferMesh.vertexStorage->m_Block, inTransferMesh.vertexStorage->m_Mutex);
								DeletionQueue::PushVirtualAlloc(inTransferMesh.indexAllocation, Get().m_IndexBufferBlock);
								inTransferMesh.vertexStorage->RemoveStrongRef();
								inTransferMesh.vertexStorage->RemoveWeakRef();
								continue;
							}

							if (Get().m_HandleAllocator.GetGeneration(inTransferMesh.mesh) != inTransferMesh.loadGen) {
								CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (loadGen=%u, current gen=%u), freed vertex/index allocations", inTransferMesh.loadGen, Get().m_HandleAllocator.GetGeneration(inTransferMesh.mesh));
								CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] transfer STALE (loadGen=%u, current gen=%u) (ticket %llu), freed vertex/index allocations", CORI_CLEAN_TYPE_NAME(Mesh), inTransferMesh.mesh.GetIndex(), inTransferMesh.mesh.GetVersion(), inTransferMesh.loadGen, Get().m_HandleAllocator.GetGeneration(inTransferMesh.mesh), static_cast<unsigned long long>(ticket));
								DeletionQueue::PushVirtualAlloc(inTransferMesh.vertexAllocation, inTransferMesh.vertexStorage->m_Block, inTransferMesh.vertexStorage->m_Mutex);
								DeletionQueue::PushVirtualAlloc(inTransferMesh.indexAllocation, Get().m_IndexBufferBlock);
								inTransferMesh.vertexStorage->RemoveStrongRef();
								inTransferMesh.vertexStorage->RemoveWeakRef();
								continue;
							}

							inTransferMesh.vertexStorage->RemoveStrongRef();

							auto& meshData = Get().m_Meshes[inTransferMesh.mesh];
							meshData.indexCount = inTransferMesh.indexCount;
							meshData.firstIndex = inTransferMesh.indexOffset;

							auto& meta = Get().m_MeshMetadata[inTransferMesh.mesh.GetIndex()];
							meta.loaded = true;
							SetAssetStatus(inTransferMesh.mesh, AssetStatus::eLoaded);
							CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: LOADED, real mesh now drawable (indexCount=%u, firstIndex=%u, firstVertexAddress=0x%llx), storage refs strong=%u weak=%u", meshData.indexCount, meshData.firstIndex, static_cast<unsigned long long>(meshData.firstVertexAddress), inTransferMesh.vertexStorage->GetStrongRefCount(), inTransferMesh.vertexStorage->GetWeakRefCount());
							CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Loaded, "%s Handle=[%u, %u] LOADED, real mesh now drawable (indexCount=%u, firstIndex=%u) (ticket %llu)", CORI_CLEAN_TYPE_NAME(Mesh), inTransferMesh.mesh.GetIndex(), inTransferMesh.mesh.GetVersion(), meshData.indexCount, meshData.firstIndex, static_cast<unsigned long long>(ticket));
						}

						VulkanEngine::AddWaitTimelineSemaphore(VulkanStreamingLine::GetTimelineSemaphoreHandle(), ticket, vk::PipelineStageFlagBits::eVertexShader);

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
					CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "ProcessUpdates: Queued upload retry", Cori::ProfileColors::Upload);
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u]", upload.mesh.GetIndex(), upload.mesh.GetVersion());
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Indices=%llu @ byte offset %llu, vertex storage=%p @ byte offset %llu", static_cast<unsigned long long>(upload.indexData.size()), static_cast<unsigned long long>(upload.indexUpload.range.offset), static_cast<void*>(upload.vertexStorage), static_cast<unsigned long long>(upload.vertexUpload.range.offset));

					if (!IsHandleValid(upload.mesh)) {
						CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: Handle invalid, queued upload dropped and allocations freed");
						CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] queued upload dropped (handle invalid) (gen=%u)", CORI_CLEAN_TYPE_NAME(Mesh), upload.mesh.GetIndex(), upload.mesh.GetVersion(), upload.loadGen);
						{
							std::lock_guard lk(*upload.vertexStorage->m_Mutex);
							upload.vertexStorage->m_Block.free(upload.vertexAlloc);
						}
						Get().m_IndexBufferBlock.free(upload.indexAlloc);
						upload.vertexStorage->RemoveStrongRef();
						upload.vertexStorage->RemoveWeakRef();
						Get().m_QueuedUploads.pop();
						continue;
					}

					if (upload.loadGen != Get().m_HandleAllocator.GetGeneration(upload.mesh)) {
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: STALE (loadGen=%u, current gen=%u), queued upload dropped and allocations freed", upload.loadGen, Get().m_HandleAllocator.GetGeneration(upload.mesh));
						CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eWarning, Cori::ProfileColors::Stale, "%s Handle=[%u, %u] queued upload dropped (STALE loadGen=%u, current gen=%u)", CORI_CLEAN_TYPE_NAME(Mesh), upload.mesh.GetIndex(), upload.mesh.GetVersion(), upload.loadGen, Get().m_HandleAllocator.GetGeneration(upload.mesh));
						{
							std::lock_guard lk(*upload.vertexStorage->m_Mutex);
							upload.vertexStorage->m_Block.free(upload.vertexAlloc);
						}
						Get().m_IndexBufferBlock.free(upload.indexAlloc);
						upload.vertexStorage->RemoveStrongRef();
						upload.vertexStorage->RemoveWeakRef();
						Get().m_QueuedUploads.pop();
						continue;
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

					auto& meta = Get().m_MeshMetadata[upload.mesh.GetIndex()];
					auto result = VulkanStreamingLine::SubmitUploads(requests);

					if (result) {
						Get().FindInTransferSlot(result.value()).emplace_back(MeshInTransfer{
							.mesh = upload.mesh,
							.vertexStorageBuffer = upload.vertexStorage->m_Buffer.m_Buffer,
							.vertexAllocation = upload.vertexAlloc,
							.vertexStorage = upload.vertexStorage,
							.indexAllocation = upload.indexAlloc,
							.indexCount = static_cast<uint32_t>(upload.indexData.size()),
							.indexOffset = static_cast<uint32_t>(upload.indexUpload.range.offset / sizeof(uint32_t)),
							.vertexByteSize = vertexDataSize,
							.vertexByteOffset = static_cast<uint32_t>(upload.vertexUpload.range.offset),
							.loadGen = upload.loadGen,
						});

						SetAssetStatus(upload.mesh, AssetStatus::eStreaming);
						CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: SUBMITTED (ticket=%llu), status=Streaming", static_cast<unsigned long long>(result.value()));
						CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Upload, "%s Handle=[%u, %u] queued upload SUBMITTED (ticket=%llu) (gen=%u), status=Streaming", CORI_CLEAN_TYPE_NAME(Mesh), upload.mesh.GetIndex(), upload.mesh.GetVersion(), static_cast<unsigned long long>(result.value()), upload.loadGen);

						Get().m_QueuedUploads.pop();
					} else {
						CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: streaming line still full, retry deferred to next frame");
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

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<Mesh> handle) {
				return Get().IsHandleValidImpl(handle);
			}

			static void SetAssetStatus(const Core::Handle<Mesh> handle, const AssetStatus newStatus) {
				Get().SetAssetStatusImpl(handle, newStatus);
			}

			template<typename T> requires std::same_as<Mesh, T>
			[[nodiscard]] static Core::Handle<Mesh> AllocateHandle() {
				return Get().m_HandleAllocator.Allocate();
			}

			~VulkanMeshManager() {
				DeletionQueue::PushVirtualBlock(m_IndexBufferBlock);

				for (auto& vertexStorage : m_VertexStorages) {
					DeletionQueue::PushBuffer(vertexStorage.m_Buffer);
					DeletionQueue::PushVirtualBlock(vertexStorage.m_Block);
				}

				DeletionQueue::PushBuffer(m_IndexBuffer);
			}

			static constexpr bool EnableHotReload = true;
			static constexpr bool EnableAutoHotReload = true;

		private:
			void SetAssetStatusImpl(const Core::Handle<Mesh> handle, const AssetStatus newStatus) {
				m_HandleAllocator.SetAssetStatus(handle, newStatus);
			}

			[[nodiscard]] bool IsHandleValidImpl(const Core::Handle<Mesh> handle) const {
				return m_HandleAllocator.IsHandleValid(handle);
			}

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

				m_PlaceholderMesh = m_HandleAllocator.Allocate();
				m_HandleAllocator.AddRef(m_PlaceholderMesh);

				std::vector<StaticVertex> placeholderVertexData{
					// +Y
					{{1, 1, -1}, 0x0007FC00, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
					{{-1, 1, -1}, 0x0007FC00, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
					{{-1, 1, 1}, 0x0007FC00, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
					{{1, 1, 1}, 0x0007FC00, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},

					// +Z
					{{1, -1, 1}, 0x1FF00000, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
					{{1, 1, 1}, 0x1FF00000, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
					{{-1, 1, 1}, 0x1FF00000, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
					{{-1, -1, 1}, 0x1FF00000, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},

					// -X
					{{-1, -1, 1}, 0x000003FF, 0x0007FC00, {1.0f, 0.0f}, 0xFFFFFFFF},
					{{-1, 1, 1}, 0x000003FF, 0x0007FC00, {1.0f, 1.0f}, 0xFFFFFFFF},
					{{-1, 1, -1}, 0x000003FF, 0x0007FC00, {0.0f, 1.0f}, 0xFFFFFFFF},
					{{-1, -1, -1}, 0x000003FF, 0x0007FC00, {0.0f, 0.0f}, 0xFFFFFFFF},

					// -Y
					{{-1, -1, -1}, 0x00000000, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
					{{1, -1, -1}, 0x00000000, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
					{{1, -1, 1}, 0x00000000, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
					{{-1, -1, 1}, 0x00000000, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},

					// +X
					{{1, -1, -1}, 0x000FFC00, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
					{{1, 1, -1}, 0x000FFC00, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
					{{1, 1, 1}, 0x000FFC00, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
					{{1, -1, 1}, 0x000FFC00, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},

					// -Z
					{{-1, -1, -1}, 0x3FF00000, 0x000003FF, {1.0f, 0.0f}, 0xFFFFFFFF},
					{{-1, 1, -1}, 0x3FF00000, 0x000003FF, {1.0f, 1.0f}, 0xFFFFFFFF},
					{{1, 1, -1}, 0x3FF00000, 0x000003FF, {0.0f, 1.0f}, 0xFFFFFFFF},
					{{1, -1, -1}, 0x3FF00000, 0x000003FF, {0.0f, 0.0f}, 0xFFFFFFFF},
				};

				std::vector<uint32_t> placeholderIndexData{
					0, 1, 2, 0, 2, 3, // +Y
					4, 5, 6, 4, 6, 7, // +Z
					8, 9, 10, 8, 10, 11, // -X
					12, 13, 14, 12, 14, 15, // -Y
					16, 17, 18, 16, 18, 19, // +X
					20, 21, 22, 20, 22, 23 // -Z
				};

				m_Meshes.EmplaceAt(m_PlaceholderMesh.GetIndex());

				uint32_t indexCount = placeholderIndexData.size();
				auto [success, indexOffset, ticket] = LoadToMesh<StaticVertex>(m_PlaceholderMesh, std::move(placeholderVertexData), std::move(placeholderIndexData), 0);
				CORI_CORE_ASSERT(success, "Failed to load placeholder mesh.");
				if (!ticket) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "The load of placeholder was rejected by the streaming like due to backpressure, it was queued and will be loaded a bit later. AssignPlaceholder calls will assign an empty mesh in that window.");
				} else {
					VulkanEngine::AddWaitTimelineSemaphore(VulkanStreamingLine::GetTimelineSemaphoreHandle(), ticket.value(), vk::PipelineStageFlagBits::eAllCommands);
					auto& placeholder = m_Meshes[m_PlaceholderMesh];
					placeholder.indexCount = indexCount;
					placeholder.firstIndex = indexOffset;
				}
			}

			void AssignPlaceholder(const Core::Handle<Mesh> handle) {
				CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Missing);
				CORI_CORE_ASSERT(IsHandleValidImpl(handle), "Invalid handle.");

				auto placeholderData = std::as_const(m_Meshes)[m_PlaceholderMesh];
				placeholderData.version = handle.GetVersion();
				m_Meshes[handle] = placeholderData;

				m_MeshMetadata[handle.GetIndex()].placeholderAssigned = true;

				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] -> PLACEHOLDER cube mesh (indexCount=%u, firstIndex=%u)", handle.GetIndex(), handle.GetVersion(), placeholderData.indexCount, placeholderData.firstIndex);
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Missing, "%s Handle=[%u, %u] assigned PLACEHOLDER cube mesh (indexCount=%u, firstIndex=%u)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), placeholderData.indexCount, placeholderData.firstIndex);
			}

			void AssignEmptyMesh(const Core::Handle<Mesh> handle) {
				CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Missing);
				CORI_CORE_ASSERT(IsHandleValidImpl(handle), "Invalid handle.");

				m_Meshes[handle] = Mesh{ .version = handle.GetVersion() };
				m_MeshMetadata[handle.GetIndex()].placeholderAssigned = true;

				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] -> PLACEHOLDER empty mesh (indexCount=0, nothing drawn)", handle.GetIndex(), handle.GetVersion());
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Missing, "%s Handle=[%u, %u] assigned PLACEHOLDER empty mesh (indexCount=0, nothing drawn)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion());
			}

			std::pair<uint32_t, uint32_t> EnsureMetadataSlot(const uint32_t index) {
				const auto size = static_cast<uint32_t>(m_MeshMetadata.size());
				if (index < size) {
					return { size, size };
				}

				m_MeshMetadata.resize(std::max<uint64_t>(static_cast<float>(size) * 1.5f, index + 1));
				return { size, static_cast<uint32_t>(m_MeshMetadata.size()) };
			}

			template<typename VertexT = StaticVertex> requires std::same_as<VertexT, StaticVertex>
			std::tuple<bool, vk::DeviceSize, std::optional<uint64_t>> LoadToMesh(const Core::Handle<Mesh> handle, std::vector<VertexT>&& vertices, std::vector<uint32_t>&& indices, const uint32_t loadGen, const std::optional<CompleteVertexAllocation>& completeVertexAlloc = std::nullopt) {
				constexpr VertexType vertexType = []{
					if constexpr (std::is_same_v<VertexT, StaticVertex>) {
						return VertexType::eStatic;
					}
				}();

				CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Upload);
				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] loadGen=%u vertices=%llu (%llu bytes) indices=%llu (%llu bytes)", handle.GetIndex(), handle.GetVersion(), loadGen, static_cast<unsigned long long>(vertices.size()), static_cast<unsigned long long>(vertices.size() * sizeof(VertexT)), static_cast<unsigned long long>(indices.size()), static_cast<unsigned long long>(indices.size() * sizeof(uint32_t)));
				CORI_CORE_ASSERT(IsHandleValidImpl(handle), "Invalid handle.");

				vma::VirtualAllocationCreateInfo indicesAllocInfo {
					.size = indices.size() * sizeof(uint32_t),
					.alignment = alignof(uint32_t),
					.flags = s_IndexAllocFlags
				};

				vma::VirtualAllocationCreateInfo verticesAllocInfo {
					.size = vertices.size() * sizeof(VertexT),
					.alignment = alignof(VertexT),
					.flags = s_VertexAllocFlags
				};

				vk::DeviceSize indexOffset;
				auto [result, indexAlloc] = m_IndexBufferBlock.virtualAllocate(indicesAllocInfo, indexOffset);
				CORI_CORE_ASSERT(result == vk::Result::eSuccess, "VulkanMeshManager failed to allocate memory for new indices, error: {}", vk::to_string(result));

				vk::DeviceSize vertexOffset;
				vma::VirtualAllocation vertexAlloc;
				VertexStorage* vertexStorage = nullptr;

				if (!completeVertexAlloc) {
					vertexStorage = AllocateNewVertexStorage();
					vertexStorage->DiscoverBoth();

					std::lock_guard lk(*vertexStorage->m_Mutex);
					auto [result2, alloc] = vertexStorage->m_Block.virtualAllocate(verticesAllocInfo, vertexOffset);
					CORI_CORE_ASSERT(result2 == vk::Result::eSuccess, "VulkanMeshManager failed to allocate memory for new vertices in a newly created vertex storage, error: {}", vk::to_string(result2));
					vertexAlloc = alloc;
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Vertex alloc: NEW storage %p @ byte offset %llu, index alloc @ byte offset %llu", static_cast<void*>(vertexStorage), static_cast<unsigned long long>(vertexOffset), static_cast<unsigned long long>(indexOffset));
				} else {
					const auto& cva = completeVertexAlloc.value();
					vertexOffset = cva.offset;
					vertexAlloc = cva.allocation;
					vertexStorage = cva.storage;
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Vertex alloc: reused worker allocation in storage %p @ byte offset %llu, index alloc @ byte offset %llu", static_cast<void*>(vertexStorage), static_cast<unsigned long long>(vertexOffset), static_cast<unsigned long long>(indexOffset));
				}

				VulkanStreamingLine::BufferUpload vertexUpload{
					.resource = vertexStorage->m_Buffer,
					.range = {.offset = vertexOffset, .alignment = alignof(VertexT) },
					.srcPipelineStages = vk::PipelineStageFlagBits2::eTopOfPipe,
					.srcAccessFlags = vk::AccessFlagBits2::eNone
				};

				VulkanStreamingLine::BufferUpload indexUpload{
					.resource = m_IndexBuffer,
					.range = {.offset = indexOffset, .alignment = alignof(uint32_t) },
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
						.firstVertexAddress = vertexStorage->m_Buffer.GetBDA() + vertexOffset,
						.vertexType = std::to_underlying(vertexType),
						.version = handle.GetVersion()
					};

					m_Meshes[handle] = mesh;

					auto& meta = m_MeshMetadata[handle.GetIndex()];

					meta.vertexAllocation = vertexAlloc;
					meta.vertexStorage = vertexStorage;
					meta.indexAllocation = indexAlloc;

					SetAssetStatusImpl(handle, AssetStatus::eStreaming);

					FindInTransferSlot(streamingResult.value()).emplace_back(MeshInTransfer{
						.mesh = handle,
						.vertexStorageBuffer = vertexStorage->m_Buffer.m_Buffer,
						.vertexAllocation = meta.vertexAllocation,
						.vertexStorage = meta.vertexStorage,
						.indexAllocation = meta.indexAllocation,
						.indexCount = static_cast<uint32_t>(indices.size()),
						.indexOffset = static_cast<uint32_t>(indexOffset / sizeof(uint32_t)),
						.vertexByteSize = static_cast<uint32_t>(vertices.size() * sizeof(VertexT)),
						.vertexByteOffset =  static_cast<uint32_t>(vertexOffset),
						.loadGen = loadGen
					});

					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Upload SUBMITTED to streaming line (ticket=%llu), status=Streaming", static_cast<unsigned long long>(streamingResult.value()));
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Upload, "%s Handle=[%u, %u] upload SUBMITTED to streaming line (ticket=%llu, indices=%llu, vertexBytes=%llu), status=Streaming", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), static_cast<unsigned long long>(streamingResult.value()), static_cast<unsigned long long>(indices.size()), static_cast<unsigned long long>(vertices.size() * sizeof(VertexT)));

					return { true, indexOffset, streamingResult.value() };
				}

				if (streamingResult.error() == ErrorCode::eInvalidData) {
					{
						std::lock_guard lk(*vertexStorage->m_Mutex);
						vertexStorage->m_Block.free(vertexAlloc);
					}
					vertexStorage->RemoveStrongRef();
					vertexStorage->RemoveWeakRef();
					m_IndexBufferBlock.free(indexAlloc);
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::MeshManager }, "VulkanStreamingLine returned with error code eInvalidData during CreateMesh call, using placeholder.");

					AssignPlaceholder(handle);

					SetAssetStatusImpl(handle, AssetStatus::eLoadFailed);

					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: streaming line returned eInvalidData, PLACEHOLDER assigned, status=LoadFailed");
					CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eError, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] LoadToMesh FAILED: streaming line eInvalidData, PLACEHOLDER assigned, status=LoadFailed", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion());

					return { false, indexOffset, std::nullopt };
				}

				Mesh mesh{
					.indexCount = 0,
					.firstIndex = 0,
					.firstVertexAddress = vertexStorage->m_Buffer.GetBDA() + vertexOffset,
					.vertexType = std::to_underlying(vertexType),
					.version = handle.GetVersion()
				};

				m_Meshes[handle] = mesh;

				auto& meta = m_MeshMetadata[handle.GetIndex()];

				meta.vertexAllocation = vertexAlloc;
				meta.vertexStorage = vertexStorage;
				meta.indexAllocation = indexAlloc;

				SetAssetStatusImpl(handle, AssetStatus::eStreamingQueued);

				CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Outcome: streaming line full, upload QUEUED, status=StreamingQueued (still showing placeholder)");
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Upload, "%s Handle=[%u, %u] upload QUEUED (streaming line full), status=StreamingQueued", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion());

				m_QueuedUploads.emplace(QueuedUpload{
					.vertexUpload = vertexUpload,
					.indexUpload = indexUpload,
					.vertexData = std::move(vertices),
					.indexData = std::move(indices),
					.mesh = handle,
					.vertexStorage = vertexStorage,
					.indexAlloc = indexAlloc,
					.vertexAlloc = vertexAlloc,
					.loadGen = loadGen
				});
				return { true, indexOffset, std::nullopt };
			}

			VertexStorage* AllocateNewVertexStorage() {
				CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Alloc);
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
				std::string name = std::format("Mesh Manager vertex storage buffer {}", m_VertexStorageBufferCounter.fetch_add(1, std::memory_order_relaxed));
				createInfo.name = name.c_str();
				#endif

				vma::VirtualBlockCreateInfo vertexBlockCreateInfo{
					.size = VERTEX_STORAGE_SIZE
				};

				auto [result1, vertexBlock] = vma::createVirtualBlock(vertexBlockCreateInfo);
				CORI_CORE_ASSERT(result1 == vk::Result::eSuccess, "Failed to create vma vertex storage virtual block. Error: {}", vk::to_string(result1));

				auto storage = m_VertexStorages.emplace_back(VulkanBuffer::Create(createInfo), vertexBlock);

				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "New vertex storage %p (%llu bytes), total storages=%llu", static_cast<void*>(&*storage), static_cast<unsigned long long>(VERTEX_STORAGE_SIZE), static_cast<unsigned long long>(m_VertexStorages.size()));
				CORI_PROFILER_MSG_CFP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Alloc, "%s vertex storage ALLOCATED %p (%llu bytes), total storages=%llu", CORI_CLEAN_TYPE_NAME(Mesh), static_cast<void*>(&*storage), static_cast<unsigned long long>(VERTEX_STORAGE_SIZE), static_cast<unsigned long long>(m_VertexStorages.size()));

				return &*storage;
			}

			void DestroyMesh(Core::Handle<Mesh> handle) {
				CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Destroy);
				if (handle == m_PlaceholderMesh) {
					CORI_PROFILER_ZONE_TEXT_P(Cori::ProfileParts::RenderingAssets, "Skipped: placeholder mesh is never destroyed");
					return;
				}

				auto& meta = m_MeshMetadata[handle.GetIndex()];

				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Handle=[%u, %u] (placeholderAssigned=%d, loaded=%d)", handle.GetIndex(), handle.GetVersion(), static_cast<int>(meta.placeholderAssigned), static_cast<int>(meta.loaded));
				CORI_PROFILER_MSG_SCFP(Cori::ProfileParts::RenderingAssets, Cori::eDebug, Cori::ProfileColors::Destroy, "%s Handle=[%u, %u] DestroyMesh (placeholderAssigned=%d, loaded=%d)", CORI_CLEAN_TYPE_NAME(Mesh), handle.GetIndex(), handle.GetVersion(), static_cast<int>(meta.placeholderAssigned), static_cast<int>(meta.loaded));

				if (!meta.placeholderAssigned && meta.loaded) {
					DeletionQueue::PushVirtualAlloc(meta.vertexAllocation, meta.vertexStorage->m_Block, meta.vertexStorage->m_Mutex);
					DeletionQueue::PushVirtualAlloc(meta.indexAllocation, Get().m_IndexBufferBlock);
					meta.vertexStorage->RemoveWeakRef();
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Queued vertex/index allocations for deletion (storage=%p, refs now strong=%u weak=%u)", static_cast<void*>(meta.vertexStorage), meta.vertexStorage->GetStrongRefCount(), meta.vertexStorage->GetWeakRefCount());
				}

				meta.loaded = false;
				meta.placeholderAssigned = false;

				auto& meshData = m_Meshes[handle];
				meshData.indexCount = 0;
				meshData.firstIndex = 0;
				meshData.firstVertexAddress = 0;
				meshData.vertexType = 0;
			}

			std::vector<MeshInTransfer>& FindInTransferSlot(const uint64_t value) {
				CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Upload);
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

				CORI_CORE_ASSERT(free, "Failed to find any free in transfer slot for a mesh.");

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
				CORI_PROFILE_FUNCTION_CP(Cori::ProfileParts::RenderingAssets, Cori::ProfileColors::Decode);
				fastObjMesh* mesh = nullptr;
				{
					CORI_PROFILE_SCOPE_CP(Cori::ProfileParts::RenderingAssets, "OBJ read (fast_obj)", Cori::ProfileColors::Decode);
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "File: %s", filepath);
					mesh = fast_obj_read(filepath);
				}

				if (!mesh) {
					//the handle-scoped LOAD FAILED message is emitted by the caller, which knows the handle
					CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: fast_obj_read could not read '%s'", filepath);
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

				CORI_PROFILER_ZONE_TEXT_FP(Cori::ProfileParts::RenderingAssets, "Outcome: Parsed %u faces -> %llu unique vertices (%llu bytes), %llu indices (%llu bytes)", mesh->face_count, static_cast<unsigned long long>(outVertices.size()), static_cast<unsigned long long>(outVertices.size() * sizeof(StaticVertex)), static_cast<unsigned long long>(outIndices.size()), static_cast<unsigned long long>(outIndices.size() * sizeof(uint32_t)));

				fast_obj_destroy(mesh);
				return true;
			}

			Core::AssetHandleAllocator<Mesh> m_HandleAllocator;

			VulkanFlatSlotMap<Mesh, 0, false> m_Meshes{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Mesh assets data buffer" };

			std::vector<MeshMetadata> m_MeshMetadata;

			// need to add holes tracking to this!
			tbb::concurrent_vector<VertexStorage> m_VertexStorages;

			vma::VirtualBlock m_IndexBufferBlock;
			VulkanBuffer m_IndexBuffer;

			std::array<InTransferSlot, TRANSFERS_IN_FLIGHT + 2> m_MeshesInTransfer;
			std::queue<QueuedUpload> m_QueuedUploads;

			Core::Handle<Mesh> m_PlaceholderMesh;

			std::vector<vk::BufferMemoryBarrier2> m_BarrierCache;

			std::atomic<uint32_t> m_VertexStorageBufferCounter{ 0 }; //used for giving debug names

			static std::unique_ptr<VulkanMeshManager> s_Instance;

			static constexpr vma::VirtualAllocationCreateFlags s_IndexAllocFlags{ vma::VirtualAllocationCreateFlagBits::eStrategyMinMemory };

			static constexpr vma::VirtualAllocationCreateFlags s_VertexAllocFlags{ vma::VirtualAllocationCreateFlagBits::eStrategyMinMemory };

			static constexpr QueueUsageFlags BUFFER_USAGE{ QueueUsageFlagBits::eGraphics | QueueUsageFlagBits::eTransfer };

			static constexpr uint32_t VERTEX_STORAGE_SIZE{ 1024 * 1024 * 64 }; //64mb

			static constexpr uint32_t INDEX_STORAGE_SIZE{ 1024 * 1024 * 128 }; //128mb
		};
	}

	namespace Core {
		CORI_ADD_ASSET_TRAITS(Mesh, Graphics);
	}
}
