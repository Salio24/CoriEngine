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

#include "PersistentRenderTarget.hpp"
#include "CameraSnapshot.hpp"

namespace Cori {
	namespace Graphics {
		struct FrameData;

		class SceneRenderer {
			using BatchIndex = uint32_t;
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
				Batch() = default;
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

			struct RenderObject {
				RenderObject() = default;
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

			static void Init();

			static void Shutdown();

			static SceneRenderer& Get();

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

				batch.DecrementObjectCounter();
				if (batch.GetObjectCount() == 0) {
					DestroyBatch(batchID);
				}

				auto drawGroupID = std::as_const(m_BatchGPUInfo)[batchID].owner;
				auto& group = m_DrawGroups[drawGroupID];

				auto [newGroup, newBatch] = FindAppropriateGroupAndBatch(group.m_ShaderEffect, std::move(newMesh));
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

			static void OnMaterialShaderEffectChanged(const Core::Handle<Material> material, [[maybe_unused]] const Core::ConstHandle<ShaderEffect> oldShaderEffect, const Core::ConstHandle<ShaderEffect> newShaderEffect) {
				for (auto it = Get().m_Objects.cbegin(); it != Get().m_Objects.cend(); ++it) {
					if (it->m_Material.GetHandle() == material) {
						auto& oldBatch = Get().m_Batches[it->m_OwnerBatch];
						auto mesh = oldBatch.m_Mesh;

						oldBatch.DecrementObjectCounter();
						if (oldBatch.GetObjectCount() == 0) {
							Get().DestroyBatch(it->m_OwnerBatch);
						}

						auto [newGroup, newBatch] = Get().FindAppropriateGroupAndBatch(newShaderEffect, std::move(mesh));
						Get().m_Objects[it.GetHandle()].m_OwnerBatch = newBatch;
						Get().m_Batches[newBatch].IncrementObjectCounter();
					}
				}
			}

			[[nodiscard]] bool IsDormant() const {
				return m_IsDormant;
			}

			void MarkNonDormant() {
				m_IsDormant = false;
			}

			[[nodiscard]] FrameData* PopRecycledFrameData() {
				FrameData* ptr = *m_RecycleRing.Front();
				if (ptr) {
					m_RecycleRing.Pop();
					return ptr;
				}

				return nullptr;
			}

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

			struct FrameContext {
				VulkanVirtualBuffer commandOffsetsBuffer;
				VulkanVirtualBuffer uniformBuffer;
				RenderGraph graph;
				UniformData uniformData;
			};

			FrameContext Stage1() {
				CORI_PROFILE_FUNCTION();

				//VulkanEngine::Get().CPUFrameStart();


				auto commandOffsetsBuffer = VulkanVirtualBufferAllocator::CreateVirtualUploadBuffer(m_DrawGroups.RawSize() * sizeof(uint32_t), 4, VulkanEngine::GetNextFrameInFlight(), "Draw Group Command Offsets");

				auto uniformBuffer = VulkanVirtualBufferAllocator::CreateVirtualUploadBuffer(sizeof(UniformData), alignof(UniformData), VulkanEngine::GetNextFrameInFlight(), "Uniform Data");

				UniformData uniformData;

				static auto startTime = std::chrono::high_resolution_clock::now();
				auto currentTime = std::chrono::high_resolution_clock::now();
				float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

				uniformData.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
				uniformData.model = glm::translate(uniformData.model, glm::vec3(0.0f, 0.0f, 0.0f));

				uniformData.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
				auto swapChainExtent = VulkanEngine::GetSwapChainExtent();
				uniformData.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.1f, 10.0f);
				uniformData.proj[1][1] *= -1;

				{
					CORI_PROFILE_SCOPE("Render Graph registry reset");

					m_GraphPassRegistry.Reset();
					m_GraphResourceRegistry.Reset(VulkanEngine::GetFrameIndex());
				}

				RenderGraph graph(m_GraphPassRegistry, m_GraphResourceRegistry);

