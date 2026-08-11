#pragma once
#include "VulkanBuffer.hpp"
#include "VulkanEngine.hpp"
#include "VulkanUploadSubsystem.hpp"
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "AssetManager/AssetLoadStatus.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "Core/AssetManager/AssetDependencies.hpp"
#include "Core/AssetManager/AssetHandleAllocator.hpp"
#include "Graphics/RenderThreadCommandQueue.hpp"
#include <fast_obj.h>

#include "Core/DataStructures/SparseFlatSlotMap.hpp"

namespace Cori {
	namespace Graphics {
		enum class VertexType : uint32_t {
			eStatic
		};

		class VulkanMeshManager;

		struct AABB3D {
			float bxCenter{ 0 };
			float byCenter{ 0 };
			float bzCenter{ 0 };
			float bxExtent{ 0 };
			float byExtent{ 0 };
			float bzExtent{ 0 };
		};

		struct Mesh : Core::SecondaryAssetBase {
			using Manager = VulkanMeshManager;
			uint32_t indexCount{ 0 };
			uint32_t firstIndex{ 0 };
			uint64_t firstVertexAddress{ 0 };
			uint32_t vertexType{ 0 };
			uint32_t version{ 0 };
			float bxCenter{ 0 };
			float byCenter{ 0 };
			float bzCenter{ 0 };
			float bxExtent{ 0 };
			float byExtent{ 0 };
			float bzExtent{ 0 };
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

					m_AABB.bxCenter = other.m_AABB.bxCenter;
					m_AABB.byCenter = other.m_AABB.byCenter;
					m_AABB.bzCenter = other.m_AABB.bzCenter;
					m_AABB.bxExtent = other.m_AABB.bxExtent;
					m_AABB.byExtent = other.m_AABB.byExtent;
					m_AABB.bzExtent = other.m_AABB.bzExtent;

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

					m_AABB.bxCenter = other.m_AABB.bxCenter;
					m_AABB.byCenter = other.m_AABB.byCenter;
					m_AABB.bzCenter = other.m_AABB.bzCenter;
					m_AABB.bxExtent = other.m_AABB.bxExtent;
					m_AABB.byExtent = other.m_AABB.byExtent;
					m_AABB.bzExtent = other.m_AABB.bzExtent;

					other.Release();
					return *this;
				}

				void Release() {
					if (m_CompleteVertexAlloc.storage) {
						//m_CompleteVertexAlloc.storage->RemoveStrongRef();
						m_CompleteVertexAlloc = {};
					}
				}

				AABB3D m_AABB;

				std::variant<std::vector<StaticVertex>> m_VertexData;
				std::vector<uint32_t> m_IndexData;
				CompleteVertexAllocation m_CompleteVertexAlloc;
			};

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

			struct AABBAtomics {
				std::atomic<uint32_t> gen;
				std::atomic<uint32_t> bxCenter;
				std::atomic<uint32_t> byCenter;
				std::atomic<uint32_t> bzCenter;
				std::atomic<uint32_t> bxExtent;
				std::atomic<uint32_t> byExtent;
				std::atomic<uint32_t> bzExtent;
			};

		public:
			~VulkanMeshManager();

			static void Init();

			static void Shutdown();

			template<typename T> requires std::same_as<Mesh, T>
			[[nodiscard]] static Core::Handle<Mesh> AllocateHandle() {
				return Get().m_HandleAllocator.Allocate();
			}

			static void BindAsset(const Core::Handle<Mesh> handle, const Core::AssetID id, const uint32_t vectorKey);

			static uint32_t BumpGeneration(const Core::Handle<Mesh> handle);

			[[nodiscard]] static bool IsHandleValid(const Core::Handle<Mesh> handle);

			static Core::AssetID GetAssetID(const Core::Handle<Mesh> handle);

			template<typename T> requires std::same_as<Mesh, T>
			static Core::Handle<T> GetPlaceholder() {
				return Get().m_PlaceholderMesh;
			}

			static bool TryAddRef(const Core::Handle<Mesh> handle);

			static void AddRef(Core::Handle<Mesh> handle);

			static void RemoveRef(Core::Handle<Mesh> handle);

			static void SetAssetStatus(const Core::Handle<Mesh> handle, const AssetStatus newStatus);

			[[nodiscard]] static AssetStatus GetAssetStatus(const Core::Handle<Mesh> handle);

			[[nodiscard]] static uint32_t GetIdentityVersion(const Core::Handle<Mesh> handle);

			[[nodiscard]] static std::expected<std::pair<Core::AssetDependencySet, uint32_t>, ErrorCode> TryReadDependencies(const Core::Handle<Mesh> handle);

			static void PublishIdentity(const Core::Handle<Mesh> handle);

