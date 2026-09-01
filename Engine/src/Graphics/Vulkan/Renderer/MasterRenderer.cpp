#include "MasterRenderer.hpp"
#include "ThumbnailAtlas.hpp"
#include "Graphics/Vulkan/VulkanTextureManager.hpp"
#include "FrameData.hpp"
#include "Core/Threading/CpuTopology.hpp"

namespace Cori {
	namespace Graphics {
		void MasterRenderer::RtTask() {
			SetThreadName("Render");
			if (Threading::CpuTopology::ShouldBind()) {
				Threading::CpuTopology::BindCurrentThreadToDomain(Threading::CpuTopology::PreferredDomain());
			}

			RenderThreadCommandQueue::SetExecuterThreadId(std::this_thread::get_id());

			while (m_Running.load(std::memory_order_acquire)) {
				CORI_PROFILE_FUNCTION();
				CORI_PROFILER_PLOT("Render thread CPU", Threading::CpuTopology::CurrentCpu());
				uint64_t wakeBefore = RenderThreadWakeup::Snapshot();

				ProcessPendingSceneRendererCreations();
				ProcessPendingSceneRendererDestructions();

				if (!m_Running.load(std::memory_order_acquire)) {
					break;
				}

				std::array<SceneRenderer*, s_MaxSceneRendererCount> ptrs{};
				uint32_t counter = 0;
				for (auto& atomic : m_SceneRenderers) {
					ptrs[counter++] = atomic.load();
				}

				{
					CORI_PROFILE_SCOPE("Master Frame");
					bool result = TryRunFrame();
					if (result) {
						RenderThreadCommandQueue::DrainOnRenderThread();
						continue;
					}
				}

				if (RenderThreadCommandQueue::DrainOnRenderThread() > 0) {
					continue;
				}

				if (RenderThreadWakeup::Snapshot() != wakeBefore) {
					continue;
				}

				RenderThreadWakeup::WaitChanged(wakeBefore);
			}
		}

