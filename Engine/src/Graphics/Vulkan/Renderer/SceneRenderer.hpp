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
#include "CameraSnapshot.hpp"

namespace Cori {
	namespace Graphics {
		using BatchIndex = uint32_t;

		struct FrameData;

		struct RenderObject {
			RenderObject() = delete;
			RenderObject(const glm::mat4& transform, const glm::vec4& uvOffsets, Core::AssetRef<Material> material, const BatchIndex batch) : m_Transform(transform), m_UVOffsets(uvOffsets), m_Material(std::move(material)), m_OwnerBatch(batch) {}
		private:
			friend SceneRenderer;
			alignas(16) glm::mat4 m_Transform{ 0.0f };
			alignas(16) glm::vec4 m_UVOffsets{ 0.0f, 0.0f, 1.0f, 1.0f };
			Core::AssetRef<Material> m_Material;
			BatchIndex m_OwnerBatch{ 0 };
		public:
			uint32_t valid{ 0 };
		};

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

			void RegisterObject(const Core::Handle<RenderObject> handle, Core::AssetRef<Mesh> mesh, Core::AssetRef<Material> material, const glm::mat4& transform, const glm::vec4& UVs = { 0.0f, 0.0f, 1.0f, 1.0f } ) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle passed to SceneRenderer::RegisterObject");

				auto shaderEffect = VulkanMaterialSystem::GetMaterialShaderEffect(material.GetHandle());

				//if (!shaderEffect) {
				//	return std::unexpected(shaderEffect.error());
				//}

				auto [groupIndex, batchIndex] = FindAppropriateGroupAndBatch(shaderEffect.value().get().GetHandle(), std::move(mesh));

				m_Batches[batchIndex].IncrementObjectCounter();
				m_TotalObjectCount++;

				if (handle.GetIndex() >= m_Objects.RawSize()) {
					m_Objects.Reserve(handle.GetIndex() * 2);
				}

				m_Objects.EmplaceAt(handle.GetIndex(), transform, UVs, std::move(material), batchIndex);
			}

			void UnregisterObject(const Core::Handle<RenderObject> handle) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle passed to SceneRenderer::UnregisterObject");

				const auto& object = std::as_const(m_Objects)[handle];

				auto& ownerBatch = m_Batches[object.m_OwnerBatch];
				ownerBatch.DecrementObjectCounter();
				m_TotalObjectCount--;

				if (ownerBatch.GetObjectCount() == 0) {
					DestroyBatch(object.m_OwnerBatch);
				}

