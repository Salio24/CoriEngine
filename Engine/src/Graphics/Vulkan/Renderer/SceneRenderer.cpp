#include "SceneRenderer.hpp"
#include "FrameData.hpp"
#include "MasterRenderer.hpp"

namespace {
	//hack to pass name to PRT only in debug builds, cuz PRT has move/copy constructors & assigment operators deleted.
	const char* GetNameForPRT(Cori::Graphics::SceneRenderer::CreateInfo& createInfo) {
		#ifdef DEBUG_BUILD
		static std::string nameBuffer;
		nameBuffer = std::format("PRT of '{}'", createInfo.name);
		return nameBuffer.c_str();
		#else
		return "";
		#endif
	}

	vk::Rect2D ProjectAABBToScreen(const glm::mat4& viewProjection, const glm::mat4& model, const Cori::Graphics::AABB3D& aabb, const vk::Extent2D extent, const int32_t padding) {
		vk::Rect2D wholeTarget{ { 0, 0 }, extent };

		glm::vec3 center{ aabb.bxCenter, aabb.byCenter, aabb.bzCenter };
		glm::vec3 halfExtent{ aabb.bxExtent, aabb.byExtent, aabb.bzExtent };

		glm::mat4 mvp = viewProjection * model;

		glm::vec2 min{ std::numeric_limits<float>::max() };
		glm::vec2 max{ std::numeric_limits<float>::lowest() };

		for (uint32_t corner = 0; corner < 8; corner++) {
			glm::vec3 local = center + halfExtent * glm::vec3{ (corner & 1u) ? 1.0f : -1.0f, (corner & 2u) ? 1.0f : -1.0f, (corner & 4u) ? 1.0f : -1.0f };

			glm::vec4 clip = mvp * glm::vec4(local, 1.0f);

			if (clip.w <= 0.0f) {
				return wholeTarget;
			}

			glm::vec2 ndc = glm::vec2(clip) / clip.w;

			min = glm::min(min, ndc);
			max = glm::max(max, ndc);
		}

		glm::vec2 screenMin = (min * 0.5f + 0.5f) * glm::vec2(extent.width, extent.height);
		glm::vec2 screenMax = (max * 0.5f + 0.5f) * glm::vec2(extent.width, extent.height);

		int32_t left = std::clamp(static_cast<int32_t>(std::floor(screenMin.x)) - padding, 0, static_cast<int32_t>(extent.width));
		int32_t top = std::clamp(static_cast<int32_t>(std::floor(screenMin.y)) - padding, 0, static_cast<int32_t>(extent.height));
		int32_t right = std::clamp(static_cast<int32_t>(std::ceil(screenMax.x)) + padding, 0, static_cast<int32_t>(extent.width));
		int32_t bottom = std::clamp(static_cast<int32_t>(std::ceil(screenMax.y)) + padding, 0, static_cast<int32_t>(extent.height));

		if (right <= left || bottom <= top) {
			return vk::Rect2D{ { 0, 0 }, { 0, 0 } };
		}

		return vk::Rect2D{
				{ left, top },
				{ static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top) }
			};
	}

	vk::Rect2D UnionRects(const vk::Rect2D a, const vk::Rect2D b) {
		if (a.extent.width == 0 || a.extent.height == 0) {
			return b;
		}

		if (b.extent.width == 0 || b.extent.height == 0) {
			return a;
		}

		int32_t left = std::min(a.offset.x, b.offset.x);
		int32_t top = std::min(a.offset.y, b.offset.y);
		int32_t right = std::max(a.offset.x + static_cast<int32_t>(a.extent.width), b.offset.x + static_cast<int32_t>(b.extent.width));
		int32_t bottom = std::max(a.offset.y + static_cast<int32_t>(a.extent.height), b.offset.y + static_cast<int32_t>(b.extent.height));

		return vk::Rect2D{ { left, top }, { static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top) } };
	}
}

namespace Cori {
	namespace Graphics {
		void SceneRenderer::OnMaterialShaderEffectChanged(void* instance, const Core::Handle<Material> material, const Core::ConstHandle<ShaderEffect> oldShaderEffect, const Core::ConstHandle<ShaderEffect> newShaderEffect) {
			auto* renderer = static_cast<SceneRenderer*>(instance);
			for (auto it = renderer->m_Objects.cbegin(); it != renderer->m_Objects.cend(); ++it) {
				if (it->m_Material.GetHandle() == material) {
					auto& oldBatch = renderer->m_Batches[it->m_OwnerBatch];
					auto mesh = oldBatch.m_Mesh;

					oldBatch.DecrementObjectCounter();
					if (oldBatch.GetObjectCount() == 0) {
						renderer->DestroyBatch(it->m_OwnerBatch);
					}

					auto [newGroup, newBatch] = renderer->FindAppropriateGroupAndBatch(newShaderEffect, std::move(mesh));
					renderer->m_Objects[it.GetIndex()].m_OwnerBatch = newBatch;
					renderer->m_Batches[newBatch].IncrementObjectCounter();
				}
			}
		}

		FrameData* SceneRenderer::PopRecycledFrameData() {
			FrameData** ptr = m_RecycleRing.Front();
			if (!ptr) {
				return nullptr;
			}

			m_RecycleRing.Pop();
			return *ptr;
		}

		void SceneRenderer::ProcessFrameData() {
			FrameData** ptr_ = m_ReadyRing.Front();
			CORI_CORE_ASSERT(ptr_, "SceneRenderer FrameData wasn't ready when ProcessFrameData was called.")
			FrameData* ptr = *ptr_;
			m_ReadyRing.Pop();
			for (auto& patch : ptr->patches) {
				CORI_CORE_ASSERT(IsHandleValid(patch.handle), "FrameData contains a patch with invalid RenderObject handle");

				if (patch.isRegisterRequest) {
					RegisterObject(patch.handle, std::move(patch.mesh.value()), std::move(patch.material.value()), patch.transform, patch.entityID, patch.uvOffsets);
				} else {
					if (patch.mesh) {
						ChangeRenderObjectMesh(patch.handle, std::move(patch.mesh.value()));
					}
					if (patch.material) {
						ChangeRenderObjectMaterial(patch.handle, std::move(patch.material.value()));
					}
					if (patch.isNewTransform) {
						ChangeRenderObjectTransform(patch.handle, patch.transform);
					}
					if (patch.isNewUvOffsets) {
						ChangeRenderObjectUVOffsets(patch.handle, patch.uvOffsets);
					}
				}
			}

			for (auto handle : ptr->deletedObjects) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "FrameData contains a delete request with invalid RenderObject handle");
				UnregisterObject(handle);
			}