		bool MasterRenderer::TryRunFrame() {
			MasterFrameData** frameDataPtr = m_ReadyRing.Front();
			if (!frameDataPtr) {
				return false;
			}

			MasterFrameData* frameData = *frameDataPtr;

			if (RenderThreadCommandQueue::DrainedCount() < frameData->rtcqWatermark) {
				return false;
			}

			VulkanPresentTiming::ThrottlePresentQueue();

			m_ReadyRing.Pop();

			std::array<ParticipatingRenderer, s_MaxSceneRendererCount> participants;
			participants.fill({ nullptr, UINT32_MAX, std::nullopt, false });
			uint32_t participantCount = 0;

			for (uint32_t i = 0; i < frameData->participantCount; i++) {
				const auto [handle, generation] = frameData->participants[i];

				if (m_Generation[handle].load(std::memory_order_acquire) != generation) {
					continue;
				}

				SceneRenderer* ptr = m_SceneRenderers[handle].load(std::memory_order_acquire);
				if (!ptr) {
					continue;
				}

				participants[participantCount].ptr = ptr;
				participants[participantCount].handle = handle;
				participantCount++;
			}

			ImGuiRenderer::ProcessTexQueueRequests();
			VulkanTextureManager::ProcessImGuiBindingRequests();

			FrameLatencyStamps latencyStamps{};
			const FrameLatencyStamps& sceneStamps = frameData->latencyStamps;
			const RendererSettings settings = frameData->settings;

			if (sceneStamps.inputTimestampSdl != 0) {
				latencyStamps.inputTimestampSdl = sceneStamps.inputTimestampSdl;
			}

			if (sceneStamps.frameStartHost != 0) {
				latencyStamps.frameStartHost = sceneStamps.frameStartHost;
			}

			SceneRendererHandle requestedHandle = m_MainRenderer.load(std::memory_order_acquire);
			Mode mode = m_CurrentMode.load(std::memory_order_acquire);

			for (uint32_t i = 0; i < participantCount; i++) {
				SceneRenderer* ptr = participants[i].ptr;
				ptr->ProcessFrameData();
			}

			uint32_t emptySceneCount = 0;

			VulkanEngine::Get().CPUFrameStart();
			for (uint32_t i = 0; i < participantCount; i++) {
				SceneRenderer* ptr = participants[i].ptr;
				participants[i].context = ptr->Stage1(settings);
				if (!participants[i].context) {
					emptySceneCount++;
					participants[i].sceneEmpty = true;
				}
			}

			auto& frameInfo = VulkanEngine::Get().GPUFrameBegin();

			for (auto& renderer : m_SceneRenderers) {
				SceneRenderer* ptr = renderer.load(std::memory_order_acquire);
				if (ptr) {
					ptr->DrainPickReadback();
				}
			}

			if (settings.Wireframe) {
				frameInfo.m_CommandBuffer.setLineWidth(settings.WireframeLineWidth);
				frameInfo.m_CommandBuffer.setPolygonModeEXT(vk::PolygonMode::eLine);

			}

			if (participantCount != 0 && !frameInfo.m_SkippedFrame) {
				DeletionQueue::Flush();
			}

			if (!frameInfo.m_SkippedFrame) {
				InitializeFreshPRTs(frameInfo.m_CommandBuffer);

				for (uint32_t i = 0; i < participantCount; i++) {
					if (!participants[i].context) {
						continue;
					}
					SceneRenderer* ptr = participants[i].ptr;
					ptr->Stage2(frameInfo, participants[i].context.value());
				}

				VulkanEngine::Get().GPUFrameMiddlePointSync();

				for (uint32_t i = 0; i < participantCount; i++) {
					if (!participants[i].context) {
						continue;
					}
					SceneRenderer* ptr = participants[i].ptr;
					ptr->Stage3(frameInfo, participants[i].context.value());
				}

				if (settings.Wireframe) {
					frameInfo.m_CommandBuffer.setPolygonModeEXT(vk::PolygonMode::eFill);
				}

				std::array<ThumbnailAtlas::Copy, s_MaxSceneRendererCount> thumbnailCopies{};
				uint32_t thumbnailCopyCount = 0;

				for (uint32_t i = 0; i < participantCount; i++) {
					if (participants[i].sceneEmpty) {
						continue;
					}

					SceneRenderer* ptr = participants[i].ptr;
					const std::optional<ThumbnailRect> rect = ptr->TakePendingThumbnailCopy();
					if (!rect) {
						continue;
					}

					const VulkanImage& sourceImage = ptr->GetPRT().GetImage();
					thumbnailCopies[thumbnailCopyCount] = ThumbnailAtlas::Copy{
						.sourceImage = sourceImage.m_Image,
						.sourceExtent = { sourceImage.m_Extent3D.width, sourceImage.m_Extent3D.height },
						.rect = rect.value()
					};

					thumbnailCopyCount++;
					ptr->NotifyThumbnailCopyRecorded();
				}

				ThumbnailAtlas::Get().ExecuteCopies(frameInfo.m_CommandBuffer, std::span<const ThumbnailAtlas::Copy>(thumbnailCopies.data(), thumbnailCopyCount));

				Composite(frameInfo.m_CommandBuffer, participants, participantCount, emptySceneCount, mode, requestedHandle);
			}

			ImGuiRenderer::RecycleSnapshot();

			VulkanEngine::Get().GPUFrameEnd(latencyStamps);

			frameData->Clear();
			m_RecycleRing.Emplace(frameData);

			return true;
		}
		std::unique_ptr<MasterRenderer> MasterRenderer::s_Instance{ nullptr };

		void MasterRenderer::Init() {
			CORI_CORE_ASSERT(!s_Instance, "MasterRenderer is already initialized.");
			s_Instance = std::unique_ptr<MasterRenderer>(new MasterRenderer());
		}

		void MasterRenderer::Shutdown() {
			s_Instance.reset();
		}

		MasterRenderer& MasterRenderer::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling MasterRenderer::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
		SceneRendererHandle MasterRenderer::CreateSceneRenderer(SceneRenderer::CreateInfo&& info) {
			SceneRendererHandle handle;

			{
				std::lock_guard lk(m_QueueMutex);
				CORI_CORE_ASSERT(!m_FreeList.empty(), "Out of scene renderer slots.");
				handle = m_FreeList.front();
				const auto old = m_Generation[handle].fetch_add(1, std::memory_order_release);
				m_FreeList.pop_front();
				m_PendingCreations.emplace_back(handle, std::move(info), old + 1);
			}

			RenderThreadWakeup::Wake();
			return handle;
		}