				{
					CORI_PROFILE_SCOPE("Render Graph configuring");

					auto ObjectBufferHandle = graph.ImportFlatSlotMap(m_Objects, "Object Data");
					auto BatchInfoBufferHandle = graph.ImportDynamicVector(m_BatchGPUInfo, "Batch Info");

					auto GroupCommandOffsetsBufferHandle = graph.ImportBuffer(commandOffsetsBuffer, "Group Command Offsets Buffer"); // per group <uint32_t>, CPU - write, GPU - compute read, semi-retained mode
					auto UniformDataBufferHandle = graph.ImportBuffer(uniformBuffer, "Uniform Data Buffer"); // per group <uint32_t>, CPU - write, GPU - compute read, semi-retained mode

					uint32_t maxObjectCount = m_Objects.RawSize();
					uint32_t maxBatchCount = m_Batches.RawSize();
					uint32_t maxGroupCount = m_DrawGroups.RawSize();

					struct ComputePS{
						uint64_t objectDataBuffer;
						uint64_t compactedObjectIDBuffer;

						uint64_t bii;

						uint64_t commandBuffer;
						uint64_t commandCountBuffer;
						uint64_t batchInfos;
						uint64_t meshDataBuffer;
						uint64_t globalAtomic;
						uint64_t commandOffsets;

						uint32_t totalInstanceCount;
						uint32_t totalBatchCount;
					};

					struct DrawPS{
						uint64_t objectDataBuffer;
						uint64_t compactedObjectIDBuffer;
						uint64_t materialDataBuffer;
						uint64_t shaderEffectDataBuffer;
						uint64_t meshDataBuffer;
						uint64_t textureAssetTable;
						uint64_t batchInfo;
						uint64_t uniformData;
					};

					auto DrawCommandBufferHandle = graph.CreateBuffer({ INDIRECT_COMMAND_SIZE * m_Batches.RawSize(), INDIRECT_COMMAND_SIZE }, "Draw Command Buffer"); //per batch <VkDrawIndexedIndirectCommand> CPU - none, GPU - compute write -> command submit read, immediate mode
					auto DrawCommandCountBufferHandle = graph.CreateBuffer({ m_DrawGroups.RawSize() * sizeof(uint32_t), alignof(uint32_t) }, "Draw Command Count"); //per group <uint32_t> CPU - none, GPU - compute write -> indirect command read, immediate mode
					auto BatchIntermediateInfoBufferHandle = graph.CreateBuffer({ m_Batches.RawSize() * sizeof(uint64_t), alignof(uint64_t) }, "Batch Intermediate Info"); //per batch <uint64_t> CPU - none, GPU - compute write/read atomic, immediate mode
					auto InstanceAtomicCounterHandle = graph.CreateBuffer({ sizeof(uint32_t), alignof(uint32_t) }, "Instance Atomic Counter"); //atomic uint32_t, CPU - none, GPU - compute write/read atomic, immediate mode
					auto CompactedInstanceListBufferHandle = graph.CreateBuffer({ m_Objects.RawSize() * sizeof(uint32_t), alignof(uint32_t) }, "Compacted Instance List Buffer"); //per visible instance <uint32_t> CPU - none, GPU - compute write -> vertex/fragment read, immediate mode

					vk::Image prtImage = m_PRT.GetImage().m_Image;
					vk::ImageView prtImageView = m_PRT.GetImageView();
					vk::Extent2D prtExtent = { m_PRT.GetImage().m_Extent3D.width, m_PRT.GetImage().m_Extent3D.height };

					auto& bufferCleanupPass = graph.CreatePass("Buffer Cleanup");
					bufferCleanupPass.Writes(DrawCommandCountBufferHandle, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite });
					bufferCleanupPass.Writes(InstanceAtomicCounterHandle, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite });
					bufferCleanupPass.Writes(CompactedInstanceListBufferHandle, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite });
					bufferCleanupPass.AssignWork([=](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
						auto& dccb = registry.GetResource(DrawCommandCountBufferHandle);
						auto& iac = registry.GetResource(InstanceAtomicCounterHandle);
						auto& cilb = registry.GetResource(CompactedInstanceListBufferHandle);
						commandBuffer.fillBuffer(dccb.GetHeapHandle(), dccb.GetStartOffset(), dccb.GetSize(), 0);
						commandBuffer.fillBuffer(iac.GetHeapHandle(), iac.GetStartOffset(), iac.GetSize(), 0);
						commandBuffer.fillBuffer(cilb.GetHeapHandle(), cilb.GetStartOffset(), cilb.GetSize(), 0);

					});