			static void RegisterAtSlot(const Core::Handle<Mesh> handle);

			static void Load(const Core::Handle<Mesh> handle, const Core::AssetID id, const uint32_t gen, const uint32_t vectorKey, std::filesystem::path path, std::string name = "");

			static void Unload(const Core::Handle<Mesh> handle);

			static void QueueUnload(const Core::Handle<Mesh> handle);

			static void ProcessUpdates(vk::CommandBuffer cmb);

			[[nodiscard]] static VulkanBuffer& GetIndexBuffer();

			[[nodiscard]] static uint64_t GetMeshAssetBufferBDA();

			[[nodiscard]] static std::expected<AABB3D, ErrorCode> GetAABB3D(const Core::Handle<Mesh> handle);

			static void AllocateExtras(const Core::Handle<Mesh> handle);

			static void FreeExtras(const Core::Handle<Mesh> handle);

			static constexpr bool EnableHotReload = true;
			static constexpr bool EnableAutoHotReload = true;

		private:
			VulkanMeshManager();

			static VulkanMeshManager& Get();

			[[nodiscard]] bool IsHandleValidImpl(const Core::Handle<Mesh> handle) const;

			void SetAssetStatusImpl(const Core::Handle<Mesh> handle, const AssetStatus newStatus);

			void AssignPlaceholder(const Core::Handle<Mesh> handle);

			void AssignEmptyMesh(const Core::Handle<Mesh> handle);

			std::pair<uint32_t, uint32_t> EnsureMetadataSlot(const uint32_t index);

			template<typename VertexT = StaticVertex> requires std::same_as<VertexT, StaticVertex>
			std::tuple<bool, vk::DeviceSize, std::optional<uint64_t>> LoadToMesh(const Core::Handle<Mesh> handle, std::vector<VertexT>&& vertices, std::vector<uint32_t>&& indices, const uint32_t loadGen, const AABB3D& aabb, const std::optional<CompleteVertexAllocation>& completeVertexAlloc = std::nullopt);

			VertexStorage* AllocateNewVertexStorage();

			void DestroyMesh(Core::Handle<Mesh> handle);

			std::vector<MeshInTransfer>& FindInTransferSlot(const uint64_t value);

			static uint32_t PackNormal(const glm::vec3& n);

			static uint32_t PackTangent(const glm::vec3& t, float bitangentSign);

			static uint32_t PackColor(const glm::vec3& c);

			//temporary and writen by gemini. quick and dirty and does its job, will move to gltf and engine specific asset types once i have an editor
			static bool LoadObjToEngine(const char* filepath, std::vector<StaticVertex>& outVertices, std::vector<uint32_t>& outIndices);

			Core::AssetHandleAllocator<Mesh> m_HandleAllocator;

			template<typename T> using MeshGPUStorage = VulkanGPUSyncedSequentialStorage<T, QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Mesh assets data buffer">;
			Core::SparseFlatSlotMap<Mesh, 0, false, MeshGPUStorage> m_Meshes;

			std::vector<MeshMetadata> m_MeshMetadata;
			tbb::concurrent_vector<AABBAtomics> m_AABBs;

			//FIXME: need to add holes tracking to this!
			tbb::concurrent_vector<VertexStorage> m_VertexStorages;

			vma::VirtualBlock m_IndexBufferBlock;
			VulkanBuffer m_IndexBuffer;

			std::array<InTransferSlot, TRANSFERS_IN_FLIGHT + 2> m_MeshesInTransfer;
			std::queue<QueuedUpload> m_QueuedUploads;

			Core::Handle<Mesh> m_PlaceholderMesh;

			std::vector<vk::BufferMemoryBarrier2> m_BarrierCache;

			std::atomic<uint32_t> m_VertexStorageBufferCounter{ 0 }; //used for giving debug names

			static constexpr vma::VirtualAllocationCreateFlags s_IndexAllocFlags{ vma::VirtualAllocationCreateFlagBits::eStrategyMinMemory };

			static constexpr vma::VirtualAllocationCreateFlags s_VertexAllocFlags{ vma::VirtualAllocationCreateFlagBits::eStrategyMinMemory };

			static constexpr QueueUsageFlags BUFFER_USAGE{ QueueUsageFlagBits::eGraphics | QueueUsageFlagBits::eTransfer };

			static constexpr uint32_t VERTEX_STORAGE_SIZE{ 1024 * 1024 * 64 }; //64mb

			static constexpr uint32_t INDEX_STORAGE_SIZE{ 1024 * 1024 * 128 }; //128mb

			static std::unique_ptr<VulkanMeshManager> s_Instance;
		};
	}

	namespace Core {
		CORI_ADD_ASSET_TRAITS(Mesh, Graphics);
	}
}