		void MasterRenderer::DestroySceneRenderer(SceneRendererHandle handle) {
			{
				std::lock_guard lk(m_QueueMutex);
				m_Generation[handle].fetch_add(1, std::memory_order_release);
				m_PendingDestructions.emplace_back(m_SceneRenderers[handle].load(std::memory_order_acquire));
				m_SceneRenderers[handle].store(nullptr, std::memory_order_release);
				m_FreeList.push_back(handle);
			}

			RenderThreadWakeup::Wake();
		}

		void MasterRenderer::BeginFrame() {
			CORI_CORE_ASSERT(!m_BuildingFrame, "MasterRenderer::BeginFrame was called twice without an EndFrame in between.");

			while (m_BuildingFrame == nullptr) {
				if (m_ReadyRing.Size() < GetAdmitDepth()) {
					MasterFrameData** ptr = m_RecycleRing.Front();
					if (ptr) {
						m_BuildingFrame = *ptr;
						m_RecycleRing.Pop();
					}
				}
			}
		}

		void MasterRenderer::AddParticipant(const SceneRendererHandle handle) {
			CORI_CORE_ASSERT(m_BuildingFrame, "MasterRenderer::AddParticipant was called outside of a BeginFrame/EndFrame pair.");
			CORI_CORE_ASSERT(handle < s_MaxSceneRendererCount, "Invalid scene renderer handle passed to MasterRenderer::AddParticipant.");

			m_BuildingFrame->participants[m_BuildingFrame->participantCount] = { handle, m_Generation[handle].load(std::memory_order_acquire) };
			m_BuildingFrame->participantCount++;

			CORI_CORE_ASSERT(m_BuildingFrame->participantCount <= s_MaxSceneRendererCount, "More participants than there are scene renderer slots.");
		}

		void MasterRenderer::EndFrame(const FrameLatencyStamps& latencyStamps, const RendererSettings& settings) {
			CORI_CORE_ASSERT(m_BuildingFrame, "MasterRenderer::EndFrame was called without a matching BeginFrame.");

			m_BuildingFrame->latencyStamps = latencyStamps;
			m_BuildingFrame->settings = settings;
			m_BuildingFrame->rtcqWatermark = RenderThreadCommandQueue::CurrentPushCount();

			[[maybe_unused]] const bool result = m_ReadyRing.TryEmplace(m_BuildingFrame);
			CORI_CORE_ASSERT(result, "MasterRenderer ready ring was full while publishing a frame.");

			m_BuildingFrame = nullptr;

			RenderThreadWakeup::Wake();
		}

		MasterRenderer::~MasterRenderer() {
			RenderThreadCommandQueue::Clear();
			m_PendingCreations.clear();
			ProcessPendingSceneRendererDestructions();

			for (auto& renderer : m_SceneRenderers) {
				delete renderer.load(std::memory_order_relaxed);
			}

		}

		void MasterRenderer::ExitThreadedMode() {
			m_Running.store(false, std::memory_order_release);
			RenderThreadWakeup::Wake();
			m_RenderThread.join();

			RenderThreadCommandQueue::SetExecuterThreadId(std::this_thread::get_id());
			RenderThreadCommandQueue::DrainOnRenderThread();
		}

		MasterRenderer::MasterRenderer() {
			for (uint32_t i = 0; i < s_MaxSceneRendererCount; i++) {
				m_FreeList.emplace_back(i);
			}

			for (auto& inst : m_FrameDataStorage) {
				m_RecycleRing.Emplace(&inst);
			}
		}