					auto& cullPass = graph.CreatePass("Cull Pass");
					cullPass.Writes(BatchIntermediateInfoBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
					cullPass.Reads(ObjectBufferHandle);
					cullPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
						ComputePS ps {
							.objectDataBuffer = registry.GetResource(ObjectBufferHandle).GetVulkanBuffer().GetBDA(),
							.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
							.bii = registry.GetResource(BatchIntermediateInfoBufferHandle).GetBDA(),
							.commandBuffer = registry.GetResource(DrawCommandBufferHandle).GetBDA(),
							.commandCountBuffer = registry.GetResource(DrawCommandCountBufferHandle).GetBDA(),
							.batchInfos = registry.GetResource(BatchInfoBufferHandle).GetVulkanBuffer().GetBDA(),
							.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
							.globalAtomic = registry.GetResource(InstanceAtomicCounterHandle).GetBDA(),
							.commandOffsets = registry.GetResource(GroupCommandOffsetsBufferHandle).GetBDA(),
							.totalInstanceCount = maxObjectCount,
							.totalBatchCount = maxBatchCount
						};

						commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(ComputePS), &ps);

						//auto result = VulkanShaderManager::GetShader(cullShader);
						//CORI_CORE_ASSERT(result, "Failed to get cullShader. Error: {}", to_string(result.error()));
						//result.value().get().Bind(commandBuffer);