			if (ptr->cameraSnapshot.has_value()) {
				m_CameraSnapshot = ptr->cameraSnapshot.value();
			}

			if (ptr->thumbnailCopy.has_value()) {
				m_PendingThumbnailCopy = ptr->thumbnailCopy;
			}

			//a newer click supersedes one that has not been recorded yet, the editor drops the stale ticket
			if (ptr->pickRequest.has_value()) {
				m_PendingPick = ptr->pickRequest;
			}

			m_Highlights = ptr->highlights;

			if (ptr->resizeRequest.has_value()) {
				m_PRT.Resize(ptr->resizeRequest.value());
			}

			m_RecycleRing.Emplace(ptr);
		}

		std::optional<SceneRenderer::FrameContext> SceneRenderer::Stage1(const RendererSettings settings) {
			CORI_PROFILE_FUNCTION();

			uint32_t maxObjectCount = m_Objects.RawSize();
			uint32_t maxBatchCount = m_Batches.RawSize();
			uint32_t maxGroupCount = m_DrawGroups.RawSize();
			if (maxObjectCount == 0 || maxBatchCount == 0 || maxGroupCount == 0) {
				if (m_PendingPick) {
					[[maybe_unused]] const bool pushed = m_PickResultRing.TryEmplace(PickResult{ .ticket = m_PendingPick->ticket, .entityID = s_NullEntityID });
					CORI_CORE_ASSERT(pushed, "SceneRenderer pick result ring was full.");
					m_PendingPick.reset();
				}

				return std::nullopt;
			}

			auto commandOffsetsBuffer = VulkanVirtualBufferAllocator::CreateVirtualUploadBuffer(m_DrawGroups.RawSize() * sizeof(uint32_t), 4, VulkanEngine::GetNextFrameInFlight(), "Draw Group Command Offsets");

			auto uniformBuffer = VulkanVirtualBufferAllocator::CreateVirtualUploadBuffer(sizeof(UniformData), alignof(UniformData), VulkanEngine::GetNextFrameInFlight(), "Uniform Data");

			auto cullDataBuffer = VulkanVirtualBufferAllocator::CreateVirtualUploadBuffer(sizeof(CullData), alignof(CullData), VulkanEngine::GetNextFrameInFlight(), "Cull Data");

			UniformData uniformData;

			vk::Image prtImage = m_PRT.GetImage().m_Image;
			vk::ImageView prtImageView = m_PRT.GetImageView();
			vk::Extent2D prtExtent = { m_PRT.GetImage().m_Extent3D.width, m_PRT.GetImage().m_Extent3D.height };

			uniformData.model = glm::mat4(1.0f);
			uniformData.view = m_CameraSnapshot.view;
			uniformData.proj = m_CameraSnapshot.projection;

			if (!settings.FreezeCulling) {
				if (settings.DisableCulling) {
					for (auto& plane : m_CullData.planes) {
						plane = { 0, 0, 0, 1 };
					}
				} else {
					const glm::mat4 viewProj = uniformData.proj * uniformData.view;
					const glm::mat4 t = glm::transpose(viewProj);

					m_CullData.planes[0] = t[3] + t[0];
					m_CullData.planes[1] = t[3] - t[0];
					m_CullData.planes[2] = t[3] + t[1];
					m_CullData.planes[3] = t[3] - t[1];
					m_CullData.planes[4] = t[2];
					m_CullData.planes[5] = t[3] - t[2];
				}
			}

			const bool recordPick = m_PendingPick.has_value() && VulkanShaderManager::GetAssetStatus(pickingShader.GetHandle()) == AssetStatus::eLoaded;

			vk::Rect2D outlineRect{ { 0, 0 }, { 0, 0 } };

			const bool outlineShadersReady = VulkanShaderManager::GetAssetStatus(outlineShader.GetHandle()) == AssetStatus::eLoaded;

			if (!m_Highlights.empty() && outlineShadersReady) {
				const glm::mat4 viewProjection = uniformData.proj * uniformData.view;

				for (const auto& highlight : m_Highlights) {
					if (!m_Objects.IsIndexValid(highlight.renderObjectIndex)) {
						continue;
					}

					const auto& object = std::as_const(m_Objects)[highlight.renderObjectIndex];
					if (object.valid == 0 || object.m_OwnerBatch >= maxBatchCount) {
						continue;
					}

					const auto aabb = VulkanMeshManager::GetAABB3D(std::as_const(m_BatchGPUInfo)[object.m_OwnerBatch].mesh);
					if (!aabb) {
						continue;
					}

					outlineRect = UnionRects(outlineRect, ProjectAABBToScreen(viewProjection, uniformData.model * object.m_Transform, aabb.value(), prtExtent, static_cast<int32_t>(OUTLINE_RADIUS) + 1));
				}
			}

			const bool recordOutline = outlineRect.extent.width != 0 && outlineRect.extent.height != 0;

			{
				CORI_PROFILE_SCOPE("Render Graph registry reset");

				m_GraphPassRegistry.Reset();
				m_GraphResourceRegistry.Reset(VulkanEngine::GetFrameIndex());
			}

			RenderGraph graph(m_GraphPassRegistry, m_GraphResourceRegistry);

			{
				CORI_PROFILE_SCOPE("Render Graph configuring");

				auto GroupCommandOffsetsBufferHandle = graph.ImportBuffer(commandOffsetsBuffer, "Group Command Offsets Buffer"); // per group <uint32_t>, CPU - write, GPU - compute read, semi-retained mode
				auto UniformDataBufferHandle = graph.ImportBuffer(uniformBuffer, "Uniform Data Buffer");

				auto CullDataBufferHandle = graph.ImportBuffer(cullDataBuffer, "Cull Data Buffer");


				CORI_CORE_ASSERT(maxObjectCount != 0 && maxBatchCount != 0 && maxGroupCount != 0, "something is zero")

					#ifdef CORI_VALIDATION_LAYER
				for (uint32_t objectIndex = 0; objectIndex < maxObjectCount; objectIndex++) {
					if (!m_Objects.IsIndexValid(objectIndex)) {
						continue;
					}

					const auto& object = std::as_const(m_Objects)[objectIndex];

					if (object.valid == 0) {
						continue;
					}

					if (object.m_OwnerBatch >= maxBatchCount) {
						CORI_CORE_ASSERT(false, "Object {} is valid but references batch {}, out of range (batch count {}). The culling shaders would index the batch intermediate info buffer out of bounds.", objectIndex, object.m_OwnerBatch, maxBatchCount);
						continue;
					}

					const auto& batch = std::as_const(m_BatchGPUInfo)[object.m_OwnerBatch];

					CORI_CORE_ASSERT(batch.owner < maxGroupCount, "Batch {} (reached via object {}) references draw group {}, out of range (group count {}).", object.m_OwnerBatch, objectIndex, batch.owner, maxGroupCount);
					CORI_CORE_ASSERT(VulkanMeshManager::IsHandleValid(batch.mesh), "Batch {} (reached via object {}) holds mesh handle [{}, {}], which is not valid. The vertex shader would fetch a garbage firstVertexAddress from it.", object.m_OwnerBatch, objectIndex, batch.mesh.GetIndex(), batch.mesh.GetVersion());
				}
					#endif

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
					uint64_t cullData;

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

				struct OutlinePS{
					uint64_t objectDataBuffer;
					uint64_t meshDataBuffer;
					uint64_t batchInfo;
					uint64_t uniformData;

					uint32_t objectIndex;
					uint32_t color;
					float offsetX;
					float offsetY;
				};

				struct PickPS{
					uint64_t objectDataBuffer;
					uint64_t compactedObjectIDBuffer;
					uint64_t meshDataBuffer;
					uint64_t batchInfo;
					uint64_t uniformData;
					uint64_t pickResult;
				};

				auto DrawCommandBufferHandle = graph.CreateBuffer({ INDIRECT_COMMAND_SIZE * m_Batches.RawSize(), INDIRECT_COMMAND_SIZE }, "Draw Command Buffer"); //per batch <VkDrawIndexedIndirectCommand> CPU - none, GPU - compute write -> command submit read, immediate mode
				auto DrawCommandCountBufferHandle = graph.CreateBuffer({ m_DrawGroups.RawSize() * sizeof(uint32_t), alignof(uint32_t) }, "Draw Command Count"); //per group <uint32_t> CPU - none, GPU - compute write -> indirect command read, immediate mode
				auto BatchIntermediateInfoBufferHandle = graph.CreateBuffer({ m_Batches.RawSize() * sizeof(uint32_t), alignof(uint32_t) }, "Batch Intermediate Info"); //per batch <uint32_t> CPU - none, GPU - compute write/read atomic, immediate mode
				auto InstanceAtomicCounterHandle = graph.CreateBuffer({ sizeof(uint32_t), alignof(uint32_t) }, "Instance Atomic Counter"); //atomic uint32_t, CPU - none, GPU - compute write/read atomic, immediate mode
				auto CompactedInstanceListBufferHandle = graph.CreateBuffer({ m_Objects.RawSize() * sizeof(uint32_t), alignof(uint32_t) }, "Compacted Instance List Buffer"); //per visible instance <uint32_t> CPU - none, GPU - compute write -> vertex/fragment read, immediate mode

				vk::ImageCreateInfo depthImageInfo{
						.imageType = vk::ImageType::e2D,
						.format = s_DepthFormat,
						.extent = { prtExtent.width, prtExtent.height, 1 },
						.mipLevels = 1,
						.arrayLayers = 1,
						.samples = vk::SampleCountFlagBits::e1,
						.tiling = vk::ImageTiling::eOptimal,
						.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
						.sharingMode = vk::SharingMode::eExclusive,
						.initialLayout = vk::ImageLayout::eUndefined
					};

				vma::AllocationCreateInfo depthAllocInfo{
						.usage = vma::MemoryUsage::eAuto,
						.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
					};

				VulkanImage::CreateInfo depthCreateInfo{
						.imageCreateInfo = &depthImageInfo,
						.allocationCreateInfo = &depthAllocInfo,
						.name = "Scene Depth Buffer"
					};

				auto DepthBufferHandle = graph.CreateImage(depthCreateInfo, "Scene Depth Buffer");

				GraphResourceHandle<VulkanImage> OutlineDepthHandle;

				vk::ImageCreateInfo outlineDepthImageInfo{
					.imageType = vk::ImageType::e2D,
					.format = s_DepthFormat,
					.extent = { prtExtent.width, prtExtent.height, 1 },
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = vk::SampleCountFlagBits::e1,
					.tiling = vk::ImageTiling::eOptimal,
					.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
					.sharingMode = vk::SharingMode::eExclusive,
					.initialLayout = vk::ImageLayout::eUndefined
				};

				vma::AllocationCreateInfo outlineDepthAllocInfo{
					.usage = vma::MemoryUsage::eAuto,
					.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal
				};

				VulkanImage::CreateInfo outlineDepthCreateInfo{
					.imageCreateInfo = &outlineDepthImageInfo,
					.allocationCreateInfo = &outlineDepthAllocInfo,
					.name = "Outline Depth"
				};

				if (recordOutline) {
					OutlineDepthHandle = graph.CreateImage(outlineDepthCreateInfo, "Outline Depth");
				}

				auto& bufferCleanupPass = graph.CreatePass("Buffer Cleanup");
				bufferCleanupPass.Writes(DrawCommandCountBufferHandle, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite });
				bufferCleanupPass.Writes(InstanceAtomicCounterHandle, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite });
				bufferCleanupPass.Writes(CompactedInstanceListBufferHandle, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite });
				bufferCleanupPass.Writes(BatchIntermediateInfoBufferHandle, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite });
				bufferCleanupPass.AssignWork([=](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					auto& dccb = registry.GetResource(DrawCommandCountBufferHandle);
					auto& iac = registry.GetResource(InstanceAtomicCounterHandle);
					auto& cilb = registry.GetResource(CompactedInstanceListBufferHandle);
					auto& bii = registry.GetResource(BatchIntermediateInfoBufferHandle);
					commandBuffer.fillBuffer(dccb.GetHeapHandle(), dccb.GetStartOffset(), dccb.GetSize(), 0);
					commandBuffer.fillBuffer(iac.GetHeapHandle(), iac.GetStartOffset(), iac.GetSize(), 0);
					commandBuffer.fillBuffer(cilb.GetHeapHandle(), cilb.GetStartOffset(), cilb.GetSize(), 0);
					commandBuffer.fillBuffer(bii.GetHeapHandle(), bii.GetStartOffset(), bii.GetSize(), 0);
				});

				auto& cullPass = graph.CreatePass("Cull Pass");
				cullPass.Writes(BatchIntermediateInfoBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				cullPass.Reads(CullDataBufferHandle);
				cullPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					ComputePS ps {
							.objectDataBuffer = m_Objects.GetVulkanBuffer().GetBDA(),
							.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
							.bii = registry.GetResource(BatchIntermediateInfoBufferHandle).GetBDA(),
							.commandBuffer = registry.GetResource(DrawCommandBufferHandle).GetBDA(),
							.commandCountBuffer = registry.GetResource(DrawCommandCountBufferHandle).GetBDA(),
							.batchInfos = m_BatchGPUInfo.GetVulkanBuffer().GetBDA(),
							.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
							.globalAtomic = registry.GetResource(InstanceAtomicCounterHandle).GetBDA(),
							.commandOffsets = registry.GetResource(GroupCommandOffsetsBufferHandle).GetBDA(),
							.cullData = registry.GetResource(CullDataBufferHandle).GetBDA(),
							.totalInstanceCount = maxObjectCount,
							.totalBatchCount = maxBatchCount
						};

					commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(ComputePS), &ps);

					VulkanShaderManager::Bind(cullShader.GetHandle(), commandBuffer);
					commandBuffer.dispatch(std::ceil(maxObjectCount / 64.0f), 1, 1);
				});

				auto& cmgPass = graph.CreatePass("Indirect Command Generation Pass");
				cmgPass.Reads(BatchIntermediateInfoBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead });
				cmgPass.Writes(BatchIntermediateInfoBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				cmgPass.Writes(DrawCommandBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				cmgPass.Writes(DrawCommandCountBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				cmgPass.Writes(InstanceAtomicCounterHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				cmgPass.Reads(GroupCommandOffsetsBufferHandle);
				cmgPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					ComputePS ps {
							.objectDataBuffer = m_Objects.GetVulkanBuffer().GetBDA(),
							.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
							.bii = registry.GetResource(BatchIntermediateInfoBufferHandle).GetBDA(),
							.commandBuffer = registry.GetResource(DrawCommandBufferHandle).GetBDA(),
							.commandCountBuffer = registry.GetResource(DrawCommandCountBufferHandle).GetBDA(),
							.batchInfos = m_BatchGPUInfo.GetVulkanBuffer().GetBDA(),
							.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
							.globalAtomic = registry.GetResource(InstanceAtomicCounterHandle).GetBDA(),
							.commandOffsets = registry.GetResource(GroupCommandOffsetsBufferHandle).GetBDA(),
							.cullData = s_PoisonValue,
							.totalInstanceCount = maxObjectCount,
							.totalBatchCount = maxBatchCount
						};

					commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(ComputePS), &ps);

					VulkanShaderManager::Bind(cmgShader.GetHandle(), commandBuffer);
					commandBuffer.dispatch(std::ceil(maxBatchCount / 64.0f), 1, 1);
				});

				auto& compactPass = graph.CreatePass("Compact Pass");
				compactPass.Reads(BatchIntermediateInfoBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageRead });
				compactPass.Writes(BatchIntermediateInfoBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				compactPass.Writes(CompactedInstanceListBufferHandle, { vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite });
				compactPass.Reads(CullDataBufferHandle);
				compactPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					ComputePS ps {
							.objectDataBuffer = m_Objects.GetVulkanBuffer().GetBDA(),
							.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
							.bii = registry.GetResource(BatchIntermediateInfoBufferHandle).GetBDA(),
							.commandBuffer = registry.GetResource(DrawCommandBufferHandle).GetBDA(),
							.commandCountBuffer = registry.GetResource(DrawCommandCountBufferHandle).GetBDA(),
							.batchInfos = m_BatchGPUInfo.GetVulkanBuffer().GetBDA(),
							.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
							.globalAtomic = registry.GetResource(InstanceAtomicCounterHandle).GetBDA(),
							.commandOffsets = registry.GetResource(GroupCommandOffsetsBufferHandle).GetBDA(),
							.cullData = registry.GetResource(CullDataBufferHandle).GetBDA(),
							.totalInstanceCount = maxObjectCount,
							.totalBatchCount = maxBatchCount
						};

					commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(ComputePS), &ps);

					VulkanShaderManager::Bind(compactShader.GetHandle(), commandBuffer);
					commandBuffer.dispatch(std::ceil(maxObjectCount / 64.0f), 1, 1);
				});

				auto& indirectPass = graph.CreatePass("Indirect Pass");
				indirectPass.Reads(CompactedInstanceListBufferHandle, { vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderStorageRead });
				indirectPass.Reads(DrawCommandBufferHandle, { vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead });
				indirectPass.Reads(DrawCommandCountBufferHandle, { vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead });
				indirectPass.Reads(UniformDataBufferHandle);
				indirectPass.Writes(DepthBufferHandle, {
					.stageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
					.accessMask = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
					.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
					.subrange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 } });

				if (recordOutline) {
					indirectPass.Writes(OutlineDepthHandle, {
						.stageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
						.accessMask = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
						.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
						.subrange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 } });
				}

				indirectPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
					DrawPS ps {
							.objectDataBuffer = m_Objects.GetVulkanBuffer().GetBDA(),
							.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
							.materialDataBuffer = VulkanMaterialSystem::GetMaterialSlotMapBDA(),
							.shaderEffectDataBuffer = VulkanShaderEffectManager::GetShaderEffectDataBufferBDA(),
							.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
							.textureAssetTable = VulkanTextureManager::GetTextureAssetTableBDA(),
							.batchInfo = m_BatchGPUInfo.GetVulkanBuffer().GetBDA(),
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

					{
						vk::ImageMemoryBarrier2 prtBar{
								.srcStageMask = vk::PipelineStageFlagBits2::eTransfer | vk::PipelineStageFlagBits2::eFragmentShader,
								.srcAccessMask = vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eShaderSampledRead,
								.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
								.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
								.oldLayout = vk::ImageLayout::eUndefined,
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

					commandBuffer.setViewportWithCount(vk::Viewport(0.0f, 0.0f, static_cast<float>(prtExtent.width), static_cast<float>(prtExtent.height), 0.0f, 1.0f));
					commandBuffer.setScissorWithCount(vk::Rect2D(vk::Offset2D(0, 0), prtExtent));

					vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);
					vk::RenderingAttachmentInfo attachmentInfo = {
							.imageView = prtImageView,
							.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
							.loadOp = vk::AttachmentLoadOp::eClear,
							.storeOp = vk::AttachmentStoreOp::eStore,
							.clearValue = clearColor
						};

					VulkanImage::ImageViewKey depthViewKey{
							.type = vk::ImageViewType::e2D,
							.subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 }
						};

					vk::RenderingAttachmentInfo depthAttachmentInfo = {
							.imageView = registry.GetResource(DepthBufferHandle).GetView(depthViewKey),
							.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
							.loadOp = vk::AttachmentLoadOp::eClear,
							.storeOp = vk::AttachmentStoreOp::eDontCare,
							.clearValue = vk::ClearDepthStencilValue(0.0f, 0)
						};

					vk::RenderingInfo renderingInfo = {
							.renderArea = { .offset = { 0, 0 }, .extent = prtExtent },
							.layerCount = 1,
							.colorAttachmentCount = 1,
							.pColorAttachments = &attachmentInfo,
							.pDepthAttachment = &depthAttachmentInfo
						};
					commandBuffer.beginRendering(renderingInfo);

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

					if (settings.DrawAABBs && VulkanShaderManager::GetAssetStatus(aabbShader.GetHandle()) == AssetStatus::eLoaded) {
						CORI_VK_LABEL(commandBuffer, "Debug AABBs", DebugLabelColors::Scene);

						struct AABBPS {
							uint64_t objectDataBuffer;
							uint64_t batchInfoBuffer;
							uint64_t meshDataBuffer;
							uint64_t uniformData;
						};

						AABBPS ps{
								.objectDataBuffer = m_Objects.GetVulkanBuffer().GetBDA(),
								.batchInfoBuffer = m_BatchGPUInfo.GetVulkanBuffer().GetBDA(),
								.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
								.uniformData = registry.GetResource(UniformDataBufferHandle).GetBDA()
							};

						commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(AABBPS), &ps);

						VulkanShaderManager::Bind(aabbShader.GetHandle(), commandBuffer);

						const PipelineState state{
								.cullMode = vk::CullModeFlagBits::eNone,
								.depthCompareOp = vk::CompareOp::eGreater,
								.depthTestEnable = true,
								.depthWriteEnable = false
							};

						state.Change(commandBuffer);

						commandBuffer.setLineWidth(1.0f);
						commandBuffer.setPrimitiveTopology(vk::PrimitiveTopology::eLineList);

						commandBuffer.draw(AABB_VERTEX_COUNT, maxObjectCount, 0, 0);

						commandBuffer.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

						if (settings.Wireframe) {
							commandBuffer.setLineWidth(settings.WireframeLineWidth);
						}
					}

					commandBuffer.endRendering();

					//drawn here rather than as its own graph pass because the PRT is imported, so the graph can not order a separate pass against the scene draw or against the handover below
					if (recordOutline) {
						CORI_VK_LABEL(commandBuffer, "Outline", DebugLabelColors::Scene);

						{
							vk::ImageMemoryBarrier2 prtBar{
									.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
									.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
									.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
									.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
									.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
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

						VulkanImage::ImageViewKey outlineDepthViewKey{
								.type = vk::ImageViewType::e2D,
								.subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 }
							};

						vk::RenderingAttachmentInfo outlineColorAttachmentInfo = {
								.imageView = prtImageView,
								.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
								.loadOp = vk::AttachmentLoadOp::eLoad,
								.storeOp = vk::AttachmentStoreOp::eStore
							};

						vk::RenderingAttachmentInfo outlineDepthAttachmentInfo = {
								.imageView = registry.GetResource(OutlineDepthHandle).GetView(outlineDepthViewKey),
								.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
								.loadOp = vk::AttachmentLoadOp::eClear,
								.storeOp = vk::AttachmentStoreOp::eDontCare,
								.clearValue = vk::ClearDepthStencilValue(0.0f, 0)
							};

						vk::RenderingInfo outlineRenderingInfo = {
								.renderArea = outlineRect,
								.layerCount = 1,
								.colorAttachmentCount = 1,
								.pColorAttachments = &outlineColorAttachmentInfo,
								.pDepthAttachment = &outlineDepthAttachmentInfo
							};

						VulkanShaderManager::Bind(outlineShader.GetHandle(), commandBuffer);

						constexpr PipelineState outlineSilhouetteState{
								.cullMode = vk::CullModeFlagBits::eNone,
								.depthCompareOp = vk::CompareOp::eAlways,
								.depthTestEnable = true,
								.depthWriteEnable = true
							};

						constexpr PipelineState outlineRingState{
								.cullMode = vk::CullModeFlagBits::eNone,
								.depthCompareOp = vk::CompareOp::eGreater,
								.depthTestEnable = true,
								.depthWriteEnable = false
							};

						if (settings.Wireframe) {
							commandBuffer.setPolygonModeEXT(vk::PolygonMode::eFill);
						}

						commandBuffer.setScissorWithCount(outlineRect);

						constexpr vk::ColorComponentFlags noColor{};
						constexpr vk::ColorComponentFlags fullColor = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

						commandBuffer.beginRendering(outlineRenderingInfo);

						bool outlinedAny = false;

						for (const auto& highlight : m_Highlights) {
							if (!m_Objects.IsIndexValid(highlight.renderObjectIndex)) {
								continue;
							}

							const auto& object = std::as_const(m_Objects)[highlight.renderObjectIndex];
							if (object.valid == 0 || object.m_OwnerBatch >= maxBatchCount) {
								continue;
							}

							const Core::Handle<Mesh> meshHandle = std::as_const(m_BatchGPUInfo)[object.m_OwnerBatch].mesh;
							if (!VulkanMeshManager::IsHandleValid(meshHandle)) {
								continue;
							}

							const auto [indexCount, firstIndex] = VulkanMeshManager::GetDrawRange(meshHandle);
							if (indexCount == 0) {
								continue;
							}

							if (outlinedAny) {
								vk::ClearAttachment depthReset{
									.aspectMask = vk::ImageAspectFlagBits::eDepth,
									.clearValue = vk::ClearDepthStencilValue(0.0f, 0)
								};

								vk::ClearRect depthResetRect{
									.rect = outlineRect,
									.baseArrayLayer = 0,
									.layerCount = 1
								};

								commandBuffer.clearAttachments(depthReset, depthResetRect);
							}

							OutlinePS ps {
									.objectDataBuffer = m_Objects.GetVulkanBuffer().GetBDA(),
									.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
									.batchInfo = m_BatchGPUInfo.GetVulkanBuffer().GetBDA(),
									.uniformData = registry.GetResource(UniformDataBufferHandle).GetBDA(),
									.objectIndex = highlight.renderObjectIndex,
									.color = highlight.color,
									.offsetX = static_cast<float>(OUTLINE_RADIUS) * 2.0f / static_cast<float>(prtExtent.width),
									.offsetY = static_cast<float>(OUTLINE_RADIUS) * 2.0f / static_cast<float>(prtExtent.height)
								};

							commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(OutlinePS), &ps);

							outlineSilhouetteState.Change(commandBuffer);
							commandBuffer.setColorWriteMaskEXT(0, 1, &noColor);

							commandBuffer.drawIndexed(indexCount, 1, firstIndex, 0, 0);

							outlineRingState.Change(commandBuffer);
							commandBuffer.setColorWriteMaskEXT(0, 1, &fullColor);

							commandBuffer.drawIndexed(indexCount, OUTLINE_RING_INSTANCES, firstIndex, 0, 1);

							outlinedAny = true;
						}

						commandBuffer.endRendering();

						if (settings.Wireframe) {
							commandBuffer.setPolygonModeEXT(vk::PolygonMode::eLine);
						}
					}

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
								.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
							};

						vk::DependencyInfo depInfo{
								.imageMemoryBarrierCount = 1,
								.pImageMemoryBarriers = &prtBar
							};

						commandBuffer.pipelineBarrier2(depInfo);
					}
				});

				if (recordPick) {
					const uint64_t readbackOffset = VulkanEngine::GetCurrentFrameInFlight() * sizeof(uint64_t);

					const uint32_t pickX = std::min(static_cast<uint32_t>(std::clamp(m_PendingPick->u, 0.0f, 1.0f) * static_cast<float>(prtExtent.width)), prtExtent.width - 1);
					const uint32_t pickY = std::min(static_cast<uint32_t>(std::clamp(m_PendingPick->v, 0.0f, 1.0f) * static_cast<float>(prtExtent.height)), prtExtent.height - 1);

					const vk::Rect2D pickRect{ vk::Offset2D{ static_cast<int32_t>(pickX), static_cast<int32_t>(pickY) }, vk::Extent2D{ 1, 1 } };

					auto PickResultBufferHandle = graph.CreateBuffer({ sizeof(uint64_t), alignof(uint64_t) }, "Pick Result"); //<uint64_t> CPU - none, GPU - fragment atomic write -> transfer read, immediate mode

					auto& pickClearPass = graph.CreatePass("Pick Clear Pass");
					pickClearPass.Writes(PickResultBufferHandle, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite });
					pickClearPass.AssignWork([=](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
						auto& pickResult = registry.GetResource(PickResultBufferHandle);
						commandBuffer.fillBuffer(pickResult.GetHeapHandle(), pickResult.GetStartOffset(), pickResult.GetSize(), 0);
					});

					auto& pickPass = graph.CreatePass("Pick Pass");
					pickPass.Reads(CompactedInstanceListBufferHandle, { vk::PipelineStageFlagBits2::eVertexShader, vk::AccessFlagBits2::eShaderStorageRead });
					pickPass.Reads(DrawCommandBufferHandle, { vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead });
					pickPass.Reads(DrawCommandCountBufferHandle, { vk::PipelineStageFlagBits2::eDrawIndirect, vk::AccessFlagBits2::eIndirectCommandRead });
					pickPass.Reads(UniformDataBufferHandle);
					pickPass.Writes(PickResultBufferHandle, { vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderStorageWrite });
					pickPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
						CORI_VK_LABEL_F(commandBuffer, DebugLabelColors::Scene, "Pick at {}x{}", pickX, pickY);

						PickPS ps {
								.objectDataBuffer = m_Objects.GetVulkanBuffer().GetBDA(),
								.compactedObjectIDBuffer = registry.GetResource(CompactedInstanceListBufferHandle).GetBDA(),
								.meshDataBuffer = VulkanMeshManager::GetMeshAssetBufferBDA(),
								.batchInfo = m_BatchGPUInfo.GetVulkanBuffer().GetBDA(),
								.uniformData = registry.GetResource(UniformDataBufferHandle).GetBDA(),
								.pickResult = registry.GetResource(PickResultBufferHandle).GetBDA()
							};

						commandBuffer.pushConstants(VulkanGlobalLayoutManager::GetGlobalPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(PickPS), &ps);

						VulkanGlobalLayoutManager::BindDescriptorBuffer(commandBuffer);
						commandBuffer.bindIndexBuffer(VulkanMeshManager::GetIndexBuffer().m_Buffer, 0, vk::IndexType::eUint32);

						VulkanShaderManager::Bind(pickingShader.GetHandle(), commandBuffer);

						const PipelineState state{
								.cullMode = vk::CullModeFlagBits::eNone,
								.depthTestEnable = false,
								.depthWriteEnable = false
							};

						state.Change(commandBuffer);

						if (settings.Wireframe) {
							commandBuffer.setPolygonModeEXT(vk::PolygonMode::eFill);
						}

						commandBuffer.setViewportWithCount(vk::Viewport(0.0f, 0.0f, static_cast<float>(prtExtent.width), static_cast<float>(prtExtent.height), 0.0f, 1.0f));
						commandBuffer.setScissorWithCount(pickRect);

						vk::RenderingInfo renderingInfo = {
								.renderArea = pickRect,
								.layerCount = 1,
								.colorAttachmentCount = 0
							};

						commandBuffer.beginRendering(renderingInfo);

						uint32_t currentCommandOffset = 0;

						auto& indirectCommandBuffer = registry.GetResource(DrawCommandBufferHandle);
						auto& indirectCommandCountBuffer = registry.GetResource(DrawCommandCountBufferHandle);

						for (uint32_t i = 0; i < m_DrawGroups.RawSize(); i++) {
							if (m_DrawGroups.IsIndexValid(i)) {
								auto& group = m_DrawGroups[i];
								if (group.GetBatchCount() != 0) {
									commandBuffer.drawIndexedIndirectCount(indirectCommandBuffer.GetHeapHandle(), currentCommandOffset * INDIRECT_COMMAND_SIZE + indirectCommandBuffer.GetStartOffset(), indirectCommandCountBuffer.GetHeapHandle(), i * sizeof(uint32_t) + indirectCommandCountBuffer.GetStartOffset(), group.GetBatchCount(), INDIRECT_COMMAND_SIZE);
									currentCommandOffset += group.GetBatchCount();
								}
							}
						}

						commandBuffer.endRendering();

						if (settings.Wireframe) {
							commandBuffer.setPolygonModeEXT(vk::PolygonMode::eLine);
						}
					});

					auto& pickReadbackPass = graph.CreatePass("Pick Readback Pass");
					pickReadbackPass.Reads(PickResultBufferHandle, { vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead });
					pickReadbackPass.AssignWork([=, this](vk::CommandBuffer commandBuffer, RenderGraphResourceRegistry& registry) {
						auto& pickResult = registry.GetResource(PickResultBufferHandle);

						vk::BufferCopy region{
								.srcOffset = pickResult.GetStartOffset(),
								.dstOffset = readbackOffset,
								.size = sizeof(uint64_t)
							};

						commandBuffer.copyBuffer(pickResult.GetHeapHandle(), m_PickReadbackBuffer.m_Buffer, region);

						vk::BufferMemoryBarrier2 hostBarrier{
								.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
								.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
								.dstStageMask = vk::PipelineStageFlagBits2::eHost,
								.dstAccessMask = vk::AccessFlagBits2::eHostRead,
								.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.buffer = m_PickReadbackBuffer.m_Buffer,
								.offset = readbackOffset,
								.size = sizeof(uint64_t)
							};

						vk::DependencyInfo depInfo{
								.bufferMemoryBarrierCount = 1,
								.pBufferMemoryBarriers = &hostBarrier
							};

						commandBuffer.pipelineBarrier2(depInfo);
					});
				}
			}

			graph.Compile(VulkanEngine::GetFrameIndex(), VulkanEngine::GetNextFrameInFlight());

			return FrameContext{
					.commandOffsetsBuffer = commandOffsetsBuffer,
					.uniformBuffer = uniformBuffer,
					.cullDataBuffer = cullDataBuffer,
					.graph = graph,
					.uniformData = uniformData,
					.pick = recordPick ? m_PendingPick : std::nullopt
				};
		}

		void SceneRenderer::Stage2(VulkanEngine::FrameInfo& frameData, FrameContext& frameContext) {
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

			frameContext.uniformBuffer.UploadToAllocation(std::span{&frameContext.uniformData, 1}, 0);
			frameContext.cullDataBuffer.UploadToAllocation(std::span{&m_CullData, 1}, 0);

			{
				CORI_PROFILE_SCOPE("Renderer dynamic container sync");
				m_Objects.Sync();
				m_BatchGPUInfo.Sync();
			}
		}

		void SceneRenderer::Stage3(const VulkanEngine::FrameInfo& frameData, FrameContext& frameContext) {
			CORI_VK_LABEL_F(frameData.m_CommandBuffer, DebugLabelColors::Scene, "Scene '{}'", m_Name);

			frameContext.graph.Execute(frameData.m_CommandBuffer);

			if (frameContext.pick) {
				m_PickSlotTickets[VulkanEngine::GetCurrentFrameInFlight()] = frameContext.pick->ticket;
				m_PendingPick.reset();
			}
		}

		void SceneRenderer::DrainPickReadback() {
			const uint32_t slot = VulkanEngine::GetCurrentFrameInFlight();
			const uint64_t ticket = m_PickSlotTickets[slot];

			if (ticket == 0) {
				return;
			}

			m_PickSlotTickets[slot] = 0;

			const uint64_t offset = slot * sizeof(uint64_t);

			auto result = VulkanEngine::GetAllocator().invalidateAllocation(m_PickReadbackBuffer.m_Allocation, offset, sizeof(uint64_t));
			CORI_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to invalidate the pick readback allocation. Error: {}", vk::to_string(result));

			uint64_t packed = 0;
			std::memcpy(&packed, static_cast<const uint8_t*>(m_PickReadbackMapped) + offset, sizeof(packed));

			bool pushed = m_PickResultRing.TryEmplace(PickResult{
					.ticket = ticket,
					.entityID = packed == 0 ? s_NullEntityID : static_cast<EntityValueType>(packed & 0xFFFFFFFFull)
				});

			CORI_CORE_ASSERT(pushed, "SceneRenderer pick result ring was full.");
		}

		bool SceneRenderer::PollPickResult(PickResult& result) {
			PickResult* ptr = m_PickResultRing.Front();
			if (!ptr) {
				return false;
			}

			result = *ptr;
			m_PickResultRing.Pop();
			return true;
		}

		SceneRenderer::~SceneRenderer() {
			VulkanMaterialSystem::RemoveOnShaderEffectSwappedListener(this);

			DeletionQueue::PushBuffer(m_PickReadbackBuffer, DeletionQueue::GetMaxDelay());

			for (auto ptr : m_FrameDataAllocated) {
				delete ptr;
			}
		}

		SceneRenderer::SceneRenderer(CreateInfo&& createInfo)
			: cullShader(Core::AssetManager2::Load<ComputeShader>("enginedata://shaders/Cull_Pass1.json")),
			cmgShader(Core::AssetManager2::Load<ComputeShader>("enginedata://shaders/Cull_Pass2.json")),
			compactShader(Core::AssetManager2::Load<ComputeShader>("enginedata://shaders/Cull_Pass3.json")),
			aabbShader(Core::AssetManager2::Load<VertFragShaderPair>("enginedata://shaders/DebugAABBShader.json")),
			pickingShader(Core::AssetManager2::Load<VertFragShaderPair>("enginedata://shaders/PickingShader.json")),
			outlineShader(Core::AssetManager2::Load<VertFragShaderPair>("enginedata://shaders/OutlineShader.json")),
			m_PRT(createInfo.initialPRTExtent, createInfo.PRTFormat, createInfo.registerPRTWithImGui, GetNameForPRT(createInfo)) {
			m_Objects.Reserve(256);
			m_Batches.Reserve(128);
			m_DrawGroups.Reserve(16);
			m_PickSlotTickets.fill(0);

			{
				vk::BufferCreateInfo pickReadbackInfo{
					.size = sizeof(uint64_t) * FRAMES_IN_FLIGHT,
					.usage = vk::BufferUsageFlagBits::eTransferDst,
					.sharingMode = vk::SharingMode::eExclusive
				};

				vma::AllocationCreateInfo pickReadbackAllocInfo{
					.flags = vma::AllocationCreateFlagBits::eHostAccessRandom | vma::AllocationCreateFlagBits::eMapped,
					.usage = vma::MemoryUsage::eAuto,
					.requiredFlags = vk::MemoryPropertyFlagBits::eHostVisible,
					.preferredFlags = vk::MemoryPropertyFlagBits::eHostCached
				};

				VulkanBuffer::CreateInfo pickReadbackCreateInfo{
					.bufferCreateInfo = &pickReadbackInfo,
					.allocationCreateInfo = &pickReadbackAllocInfo,
					.name = "Pick Readback Buffer"
				};

				m_PickReadbackBuffer = VulkanBuffer::Create(pickReadbackCreateInfo);
				m_PickReadbackMapped = VulkanEngine::GetAllocator().getAllocationInfo(m_PickReadbackBuffer.m_Allocation).pMappedData;

				CORI_CORE_ASSERT(m_PickReadbackMapped, "Pick readback buffer was not persistently mapped.");
			}

			m_PRT.InitialRegisterWithImGui();

			#ifdef DEBUG_BUILD
			m_Name = createInfo.name;
			#endif

			VulkanMaterialSystem::AddOnShaderEffectSwappedListener(this, OnMaterialShaderEffectChanged);

			for (auto& ptr : m_FrameDataAllocated) {
				ptr = new FrameData();
				m_RecycleRing.Emplace(ptr);
			}
		}
		void SceneRenderer::RegisterObject(const Core::Handle<RenderObject> handle, Core::AssetRef<Mesh> mesh, Core::AssetRef<Material> material, const glm::mat4& transform, const uint32_t entityID, const glm::vec4& UVs) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle passed to SceneRenderer::RegisterObject");

			auto shaderEffect = VulkanMaterialSystem::GetMaterialShaderEffect(material.GetHandle());

			auto [groupIndex, batchIndex] = FindAppropriateGroupAndBatch(shaderEffect.value().get().GetHandle(), std::move(mesh));

			m_Batches[batchIndex].IncrementObjectCounter();
			m_TotalObjectCount++;

			if (handle.GetIndex() >= m_Objects.RawSize()) {
				m_Objects.Reserve(handle.GetIndex() * 2);
			}

			m_Objects.EmplaceAt(handle.GetIndex(), transform, UVs, std::move(material), batchIndex, entityID);
		}

		void SceneRenderer::UnregisterObject(const Core::Handle<RenderObject> handle) {
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

		std::expected<std::reference_wrapper<const glm::mat4>, ErrorCode> SceneRenderer::GetRenderObjectTransform(const Core::Handle<RenderObject> handle) {
			if (!IsHandleValid(handle)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			return std::cref(std::as_const(m_Objects)[handle].m_Transform);
		}

		void SceneRenderer::ChangeRenderObjectTransform(const Core::Handle<RenderObject> handle, const glm::mat4& newTransform) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle passed to SceneRenderer::ChangeRenderObjectTransform");

			m_Objects[handle].m_Transform = newTransform;
		}

		std::expected<std::reference_wrapper<const glm::vec4>, ErrorCode> SceneRenderer::GetRenderObjectUVOffsets(const Core::Handle<RenderObject> handle) {
			if (!IsHandleValid(handle)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			return std::cref(std::as_const(m_Objects)[handle].m_UVOffsets);
		}

		void SceneRenderer::ChangeRenderObjectUVOffsets(const Core::Handle<RenderObject> handle, const glm::vec4& newUVOffsets) {
			CORI_CORE_ASSERT(IsHandleValid(handle), "Invalid handle passed to SceneRenderer::ChangeRenderObjectUVOffsets");

			m_Objects[handle].m_UVOffsets = newUVOffsets;
		}

		std::expected<Core::Handle<Mesh>, ErrorCode> SceneRenderer::GetRenderObjectMesh(const Core::Handle<RenderObject> handle) {
			if (!IsHandleValid(handle)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			return m_Batches[std::as_const(m_Objects)[handle].m_OwnerBatch].m_Mesh.GetHandle();
		}

		void SceneRenderer::ChangeRenderObjectMesh(const Core::Handle<RenderObject> handle, Core::AssetRef<Mesh> newMesh) {
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

		std::expected<Core::Handle<Material>, ErrorCode> SceneRenderer::GetRenderObjectMaterial(const Core::Handle<RenderObject> handle) {
			if (!IsHandleValid(handle)) {
				return std::unexpected(ErrorCode::eInvalidHandle);
			}

			//yes const casts are bad and all, but this is a workaround to avoid marking sector as dirty, while at the same time retrieving non const material handle.
			auto& materialRef = const_cast<Core::AssetRef<Material>&>(std::as_const(m_Objects)[handle].m_Material);

			return materialRef.GetHandle();
		}

		void SceneRenderer::ChangeRenderObjectMaterial(const Core::Handle<RenderObject> handle, Core::AssetRef<Material> newMaterial) {
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
			m_Objects[handle].m_Material = std::move(newMaterial);
			m_Batches[newBatch].IncrementObjectCounter();
		}

		std::optional<ThumbnailRect> SceneRenderer::TakePendingThumbnailCopy() {
			std::optional<ThumbnailRect> pending = m_PendingThumbnailCopy;
			m_PendingThumbnailCopy.reset();
			return pending;
		}

		std::pair<SceneRenderer::DrawGroupIndex, BatchIndex> SceneRenderer::FindAppropriateGroupAndBatch(const Core::ConstHandle<ShaderEffect> shaderEffect, Core::AssetRef<Mesh> mesh) {
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

		void SceneRenderer::DestroyBatch(const BatchIndex batchID) {
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

		void SceneRenderer::DestroyGroup(const DrawGroupIndex groupID) {
			auto& group = m_DrawGroups[groupID];

			m_SubBatchLookup.erase(group.m_ShaderEffect);

			m_DrawGroups.Remove({ groupID, 1 });
		}

	}
}