		void MasterRenderer::ProcessPendingSceneRendererCreations() {
			static std::vector<PendingCreation> copy;
			std::array<std::pair<SceneRenderer*, uint32_t>, s_MaxSceneRendererCount> createdRenderers{};

			{
				std::lock_guard lk(m_QueueMutex);
				copy.swap(m_PendingCreations);
			}

			for (auto& pendingCreation : copy) {
				if (m_Generation[pendingCreation.handle].load(std::memory_order_acquire) == pendingCreation.generation) {
					createdRenderers[pendingCreation.handle] = { new SceneRenderer(std::move(pendingCreation.creationInfo)), pendingCreation.generation };
				}
			}

			{
				std::lock_guard lk(m_QueueMutex);

				for (uint32_t i = 0; i < s_MaxSceneRendererCount; i++) {
					auto [ptr, gen] = createdRenderers[i];
					if (ptr) {
						if (m_Generation[i].load(std::memory_order_acquire) == gen) {
							m_SceneRenderers[i].store(ptr, std::memory_order_release);
						} else {
							delete ptr;
						}
					}
				}
			}

			copy.clear();
		}

		void MasterRenderer::ProcessPendingSceneRendererDestructions() {
			static std::vector<SceneRenderer*> copy;

			{
				std::lock_guard lk(m_QueueMutex);
				copy.swap(m_PendingDestructions);
			}

			for (auto ptr : copy) {
				delete ptr;
			}

			copy.clear();
		}

		void MasterRenderer::InitializeFreshPRTs(vk::CommandBuffer cmb) {
			static std::vector<vk::ImageMemoryBarrier2> toShaderRead;

			toShaderRead.clear();

			for (const vk::Image image : m_PRTInitialTransitionQueue) {
				toShaderRead.emplace_back(vk::ImageMemoryBarrier2{
					.srcStageMask = vk::PipelineStageFlagBits2::eNone,
					.srcAccessMask = vk::AccessFlagBits2::eNone,
					.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
					.dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
					.oldLayout = vk::ImageLayout::eUndefined,
					.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = image,
					.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
				});
			}


			if (m_PRTInitialTransitionQueue.empty()) {
				return;
			}

			CORI_VK_LABEL_F(cmb, DebugLabelColors::Composite, "Initialize {} fresh PRT image(s)", m_PRTInitialTransitionQueue.size());

			{
				CORI_VK_LABEL_INSERT(cmb, "Fresh PRT images -> ShaderReadOnlyOptimal", DebugLabelColors::Barrier);

				vk::DependencyInfo depInfo{
					.imageMemoryBarrierCount = static_cast<uint32_t>(toShaderRead.size()),
					.pImageMemoryBarriers = toShaderRead.data()
				};

				m_PRTInitialTransitionQueue.clear();

				cmb.pipelineBarrier2(depInfo);
			}
		}