						VulkanShaderManager::Bind(cullShader.GetHandle(), commandBuffer);
						commandBuffer.dispatch(std::ceil(maxObjectCount / 64.0f), 1, 1);
					});

					auto& cmgPass = graph.CreatePass("Indirect Command Generation Pass");
					cmgPass.Writes(BatchIntermediateInfoBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
					cmgPass.Writes(DrawCommandBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
					cmgPass.Writes(DrawCommandCountBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
					cmgPass.Writes(InstanceAtomicCounterHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
					cmgPass.Reads(BatchInfoBufferHandle);
					cmgPass.Reads(GroupCommandOffsetsBufferHandle);
					cmgPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
						ComputePS ps {
							.objectDataBuffer = registry.GetResource(ObjectBufferHandle).GetVulkanBuffer().GetBDA(),
							.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
							.bii = registry.GetResource(BatchIntermediateInfoBufferHandle).GetBDA(),
							.commandBuffer = registry.GetResource(DrawCommandBufferHandle).GetBDA(),
							.commandCountBuffer = registry.GetResource(DrawCommandCountBufferHandle).GetBDA(),
							.batchInfos = registry.GetResource(BatchInfoBufferHandle).GetVulkanBuffer().GetBDA(),
							.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
							.globalAtomic = registry.GetResource(InstanceAtomicCounterHandle).GetBDA(),
							.commandOffsets = registry.GetResource(GroupCommandOffsetsBufferHandle).GetBDA(),
							.totalInstanceCount = maxObjectCount,
							.totalBatchCount = maxBatchCount
						};

						commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(ComputePS), &ps);

						//auto result = VulkanShaderManager::GetShader(cmgShader);
						//CORI_CORE_ASSERT(result, "Failed to get cmgShader. Error: {}", to_string(result.error()));
						//result.value().get().Bind(commandBuffer);

						VulkanShaderManager::Bind(cmgShader.GetHandle(), commandBuffer);
						commandBuffer.dispatch(std::ceil(maxBatchCount / 64.0f), 1, 1);
					});

					auto& compactPass = graph.CreatePass("Compact Pass");
					compactPass.Writes(BatchIntermediateInfoBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
					compactPass.Writes(CompactedInstanceListBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
					compactPass.Reads(ObjectBufferHandle);
					compactPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
						ComputePS ps {
							.objectDataBuffer = registry.GetResource(ObjectBufferHandle).GetVulkanBuffer().GetBDA(),
							.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
							.bii = registry.GetResource(BatchIntermediateInfoBufferHandle).GetBDA(),
							.commandBuffer = registry.GetResource(DrawCommandBufferHandle).GetBDA(),
							.commandCountBuffer = registry.GetResource(DrawCommandCountBufferHandle).GetBDA(),
							.batchInfos = registry.GetResource(BatchInfoBufferHandle).GetVulkanBuffer().GetBDA(),
							.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
							.globalAtomic = registry.GetResource(InstanceAtomicCounterHandle).GetBDA(),
							.commandOffsets = registry.GetResource(GroupCommandOffsetsBufferHandle).GetBDA(),
							.totalInstanceCount = maxObjectCount,
							.totalBatchCount = maxBatchCount
						};

						commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(ComputePS), &ps);

						//auto result = VulkanShaderManager::GetShader(compactShader);
						//CORI_CORE_ASSERT(result, "Failed to get compactShader. Error: {}", to_string(result.error()));
						//result.value().get().Bind(commandBuffer);

						VulkanShaderManager::Bind(compactShader.GetHandle(), commandBuffer);
						commandBuffer.dispatch(std::ceil(maxObjectCount / 64.0f), 1, 1);
					});

					auto& indirectPass = graph.CreatePass("Indirect Pass");
					indirectPass.Reads(ObjectBufferHandle);
					indirectPass.Reads(CompactedInstanceListBufferHandle, { vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderStorageRead });
					indirectPass.Reads(DrawCommandBufferHandle, { vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead });
					indirectPass.Reads(DrawCommandCountBufferHandle, { vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead });
					indirectPass.Reads(UniformDataBufferHandle);
					indirectPass.Reads(BatchInfoBufferHandle);
					indirectPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
						DrawPS ps {
							.objectDataBuffer = registry.GetResource(ObjectBufferHandle).GetVulkanBuffer().GetBDA(),
							.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
							.materialDataBuffer = VulkanMaterialSystem::GetMaterialSlotMapBDA(),
							.shaderEffectDataBuffer = VulkanShaderEffectManager::GetShaderEffectDataBufferBDA(),
							.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
							.textureAssetTable = VulkanTextureManager::GetTextureAssetTableBDA(),
							.batchInfo = registry.GetResource(BatchInfoBufferHandle).GetVulkanBuffer().GetBDA(),
							.uniformData = registry.GetResource(UniformDataBufferHandle).GetBDA()
						};

						commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(DrawPS), &ps);

						VulkanGlobalLayoutManager::BindDescriptorBuffer(commandBuffer);
						commandBuffer.bindIndexBuffer(VulkanMeshManager::GetIndexBuffer().m_Buffer, 0, vk::IndexType::eUint32);

						PipelineState currentPipelineState;
						currentPipelineState.Change(commandBuffer);

						uint32_t currentCommandOffset = 0;
						Core::ConstHandle<ShaderEffect> currentShaderEffect;

						auto& indirectCommandBuffer = registry.GetResource(DrawCommandBufferHandle);
						auto& indirectCommandCountBuffer = registry.GetResource(DrawCommandCountBufferHandle);

						//this is temporary, need to add support to external images to the render graph
						{
							vk::ImageMemoryBarrier2 prtBar{
							   .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
							   .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
							   .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							   .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
							   .oldLayout = vk::ImageLayout::eTransferDstOptimal,
							   .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
							   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							   .image = prtImage,
							   .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
						   };

						   vk::DependencyInfo depInfo{
							   .imageMemoryBarrierCount = 1,
							   .pImageMemoryBarriers = &prtBar
						   };

						   commandBuffer.pipelineBarrier2(depInfo);
						}

						//temp
						vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);
						vk::RenderingAttachmentInfo attachmentInfo = {
							.imageView = prtImageView,
							.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
							.loadOp = vk::AttachmentLoadOp::eClear,
							.storeOp = vk::AttachmentStoreOp::eStore,
							.clearValue = clearColor
						};

						vk::RenderingInfo renderingInfo = {
							.renderArea = { .offset = { 0, 0 }, .extent = prtExtent },
							.layerCount = 1,
							.colorAttachmentCount = 1,
							.pColorAttachments = &attachmentInfo
						};
						commandBuffer.beginRendering(renderingInfo);
						//temp

						for (uint32_t i = 0; i < m_DrawGroups.RawSize(); i++) {
							if (m_DrawGroups.IsIndexValid(i)) {
								auto& group = m_DrawGroups[i];
								if (group.GetBatchCount() != 0) {
									if (group.m_ShaderEffect != currentShaderEffect) {
										auto pairHandleResult = VulkanShaderEffectManager::GetShaderEffectShaderPair(group.m_ShaderEffect);

										CORI_CORE_ASSERT(pairHandleResult, "Group hold an invalid  shader effect handle.");

										VulkanShaderManager::Bind(pairHandleResult.value().get().GetHandle(), commandBuffer);

										auto pipelineState = VulkanShaderEffectManager::GetShaderEffectPipelineState(group.m_ShaderEffect).value();

										if (pipelineState.get() != currentPipelineState) {
											pipelineState.get().Change(commandBuffer);
										}

										currentPipelineState = pipelineState;
										currentShaderEffect = group.m_ShaderEffect;
									}

									commandBuffer.drawIndexedIndirectCount(indirectCommandBuffer.GetHeapHandle(), currentCommandOffset * INDIRECT_COMMAND_SIZE + indirectCommandBuffer.GetStartOffset(), indirectCommandCountBuffer.GetHeapHandle(), i * sizeof(uint32_t) + indirectCommandCountBuffer.GetStartOffset(), group.GetBatchCount(), INDIRECT_COMMAND_SIZE);
									currentCommandOffset += group.GetBatchCount();
								}
							}
						}

						commandBuffer.endRendering();

						//this is temporary, need to add support to external images to the render graph
						{
							vk::ImageMemoryBarrier2 prtBar{
									.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
									.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
									.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
									.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
									.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
									.newLayout = vk::ImageLayout::eTransferSrcOptimal,
									.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
									.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
									.image = prtImage,
									.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
								};

							vk::DependencyInfo depInfo{
									.imageMemoryBarrierCount = 1,
									.pImageMemoryBarriers = &prtBar
								};

							commandBuffer.pipelineBarrier2(depInfo);
						}
					});
				}

				graph.Compile(VulkanEngine::GetFrameIndex(), VulkanEngine::GetNextFrameInFlight());
				/*
				auto& frameData = VulkanEngine::Get().GPUFrameBegin();
				if (!frameData.m_SkippedFrame) {
					if (m_DrawGroups.RawSize() > 0) {
						uint32_t value = 0;
						commandOffsetsBuffer.UploadToAllocation<uint32_t>(std::span{ &value, 1 }, 0);
					}

					uint32_t commandOffset = 0;
					for (uint32_t i = 0; i < m_DrawGroups.RawSize() - 1; i++) {
						if (m_DrawGroups.IsIndexValid(i)) {
							commandOffset += m_DrawGroups[i].GetBatchCount();
						}

						commandOffsetsBuffer.UploadToAllocation<uint32_t>(std::span{ &commandOffset, 1 }, sizeof(uint32_t) * (i + 1));
					}

					uniformBuffer.UploadToAllocation(std::span<UniformData>{ &uniformData, 1 }, 0);

					{
						CORI_PROFILE_SCOPE("Renderer dynamic container sync");
						m_Objects.Sync();
						m_BatchGPUInfo.Sync();
					}

					VulkanEngine::Get().GPUFrameMiddlePointSync();
					graph.Execute(frameData.m_CommandBuffer);
				}

				ImGuiRenderer::Render(frameData.m_CommandBuffer, VulkanEngine::GetSwapChainImageView(), VulkanEngine::GetSwapChainExtent(), frameData.m_SkippedFrame);

				VulkanEngine::Get().GPUFrameEnd();
				*/

				return FrameContext{
					.commandOffsetsBuffer = commandOffsetsBuffer,
					.uniformBuffer = uniformBuffer,
					.graph = graph,
					.uniformData = uniformData
				};
			}

			void Stage2(VulkanEngine::FrameInfo& frameData, FrameContext& frameContext) {
				if (m_DrawGroups.RawSize() > 0) {
					uint32_t value = 0;
					frameContext.commandOffsetsBuffer.UploadToAllocation<uint32_t>(std::span{&value, 1}, 0);
				}

				uint32_t commandOffset = 0;
				for (uint32_t i = 0; i < m_DrawGroups.RawSize() - 1; i++) {
					if (m_DrawGroups.IsIndexValid(i)) {
						commandOffset += m_DrawGroups[i].GetBatchCount();
					}

					frameContext.commandOffsetsBuffer.UploadToAllocation<uint32_t>(std::span{&commandOffset, 1}, sizeof(uint32_t) * (i + 1));
				}

				frameContext.uniformBuffer.UploadToAllocation(std::span<UniformData>{&frameContext.uniformData, 1}, 0);

				{
					CORI_PROFILE_SCOPE("Renderer dynamic container sync");
					m_Objects.Sync();
					m_BatchGPUInfo.Sync();
				}
			}

			void Stage3(VulkanEngine::FrameInfo& frameData, FrameContext& frameContext) {
				frameContext.graph.Execute(frameData.m_CommandBuffer);
			}

			~SceneRenderer() = default;

			SceneRenderer(CreateInfo&& createInfo);

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

				m_Batches.Remove(m_Batches.GetIndexHandle(batchID));
			}

			void DestroyGroup(const DrawGroupIndex groupID) {
				auto& group = m_DrawGroups[groupID];

				m_SubBatchLookup.erase(group.m_ShaderEffect);

				m_DrawGroups.Remove(m_DrawGroups.GetIndexHandle(groupID));
			}

			Threading::ConcurrentHandleAllocator<RenderObject, 64> m_RenderObjectAllocator;
			VulkanFlatSlotMap<RenderObject, 0, false> m_Objects{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Render Object SlotMap" };

			Core::FlatSlotMap<Batch, 0, false> m_Batches;
			VulkanDynamicVector<BatchGPUInfo> m_BatchGPUInfo{ QueueUsageFlagBits::eGraphics, vk::BufferUsageFlagBits::eShaderDeviceAddress, "Batch GPU Data" };

			Core::FlatSlotMap<DrawGroup, 0, false> m_DrawGroups;
			uint32_t m_TotalObjectCount{ 0 };

			std::unordered_map<Core::ConstHandle<ShaderEffect>, std::pair<DrawGroupIndex, std::unordered_map<Core::Handle<Mesh>, BatchIndex>>> m_SubBatchLookup;

			Core::AssetRef<ComputeShader> cullShader;
			Core::AssetRef<ComputeShader> cmgShader;
			Core::AssetRef<ComputeShader> compactShader;

			//Core::Handle<VertFragShaderPair> testShader;
			//Core::Handle<VertFragShaderPair> defaultShader;

			Core::AssetRef<Mesh> quad;
			//Core::AssetRef<Material> material;
			//Core::AssetRef<Material> material2;
			Core::AssetRef<Material> swordMaterial;
			Core::AssetRef<Mesh> sword;

			//Core::AssetRef<Texture2> texture;
			//Core::AssetRef<Texture2> swordAlbedo;

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
			std::array<FrameData*, FRAMES_IN_FLIGHT> m_FrameDataAllocated;

			static constexpr uint32_t INDIRECT_COMMAND_SIZE = 20;

			static std::unique_ptr<SceneRenderer> s_Instance;
		};
	}
}