				m_Objects.RemoveAt(handle.GetIndex());
				m_RenderObjectAllocator.Free(handle);
			}

			[[nodiscard]] bool IsHandleValid(const Core::Handle<RenderObject> handle) const {
				return m_RenderObjectAllocator.IsHandleValid(handle);
			}

			[[nodiscard]] std::expected<std::reference_wrapper<const glm::mat4>, ErrorCode> GetRenderObjectTransform(const Core::Handle<RenderObject> handle) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				return std::cref(std::as_const(m_Objects)[handle].m_Transform);
			}

			void ChangeRenderObjectTransform(const Core::Handle<RenderObject> handle, const glm::mat4& newTransform) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle passed to SceneRenderer::ChangeRenderObjectTransform");

				m_Objects[handle].m_Transform = newTransform;

				return;
			}

			[[nodiscard]] std::expected<std::reference_wrapper<const glm::vec4>, ErrorCode> GetRenderObjectUVOffsets(const Core::Handle<RenderObject> handle) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				return std::cref(std::as_const(m_Objects)[handle].m_UVOffsets);
			}

			void ChangeRenderObjectUVOffsets(const Core::Handle<RenderObject> handle, const glm::vec4& newUVOffsets) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle passed to SceneRenderer::ChangeRenderObjectUVOffsets");

				m_Objects[handle].m_UVOffsets = newUVOffsets;
			}

			[[nodiscard]] std::expected<Core::Handle<Mesh>, ErrorCode> GetRenderObjectMesh(const Core::Handle<RenderObject> handle) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				return m_Batches[std::as_const(m_Objects)[handle].m_OwnerBatch].m_Mesh.GetHandle();
			}

			void ChangeRenderObjectMesh(const Core::Handle<RenderObject> handle, Core::AssetRef<Mesh> newMesh) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle passed to SceneRenderer::ChangeRenderObjectMesh");

				auto batchID = std::as_const(m_Objects)[handle].m_OwnerBatch;
				auto& batch = m_Batches[batchID];

				auto oldMesh = batch.m_Mesh;
				if (oldMesh.GetAssetID() == newMesh.GetAssetID()) {
					return;
				}

				auto drawGroupID = std::as_const(m_BatchGPUInfo)[batchID].owner;
				auto shaderEffect = m_DrawGroups[drawGroupID].m_ShaderEffect;

				batch.DecrementObjectCounter();
				if (batch.GetObjectCount() == 0) {
					DestroyBatch(batchID);
				}

				auto [newGroup, newBatch] = FindAppropriateGroupAndBatch(shaderEffect, std::move(newMesh));
				m_Objects[handle].m_OwnerBatch = newBatch;
				m_Batches[newBatch].IncrementObjectCounter();
			}

			[[nodiscard]] std::expected<Core::Handle<Material>, ErrorCode> GetRenderObjectMaterial(const Core::Handle<RenderObject> handle) {
				if (!IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				//yes const casts are bad and all, but this is a workaround to avoid marking sector as dirty, while at the same time retrieving non const material handle.
				auto& materialRef = const_cast<Core::AssetRef<Material>&>(std::as_const(m_Objects)[handle].m_Material);

				return materialRef.GetHandle();
			}

			void ChangeRenderObjectMaterial(const Core::Handle<RenderObject> handle, Core::AssetRef<Material> newMaterial) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle passed to SceneRenderer::ChangeRenderObjectMaterial");

				const auto& constObjectRef = std::as_const(m_Objects)[handle];

				if (newMaterial.GetHandle() == constObjectRef.m_Material.GetHandle()) {
					return;
				}

				auto newShaderEffect = VulkanMaterialSystem::GetMaterialShaderEffect(newMaterial.GetHandle());
				auto oldShaderEffect = VulkanMaterialSystem::GetMaterialShaderEffect(constObjectRef.m_Material.GetHandle());
				if (oldShaderEffect) {
					if (oldShaderEffect.value().get().GetHandle() == newShaderEffect.value().get().GetHandle()) {
						m_Objects[handle].m_Material = newMaterial;
						return;
					}
				}

				auto& oldBatch = m_Batches[constObjectRef.m_OwnerBatch];
				auto mesh = oldBatch.m_Mesh;

				oldBatch.DecrementObjectCounter();
				if (oldBatch.GetObjectCount() == 0) {
					DestroyBatch(constObjectRef.m_OwnerBatch);
				}

				auto [newGroup, newBatch] = FindAppropriateGroupAndBatch(newShaderEffect.value().get().GetHandle(), std::move(mesh));
				m_Objects[handle].m_OwnerBatch = newBatch;
				m_Batches[newBatch].IncrementObjectCounter();
			}

			static void OnMaterialShaderEffectChanged(void* instance, const Core::Handle<Material> material, [[maybe_unused]] const Core::ConstHandle<ShaderEffect> oldShaderEffect, const Core::ConstHandle<ShaderEffect> newShaderEffect);

			[[nodiscard]] bool IsDormant() const {
				return m_IsDormant;
			}

			void MarkNonDormant() {
				m_IsDormant = false;
			}

			[[nodiscard]] FrameData* PopRecycledFrameData();

			[[nodiscard]] FrameData* PeekRecycledFrameData();

			bool PushFrameData(FrameData* frameData) {
				return m_ReadyRing.TryEmplace(frameData);
			}

			[[nodiscard]] bool IsReady() {
				return m_ReadyRing.Front() != nullptr;
			}

			[[nodiscard]] FrameData** PeekFrameData() {
				return m_ReadyRing.Front();
			}

			[[nodiscard]] PersistentRenderTarget& GetPRT() {
				return m_PRT;
			}

			void ProcessFrameData();

			struct UniformData {
				alignas(16) glm::mat4 model{};
				alignas(16) glm::mat4 view{};
				alignas(16) glm::mat4 proj{};
			};

			struct CullData {
				//glm::vec4 left{};
				//glm::vec4 right{};
				//glm::vec4 bottom{};
				//glm::vec4 top{};
				//glm::vec4 near{};
				//glm::vec4 far{};
				alignas(16) std::array<glm::vec4, 6> planes;
			};

			struct FrameContext {
				VulkanVirtualBuffer commandOffsetsBuffer;
				VulkanVirtualBuffer uniformBuffer;
				VulkanVirtualBuffer cullDataBuffer;
				RenderGraph graph;
				UniformData uniformData;
			};

			std::optional<FrameContext> Stage1(const RendererSettings settings);

			void Stage2(VulkanEngine::FrameInfo& frameData, FrameContext& frameContext);

			void Stage3(VulkanEngine::FrameInfo& frameData, FrameContext& frameContext);

			~SceneRenderer();

			explicit SceneRenderer(CreateInfo&& createInfo);

		private:
			[[nodiscard]] std::pair<DrawGroupIndex, BatchIndex> FindAppropriateGroupAndBatch(const Core::ConstHandle<ShaderEffect> shaderEffect, Core::AssetRef<Mesh> mesh) {
				CORI_CORE_ASSERT(mesh.IsInitialized(), "Uninitialized mesh asset ref passed to FindAppropriateGroupAndBatch in SceneRenderer.");
				auto [it, groupInserted] = m_SubBatchLookup.try_emplace(shaderEffect, std::pair<DrawGroupIndex, std::unordered_map<Core::Handle<Mesh>, BatchIndex>>{});

				if (groupInserted) {
					auto handle = m_DrawGroups.Emplace(shaderEffect);

					it->second.first = handle.GetIndex();
					it->second.second.reserve(64);
				}

				auto [it_, batchInserted] = it->second.second.try_emplace(mesh.GetHandle(), BatchIndex{});

				if (batchInserted) {
					auto handle = m_Batches.Emplace(mesh);

					m_BatchGPUInfo.Resize(m_Batches.Capacity());

					m_BatchGPUInfo[handle.GetIndex()] = BatchGPUInfo{ mesh.GetHandle(), it->second.first };

					it_->second = handle.GetIndex();

					auto& group = m_DrawGroups[it->second.first];
					group.IncrementBatchCounter();
				}

				return std::make_pair(it->second.first, it_->second);
			}

			void DestroyBatch(const BatchIndex batchID) {
				auto groupID = std::as_const(m_BatchGPUInfo)[batchID].owner;

				auto& group = m_DrawGroups[groupID];
				group.DecrementBatchCounter();

				auto mesh = m_Batches[batchID].m_Mesh;

				if (group.GetBatchCount() == 0) {
					DestroyGroup(groupID);
				} else {
					m_SubBatchLookup.at(group.m_ShaderEffect).second.erase(mesh.GetHandle());
				}

				m_Batches.Remove({ batchID, 1 });
			}

			void DestroyGroup(const DrawGroupIndex groupID) {
				auto& group = m_DrawGroups[groupID];

				m_SubBatchLookup.erase(group.m_ShaderEffect);

				m_DrawGroups.Remove({ groupID, 1 });
			}

			Threading::ConcurrentHandleAllocator<RenderObject, 64> m_RenderObjectAllocator;
			//VulkanFlatSlotMap<RenderObject, 0, false> m_Objects{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Render Object SlotMap" };

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

			CullData m_CullData;

			RenderGraphResourceRegistry m_GraphResourceRegistry;
			RenderGraphPassRegistry m_GraphPassRegistry;

			CameraSnapshot m_CameraSnapshot;
			PersistentRenderTarget m_PRT;

			#ifdef DEBUG_BUILD
			std::string m_Name;
			#endif

			bool m_IsDormant{ true };

			Threading::SPSCRing<FrameData*> m_ReadyRing{ FRAMES_IN_FLIGHT };
			Threading::SPSCRing<FrameData*> m_RecycleRing{ FRAMES_IN_FLIGHT };
			std::array<FrameData*, FRAMES_IN_FLIGHT> m_FrameDataAllocated{};

			static constexpr uint32_t INDIRECT_COMMAND_SIZE = 20;

			//12 edges of the AABB box, 2 vertices each
			static constexpr uint32_t AABB_VERTEX_COUNT = 24;

			static constexpr vk::Format s_DepthFormat = vk::Format::eD32Sfloat;

			static std::unique_ptr<SceneRenderer> s_Instance;
		};
	}
}