		void MasterRenderer::Composite(vk::CommandBuffer cmb, std::array<ParticipatingRenderer, s_MaxSceneRendererCount>& participants, const uint32_t participantCount, const uint32_t emptySceneCount, const Mode mode, const SceneRendererHandle requestedHandle) {
			CORI_VK_LABEL_F(cmb, DebugLabelColors::Composite, "Composite {} scene(s)", participantCount);

			switch (mode) {
			case Mode::eDirectBlit:
				{
					if (participantCount == 0 || emptySceneCount == participantCount) {
						CORI_VK_LABEL_INSERT(cmb, "Swapchain image -> PresentSrcKHR (skip)", DebugLabelColors::Barrier);

						vk::ImageMemoryBarrier2 scBar{
								.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
								.srcAccessMask = vk::AccessFlagBits2::eNone,
								.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
								.dstAccessMask = vk::AccessFlagBits2::eNone,
								.oldLayout = vk::ImageLayout::eUndefined,
								.newLayout = vk::ImageLayout::ePresentSrcKHR,
								.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
								.image = VulkanEngine::GetSwapChainImage(),
								.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
							};

						vk::DependencyInfo depInfo{
								.imageMemoryBarrierCount = 1,
								.pImageMemoryBarriers = &scBar
							};

						cmb.pipelineBarrier2(depInfo);
						break;
					}

					ParticipatingRenderer* participant = nullptr;
					for (uint32_t i = 0; i < participantCount; i++) {
						if (participants[i].handle == requestedHandle) {
							participant = &participants[i];
						}
					}

					if (participant == nullptr || participant->sceneEmpty) {
						CORI_VK_LABEL_INSERT(cmb, "Swapchain image -> PresentSrcKHR (skip)", DebugLabelColors::Barrier);

						vk::ImageMemoryBarrier2 scBar{
							.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							.srcAccessMask = vk::AccessFlagBits2::eNone,
							.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
							.dstAccessMask = vk::AccessFlagBits2::eNone,
							.oldLayout = vk::ImageLayout::eUndefined,
							.newLayout = vk::ImageLayout::ePresentSrcKHR,
							.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.image = VulkanEngine::GetSwapChainImage(),
							.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
						};

						vk::DependencyInfo depInfo{
							.imageMemoryBarrierCount = 1,
							.pImageMemoryBarriers = &scBar
						};

						cmb.pipelineBarrier2(depInfo);
						break;
					}

					SceneRenderer* chosen = participant->ptr;

					{
						CORI_VK_LABEL_INSERT(cmb, "Swapchain image -> TransferDstOptimal", DebugLabelColors::Barrier);

						vk::ImageMemoryBarrier2 scBar{
							.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							.srcAccessMask = vk::AccessFlagBits2::eNone,
							.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
							.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
							.oldLayout = vk::ImageLayout::eUndefined,
							.newLayout = vk::ImageLayout::eTransferDstOptimal,
							.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.image = VulkanEngine::GetSwapChainImage(),
							.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
						};

						vk::DependencyInfo depInfo{
							.imageMemoryBarrierCount = 1,
							.pImageMemoryBarriers = &scBar
						};

						cmb.pipelineBarrier2(depInfo);
					}

					vk::Extent2D prtExtent = { chosen->GetPRT().GetImage().m_Extent3D.width, chosen->GetPRT().GetImage().m_Extent3D.height };
					vk::Extent2D scExtent = VulkanEngine::GetSwapChainExtent();

					std::array srcOffsets = { vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ static_cast<int32_t>(prtExtent.width), static_cast<int32_t>(prtExtent.height), 1 } };
					std::array dstOffsets = { vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ static_cast<int32_t>(scExtent.width), static_cast<int32_t>(scExtent.height), 1 } };

					vk::ImageBlit blit{
						.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
						.srcOffsets = srcOffsets,
						.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
						.dstOffsets = dstOffsets
					};

					CORI_VK_LABEL_INSERT_F(cmb, DebugLabelColors::Composite, "Blit PRT {}x{} -> swapchain {}x{}", prtExtent.width, prtExtent.height, scExtent.width, scExtent.height);

					cmb.blitImage(chosen->GetPRT().GetImage().m_Image, vk::ImageLayout::eTransferSrcOptimal, VulkanEngine::GetSwapChainImage(), vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

					{
						CORI_VK_LABEL_INSERT(cmb, "Swapchain image -> PresentSrcKHR", DebugLabelColors::Barrier);

						vk::ImageMemoryBarrier2 scBar{
							.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
							.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
							.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
							.dstAccessMask = vk::AccessFlagBits2::eNone,
							.oldLayout = vk::ImageLayout::eTransferDstOptimal,
							.newLayout = vk::ImageLayout::ePresentSrcKHR,
							.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.image = VulkanEngine::GetSwapChainImage(),
							.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
						};

						vk::DependencyInfo depInfo{
							.imageMemoryBarrierCount = 1,
							.pImageMemoryBarriers = &scBar
						};

						cmb.pipelineBarrier2(depInfo);
					}

					break;
				}
			case Mode::eHybrid:
				{
					if (participantCount == 0 || emptySceneCount == participantCount) {
						CORI_VK_LABEL_INSERT(cmb, "Swapchain image -> PresentSrcKHR (skip)", DebugLabelColors::Barrier);

						vk::ImageMemoryBarrier2 scBar{
							.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							.srcAccessMask = vk::AccessFlagBits2::eNone,
							.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
							.dstAccessMask = vk::AccessFlagBits2::eNone,
							.oldLayout = vk::ImageLayout::eUndefined,
							.newLayout = vk::ImageLayout::ePresentSrcKHR,
							.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.image = VulkanEngine::GetSwapChainImage(),
							.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
						};

						vk::DependencyInfo depInfo{
							.imageMemoryBarrierCount = 1,
							.pImageMemoryBarriers = &scBar
						};

						cmb.pipelineBarrier2(depInfo);
						break;
					}


					ParticipatingRenderer* participant = nullptr;
					for (uint32_t i = 0; i < participantCount; i++) {
						if (participants[i].handle == requestedHandle) {
							participant = &participants[i];
						}
					}

					if (participant == nullptr || participant->sceneEmpty) {
						CORI_VK_LABEL_INSERT(cmb, "Swapchain image -> PresentSrcKHR (skip)", DebugLabelColors::Barrier);

						vk::ImageMemoryBarrier2 scBar{
							.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							.srcAccessMask = vk::AccessFlagBits2::eNone,
							.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
							.dstAccessMask = vk::AccessFlagBits2::eNone,
							.oldLayout = vk::ImageLayout::eUndefined,
							.newLayout = vk::ImageLayout::ePresentSrcKHR,
							.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.image = VulkanEngine::GetSwapChainImage(),
							.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
						};

						vk::DependencyInfo depInfo{
							.imageMemoryBarrierCount = 1,
							.pImageMemoryBarriers = &scBar
						};

						cmb.pipelineBarrier2(depInfo);
						break;
					}

					SceneRenderer* chosen = participant->ptr;

					{
						CORI_VK_LABEL_INSERT(cmb, "Swapchain image -> TransferDstOptimal", DebugLabelColors::Barrier);

						vk::ImageMemoryBarrier2 scBar{
							.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							.srcAccessMask = vk::AccessFlagBits2::eNone,
							.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
							.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
							.oldLayout = vk::ImageLayout::eUndefined,
							.newLayout = vk::ImageLayout::eTransferDstOptimal,
							.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.image = VulkanEngine::GetSwapChainImage(),
							.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
						};

						vk::DependencyInfo depInfo{
							.imageMemoryBarrierCount = 1,
							.pImageMemoryBarriers = &scBar
						};

						cmb.pipelineBarrier2(depInfo);
					}

					vk::Extent2D prtExtent = { chosen->GetPRT().GetImage().m_Extent3D.width, chosen->GetPRT().GetImage().m_Extent3D.height };
					vk::Extent2D scExtent = VulkanEngine::GetSwapChainExtent();

					std::array srcOffsets = { vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ static_cast<int32_t>(prtExtent.width), static_cast<int32_t>(prtExtent.height), 1 } };
					std::array dstOffsets = { vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ static_cast<int32_t>(scExtent.width), static_cast<int32_t>(scExtent.height), 1 } };

