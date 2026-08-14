#pragma once
#include "Graphics/Vulkan/VulkanEngine.hpp"
#include "RenderGraph.hpp"
#include "Core/Time.hpp"
#include "Graphics/Vulkan/VulkanUploadSubsystem.hpp"
#include "Graphics/Vulkan/VulkanMeshManager.hpp"
#include "Graphics/Vulkan/VulkanShaderManager.hpp"
#include "Graphics/Vulkan/VulkanLayoutManager.hpp"
#include "Graphics/Vulkan/VulkanTextureManager.hpp"
#include "Graphics/Vulkan/VulkanMaterialSystem.hpp"
#include "ImGuiRenderer.hpp"
#include "FileSystem/PathManager.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "Core/Threading/ConcurrentHandleAllocator.hpp"
#include "Core/Threading/SPSCRing.hpp"
#include "Graphics/RendererSettings.hpp"

#include "PersistentRenderTarget.hpp"
#include "ThumbnailRect.hpp"
#include "PickRequest.hpp"
#include "HighlightRequest.hpp"
#include "CameraSnapshot.hpp"

namespace Cori {
	namespace Graphics {
		using BatchIndex = uint32_t;

		struct FrameData;

		struct RenderObject {
			RenderObject() = delete;
			RenderObject(const glm::mat4& transform, const glm::vec4& uvOffsets, Core::AssetRef<Material> material, const BatchIndex batch, const uint32_t entity) : m_Transform(transform), m_UVOffsets(uvOffsets), m_Material(std::move(material)), m_OwnerBatch(batch), entityID(entity) {}
		private:
			friend SceneRenderer;
			alignas(16) glm::mat4 m_Transform{ 0.0f };
			alignas(16) glm::vec4 m_UVOffsets{ 0.0f, 0.0f, 1.0f, 1.0f };
			Core::AssetRef<Material> m_Material;
			BatchIndex m_OwnerBatch{ 0 };
		public:
			uint32_t valid{ 0 };
			EntityValueType entityID{ s_NullEntityID };
			uint32_t pad1{};
			uint32_t pad2{};
			uint32_t pad3{};
		};

		static_assert(sizeof(RenderObject) == 112, "CHANGE THE SLANG DEFINE!");

		class SceneRenderer {
			using DrawGroupIndex = uint32_t;

			class DrawGroup {
			public:
				DrawGroup() = default;
				explicit DrawGroup(const Core::ConstHandle<ShaderEffect> shaderEffect) : m_ShaderEffect(shaderEffect) {}

				[[nodiscard]] uint32_t GetBatchCount() const {
					return m_BatchCount;
				}

				void IncrementBatchCounter() {
					m_BatchCount++;
				}

				void DecrementBatchCounter() {
					m_BatchCount--;
				}

				Core::ConstHandle<ShaderEffect> m_ShaderEffect;
			private:
				uint32_t m_BatchCount{ 0 };
			};

			class Batch {
			public:
				explicit Batch(Core::AssetRef<Mesh> mesh) : m_Mesh(std::move(mesh)) {}

				[[nodiscard]] uint32_t GetObjectCount() const {
					return m_ObjectCount;
				}

				void IncrementObjectCounter() {
					m_ObjectCount++;
				}

				void DecrementObjectCounter() {
					m_ObjectCount--;
				}

				Core::AssetRef<Mesh> m_Mesh;
			private:
				uint32_t m_ObjectCount{ 0 };
			};

			struct BatchGPUInfo {
				Core::Handle<Mesh> mesh;
				DrawGroupIndex owner{ 0 };
			};

		public:
			struct CreateInfo {
				vk::Extent2D initialPRTExtent;
				vk::Format PRTFormat;
				#ifdef DEBUG_BUILD
				std::string name;
				#endif
				bool registerPRTWithImGui;
			};

			[[nodiscard]] Core::Handle<RenderObject> AllocateRenderObjectHandle() {
				return m_RenderObjectAllocator.Allocate();
			}

			void RegisterObject(const Core::Handle<RenderObject> handle, Core::AssetRef<Mesh> mesh, Core::AssetRef<Material> material, const glm::mat4& transform, const EntityValueType entityID, const glm::vec4& UVs = { 0.0f, 0.0f, 1.0f, 1.0f } );

			void UnregisterObject(const Core::Handle<RenderObject> handle);

			[[nodiscard]] bool IsHandleValid(const Core::Handle<RenderObject> handle) const {
				return m_RenderObjectAllocator.IsHandleValid(handle);
			}

			[[nodiscard]] std::expected<std::reference_wrapper<const glm::mat4>, ErrorCode> GetRenderObjectTransform(const Core::Handle<RenderObject> handle);

			void ChangeRenderObjectTransform(const Core::Handle<RenderObject> handle, const glm::mat4& newTransform);

			[[nodiscard]] std::expected<std::reference_wrapper<const glm::vec4>, ErrorCode> GetRenderObjectUVOffsets(const Core::Handle<RenderObject> handle);

			void ChangeRenderObjectUVOffsets(const Core::Handle<RenderObject> handle, const glm::vec4& newUVOffsets);

			[[nodiscard]] std::expected<Core::Handle<Mesh>, ErrorCode> GetRenderObjectMesh(const Core::Handle<RenderObject> handle);

			void ChangeRenderObjectMesh(const Core::Handle<RenderObject> handle, Core::AssetRef<Mesh> newMesh);

			[[nodiscard]] std::expected<Core::Handle<Material>, ErrorCode> GetRenderObjectMaterial(const Core::Handle<RenderObject> handle);