					vk::ImageBlit blit{
						.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
						.srcOffsets = srcOffsets,
						.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
						.dstOffsets = dstOffsets
					};

					CORI_VK_LABEL_INSERT_F(cmb, DebugLabelColors::Composite, "Blit PRT {}x{} -> swapchain {}x{}", prtExtent.width, prtExtent.height, scExtent.width, scExtent.height);

					cmb.blitImage(chosen->GetPRT().GetImage().m_Image, vk::ImageLayout::eTransferSrcOptimal, VulkanEngine::GetSwapChainImage(), vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eNearest);

					{
						CORI_VK_LABEL_INSERT(cmb, "Swapchain image -> eColorAttachmentOutput", DebugLabelColors::Barrier);

						vk::ImageMemoryBarrier2 scBar{
							.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
							.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
							.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
							.oldLayout = vk::ImageLayout::eTransferDstOptimal,
							.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
							.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.image = VulkanEngine::GetSwapChainImage(),
							.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
						};

						vk::DependencyInfo depInfo{
							.imageMemoryBarrierCount = 1,
							.pImageMemoryBarriers = &scBar
						};

						cmb.pipelineBarrier2(depInfo);
					}

					vk::RenderingAttachmentInfo colorAttachment = {
						.imageView = VulkanEngine::GetSwapChainImageView(),
						.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
						.loadOp = vk::AttachmentLoadOp::eLoad,
						.storeOp = vk::AttachmentStoreOp::eStore
					};

					vk::RenderingInfo renderInfo = {
						.renderArea = {{0, 0}, VulkanEngine::GetSwapChainExtent()},
						.layerCount = 1,
						.colorAttachmentCount = 1,
						.pColorAttachments = &colorAttachment
					};

					cmb.beginRendering(renderInfo);

					ImGuiRenderer::Render(cmb);

					cmb.endRendering();

					{
						CORI_VK_LABEL_INSERT(cmb, "Swapchain image -> PresentSrcKHR", DebugLabelColors::Barrier);

						vk::ImageMemoryBarrier2 scBar{
							.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
							.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
							.dstAccessMask = vk::AccessFlagBits2::eNone,
							.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
							.newLayout = vk::ImageLayout::ePresentSrcKHR,
							.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.image = VulkanEngine::GetSwapChainImage(),
							.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
						};

						vk::DependencyInfo depInfo{
							.imageMemoryBarrierCount = 1,
							.pImageMemoryBarriers = &scBar
						};

						cmb.pipelineBarrier2(depInfo);
					}

					break;
				}
			case Mode::eDockSpace:
				{
					std::array<vk::ImageMemoryBarrier2, s_MaxSceneRendererCount> prtBarriers{};
					uint32_t counter = 0;

					for (uint32_t i = 0; i < participantCount; i++) {
						if (participants[i].sceneEmpty) {
							continue;
						}

						prtBarriers[counter] = vk::ImageMemoryBarrier2{
							.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput | vk::PipelineStageFlagBits2::eTransfer,
							.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eTransferWrite,
							.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
							.dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
							.oldLayout = vk::ImageLayout::eTransferSrcOptimal,
							.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
							.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.image = participants[i].ptr->GetPRT().GetImage().m_Image,
							.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
						};

						counter++;
					}

					if (counter > 0) {
						CORI_VK_LABEL_INSERT(cmb, "Scene PRTs -> ShaderReadOnlyOptimal", DebugLabelColors::Barrier);

						vk::DependencyInfo depInfo{
							.imageMemoryBarrierCount = counter,
							.pImageMemoryBarriers = prtBarriers.data()
						};

						cmb.pipelineBarrier2(depInfo);
					}

					{
						CORI_VK_LABEL_INSERT(cmb, "Swapchain image -> eColorAttachmentOutput", DebugLabelColors::Barrier);

						vk::ImageMemoryBarrier2 scBar{
							.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							.srcAccessMask = vk::AccessFlagBits2::eNone,
							.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
							.oldLayout = vk::ImageLayout::eUndefined,
							.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
							.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.image = VulkanEngine::GetSwapChainImage(),
							.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
						};

						vk::DependencyInfo depInfo{
							.imageMemoryBarrierCount = 1,
							.pImageMemoryBarriers = &scBar
						};

						cmb.pipelineBarrier2(depInfo);
					}

					vk::RenderingAttachmentInfo colorAttachment = {
						.imageView = VulkanEngine::GetSwapChainImageView(),
						.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
						.loadOp = vk::AttachmentLoadOp::eDontCare,
						.storeOp = vk::AttachmentStoreOp::eStore
					};

					vk::RenderingInfo renderInfo = {
						.renderArea = {{0, 0}, VulkanEngine::GetSwapChainExtent()},
						.layerCount = 1,
						.colorAttachmentCount = 1,
						.pColorAttachments = &colorAttachment
					};

					cmb.beginRendering(renderInfo);

					ImGuiRenderer::Render(cmb);

					cmb.endRendering();

					{
						CORI_VK_LABEL_INSERT(cmb, "Swapchain image -> PresentSrcKHR", DebugLabelColors::Barrier);

						vk::ImageMemoryBarrier2 scBar{
							.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
							.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
							.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
							.dstAccessMask = vk::AccessFlagBits2::eNone,
							.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
							.newLayout = vk::ImageLayout::ePresentSrcKHR,
							.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
							.image = VulkanEngine::GetSwapChainImage(),
							.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
						};

						vk::DependencyInfo depInfo{
							.imageMemoryBarrierCount = 1,
							.pImageMemoryBarriers = &scBar
						};

						cmb.pipelineBarrier2(depInfo);
					}

					break;
				}
			}
		}

	}
}