			void ChangeRenderObjectMaterial(const Core::Handle<RenderObject> handle, Core::AssetRef<Material> newMaterial);

			static void OnMaterialShaderEffectChanged(void* instance, const Core::Handle<Material> material, [[maybe_unused]] const Core::ConstHandle<ShaderEffect> oldShaderEffect, const Core::ConstHandle<ShaderEffect> newShaderEffect);

			[[nodiscard]] FrameData* PopRecycledFrameData();

			bool PushFrameData(FrameData* frameData) {
				return m_ReadyRing.TryEmplace(frameData);
			}

			[[nodiscard]] PersistentRenderTarget& GetPRT() {
				return m_PRT;
			}

			[[nodiscard]] std::optional<ThumbnailRect> TakePendingThumbnailCopy();

			void NotifyThumbnailCopyRecorded() {
				m_ThumbnailCopyCount.fetch_add(1, std::memory_order_release);
			}

			[[nodiscard]] uint64_t GetThumbnailCopyCount() const {
				return m_ThumbnailCopyCount.load(std::memory_order_acquire);
			}

			void DrainPickReadback();

			[[nodiscard]] bool PollPickResult(PickResult& result);

			void ProcessFrameData();

			struct UniformData {
				alignas(16) glm::mat4 model{};
				alignas(16) glm::mat4 view{};
				alignas(16) glm::mat4 proj{};
			};

			struct CullData {
				alignas(16) std::array<glm::vec4, 6> planes;
			};

			struct FrameContext {
				VulkanVirtualBuffer commandOffsetsBuffer;
				VulkanVirtualBuffer uniformBuffer;
				VulkanVirtualBuffer cullDataBuffer;
				RenderGraph graph;
				UniformData uniformData;
				std::optional<PickRequest> pick;
			};

			std::optional<FrameContext> Stage1(const RendererSettings settings);

			void Stage2(VulkanEngine::FrameInfo& frameData, FrameContext& frameContext);

			void Stage3(const VulkanEngine::FrameInfo& frameData, FrameContext& frameContext);

			~SceneRenderer();

			explicit SceneRenderer(CreateInfo&& createInfo);

		private:
			[[nodiscard]] std::pair<DrawGroupIndex, BatchIndex> FindAppropriateGroupAndBatch(const Core::ConstHandle<ShaderEffect> shaderEffect, Core::AssetRef<Mesh> mesh);

			void DestroyBatch(const BatchIndex batchID);

			void DestroyGroup(const DrawGroupIndex groupID);

			Threading::ConcurrentHandleAllocator<RenderObject, 64> m_RenderObjectAllocator;

			template<typename T> using ObjectsGPUStorage = VulkanGPUSyncedSequentialStorage<T, QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Render Object SlotMap">;

			Core::SparseFlatSlotMap<RenderObject, 0, false, ObjectsGPUStorage> m_Objects;

			Core::SparseFlatSlotMap<Batch, 64, false> m_Batches;
			VulkanDynamicVector<BatchGPUInfo> m_BatchGPUInfo{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Batch GPU Data" };

			Core::SparseFlatSlotMap<DrawGroup, 64, false> m_DrawGroups;
			uint32_t m_TotalObjectCount{ 0 };

			std::unordered_map<Core::ConstHandle<ShaderEffect>, std::pair<DrawGroupIndex, std::unordered_map<Core::Handle<Mesh>, BatchIndex>>> m_SubBatchLookup;

			Core::AssetRef<ComputeShader> cullShader;
			Core::AssetRef<ComputeShader> cmgShader;
			Core::AssetRef<ComputeShader> compactShader;
			Core::AssetRef<VertFragShaderPair> aabbShader;
			Core::AssetRef<VertFragShaderPair> pickingShader;
			Core::AssetRef<VertFragShaderPair> outlineShader;


			std::vector<HighlightRequest> m_Highlights;

			CullData m_CullData;

			RenderGraphResourceRegistry m_GraphResourceRegistry;
			RenderGraphPassRegistry m_GraphPassRegistry;

			CameraSnapshot m_CameraSnapshot;
			PersistentRenderTarget m_PRT;

			std::optional<ThumbnailRect> m_PendingThumbnailCopy;
			std::atomic<uint64_t> m_ThumbnailCopyCount{ 0 };

			std::optional<PickRequest> m_PendingPick;
			VulkanBuffer m_PickReadbackBuffer;
			Threading::SPSCRing<PickResult> m_PickResultRing{ FRAMES_IN_FLIGHT + 2};

			std::array<uint64_t, FRAMES_IN_FLIGHT> m_PickSlotTickets{};

			#ifdef DEBUG_BUILD
			std::string m_Name;
			#endif

			Threading::SPSCRing<FrameData*> m_ReadyRing{ FRAMES_IN_FLIGHT };
			Threading::SPSCRing<FrameData*> m_RecycleRing{ FRAMES_IN_FLIGHT };
			std::array<FrameData*, FRAMES_IN_FLIGHT> m_FrameDataAllocated{};

			void* m_PickReadbackMapped{ nullptr };

			static constexpr uint32_t INDIRECT_COMMAND_SIZE = 20;

			static constexpr uint32_t AABB_VERTEX_COUNT = 24;

			static constexpr uint32_t OUTLINE_RADIUS = 2;

			static constexpr uint32_t OUTLINE_RING_INSTANCES = 8;

			static constexpr vk::Format s_DepthFormat = vk::Format::eD32Sfloat;

			static std::unique_ptr<SceneRenderer> s_Instance;
		};
	}
}