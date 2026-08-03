#pragma once
#include "SceneRenderer.hpp"
#include "FrameData.hpp"
#include "Graphics/RenderThreadCommandQueue.hpp"
#include "Graphics/RenderThreadWakeup.hpp"
#include "Core/Threading/CpuTopology.hpp"

namespace Cori {
	namespace Graphics {
		using SceneRendererHandle = uint32_t;

		struct MasterFrameData {
			FrameLatencyStamps latencyStamps{};

			void Clear() {
				latencyStamps = {};
			}
		};

		class MasterRenderer {
			struct PendingCreation {
				SceneRendererHandle handle;
				SceneRenderer::CreateInfo creationInfo;
				uint32_t generation;
			};
		public:
			enum class Mode {
				eDirectBlit,
				eHybrid,
				eDockSpace
			};

			static void Init();

			static void Shutdown();

			static MasterRenderer& Get();

			SceneRendererHandle CreateSceneRenderer(SceneRenderer::CreateInfo&& info) {
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

			void DestroySceneRenderer(SceneRendererHandle handle) {
				{
					std::lock_guard lk(m_QueueMutex);
					m_Generation[handle].fetch_add(1, std::memory_order_release);
					m_PendingDestructions.emplace_back(m_SceneRenderers[handle].load(std::memory_order_acquire));
					m_SceneRenderers[handle].store(nullptr, std::memory_order_release);
					m_FreeList.push_back(handle);
				}

				RenderThreadWakeup::Wake();
			}

			SceneRenderer* Resolve(SceneRendererHandle handle) {
				CORI_CORE_ASSERT(handle < s_MaxSceneRendererCount, "Invalid scene renderer handle passed to MasterRenderer::Resolve.");
				return m_SceneRenderers[handle].load(std::memory_order_acquire);
			}

			[[nodiscard]] MasterFrameData* PopRecycledFrameData() {
				if (m_ReadyRing.Size() < GetAdmitDepth()) {
					MasterFrameData** ptr = m_RecycleRing.Front();
					if (ptr) {
						m_RecycleRing.Pop();
						return *ptr;
					}
				}

				return nullptr;
			}

			void WaitForRecycledFrameData() {
				MasterFrameData* result = nullptr;
				while (result == nullptr) {
					if (m_ReadyRing.Size() < GetAdmitDepth()) {
						MasterFrameData** ptr = m_RecycleRing.Front();
						if (ptr) {
							result = *ptr;
						}
					}
				}
			}

			bool PushFrameData(MasterFrameData* frameData) {
				bool result = m_ReadyRing.TryEmplace(frameData);
				RenderThreadWakeup::Wake();
				return result;
			}

			static void ChangeCompositeMode(const Mode newMode) {
				Get().m_CurrentMode.store(newMode, std::memory_order_release);
			}

			static void ChangeMainRenderer(const SceneRendererHandle newRenderer) {
				Get().m_MainRenderer.store(newRenderer, std::memory_order_release);
			}

			~MasterRenderer() {
				RenderThreadCommandQueue::Clear();
				m_PendingCreations.clear();
				ProcessPendingSceneRendererDestructions();

				for (auto& renderer : m_SceneRenderers) {
					delete renderer.load(std::memory_order_relaxed);
				}

			}
		protected:
			friend VulkanEngine;
			friend SceneRenderer;
			void EnterThreadedMode() {
				RenderThreadCommandQueue::ClearExecuterThreadId();
				m_RenderThread = std::move(std::thread([this] {
					RtTask();
				}));
			}

			void ExitThreadedMode() {
				m_Running.store(false, std::memory_order_release);
				RenderThreadWakeup::Wake();
				m_RenderThread.join();

				RenderThreadCommandQueue::SetExecuterThreadId(std::this_thread::get_id());
				RenderThreadCommandQueue::DrainOnRenderThread();
			}

			constexpr static uint32_t GetAdmitDepth() {
				return 1;
			}

		private:
			void RtTask() {
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

					{
						CORI_PROFILE_SCOPE("Master Frame");
						if (TryRunFrame()) {
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

			static constexpr uint32_t s_MaxSceneRendererCount{ 16 };

			MasterRenderer() {
				for (uint32_t i = 0; i < s_MaxSceneRendererCount; i++) {
					m_FreeList.emplace_back(i);
				}

				for (auto& inst : m_FrameDataStorage) {
					m_RecycleRing.Emplace(&inst);
				}
			}

			void ProcessPendingSceneRendererCreations() {
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

			void ProcessPendingSceneRendererDestructions() {
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

			bool HasNonDormantScene() {
				for (auto& render : m_SceneRenderers) {
					auto* raw = render.load(std::memory_order_relaxed);
					if (raw) {
						if (!raw->IsDormant()) {
							return true;
						}
					}
				}

				return false;
			}

			bool TryRunFrame() {
				uint64_t maxWatermark = 0;
				FrameLatencyStamps latencyStamps{};

				std::array<std::pair<SceneRenderer*, SceneRendererHandle>, s_MaxSceneRendererCount> nonDormant;
				nonDormant.fill({ nullptr, UINT32_MAX});
				uint32_t nonDormantCounter = 0;
				SceneRendererHandle handleCounter = 0;

				for (auto& renderer : m_SceneRenderers) {
					SceneRenderer* ptr = renderer.load(std::memory_order_relaxed);
					if (ptr) {
						FrameData** dataPtr = ptr->PeekFrameData();
						if (!dataPtr) {
							if (ptr->IsDormant()) {
								continue;
							}

							return false;
						}

						ptr->MarkNonDormant();
						maxWatermark = std::max(maxWatermark, (*dataPtr)->rtcqWatermark);

						nonDormantCounter++;
						nonDormant[nonDormantCounter - 1].first = ptr;
						nonDormant[nonDormantCounter - 1].second = handleCounter;
					}
					handleCounter++;
				}

				bool ghostFrame = false;
				if (nonDormantCounter == 0) {
					if (m_ReadyRing.Front()) {
						ghostFrame = true;
					}
					else {
						return false;
					}
				}

				if (RenderThreadCommandQueue::DrainedCount() < maxWatermark) {
					return false;
				}

				VulkanPresentTiming::ThrottlePresentQueue();

				MasterFrameData** frameData = m_ReadyRing.Front();
				if (!frameData) {
					return false;
				}

				m_ReadyRing.Pop();

				ImGuiRenderer::ProcessTexQueueRequests();

				const FrameLatencyStamps& sceneStamps = (*frameData)->latencyStamps;

				if (sceneStamps.inputTimestampSdl != 0) {
					latencyStamps.inputTimestampSdl = sceneStamps.inputTimestampSdl;
				}

				if (sceneStamps.frameStartHost != 0) {
					latencyStamps.frameStartHost = sceneStamps.frameStartHost;
				}

				SceneRendererHandle requestedHandle = m_MainRenderer.load(std::memory_order_acquire);
				Mode mode = m_CurrentMode.load(std::memory_order_acquire);

				(*frameData)->Clear();
				m_RecycleRing.Emplace(*frameData);

				if (nonDormantCounter == 0 && !ghostFrame) {
					return false;
				}

				for (uint32_t i = 0; i < nonDormantCounter; i++) {
					SceneRenderer* ptr = nonDormant[i].first;
					ptr->ProcessFrameData();
				}

				static std::vector<SceneRenderer::FrameContext> frameContexts;
				frameContexts.clear();

				VulkanEngine::Get().CPUFrameStart();
				for (uint32_t i = 0; i < nonDormantCounter; i++) {
					SceneRenderer* ptr = nonDormant[i].first;
					frameContexts.push_back(ptr->Stage1());
				}

				auto& frameInfo = VulkanEngine::Get().GPUFrameBegin();
				if (!ghostFrame && !frameInfo.m_SkippedFrame) {
					DeletionQueue::Flush();
				}

				if (!frameInfo.m_SkippedFrame) {
					for (uint32_t i = 0; i < nonDormantCounter; i++) {
						SceneRenderer* ptr = nonDormant[i].first;
						ptr->Stage2(frameInfo, frameContexts[i]);
					}

					VulkanEngine::Get().GPUFrameMiddlePointSync();

					for (uint32_t i = 0; i < nonDormantCounter; i++) {
						SceneRenderer* ptr = nonDormant[i].first;
						ptr->Stage3(frameInfo, frameContexts[i]);
					}

					Composite(frameInfo.m_CommandBuffer, nonDormant, nonDormantCounter, mode, requestedHandle);
				}

				ImGuiRenderer::RecycleSnapshot();

				VulkanEngine::Get().GPUFrameEnd(latencyStamps);
				return true;
			}


			void Composite(vk::CommandBuffer cmb, std::array<std::pair<SceneRenderer*, SceneRendererHandle>, s_MaxSceneRendererCount>& nonDormantRenderers, const uint32_t nonDormantRendererCount, const Mode mode, const SceneRendererHandle requestedHandle) {
				CORI_VK_LABEL_F(cmb, DebugLabelColors::Composite, "Composite {} scene(s)", nonDormantRendererCount);

				switch (mode) {
				case Mode::eDirectBlit:
					{
						if (nonDormantRendererCount == 0) {
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

						SceneRenderer* chosen = nullptr;
						for (uint32_t i = 0; i < nonDormantRendererCount; i++) {
							if (nonDormantRenderers[i].second == requestedHandle) {
								chosen = nonDormantRenderers[i].first;
							}
						}

						if (chosen == nullptr) {
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
						if (nonDormantRendererCount == 0) {
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

						SceneRenderer* chosen = nullptr;
						for (uint32_t i = 0; i < nonDormantRendererCount; i++) {
							if (nonDormantRenderers[i].second == requestedHandle) {
								chosen = nonDormantRenderers[i].first;
							}
						}

						if (chosen == nullptr) {
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
				}
			}

			std::array<std::atomic<SceneRenderer*>, s_MaxSceneRendererCount> m_SceneRenderers{ nullptr };
			std::array<std::atomic<uint32_t>, s_MaxSceneRendererCount> m_Generation{};
			std::atomic<Mode> m_CurrentMode{ Mode::eDirectBlit };
			std::atomic<SceneRendererHandle> m_MainRenderer{ UINT32_MAX };

			std::vector<PendingCreation> m_PendingCreations;
			std::vector<SceneRenderer*> m_PendingDestructions;
			std::deque<SceneRendererHandle> m_FreeList;
			std::mutex m_QueueMutex;

			std::thread m_RenderThread;
			std::atomic<bool> m_Running{ true };

			Threading::SPSCRing<MasterFrameData*> m_ReadyRing{ FRAMES_IN_FLIGHT };
			Threading::SPSCRing<MasterFrameData*> m_RecycleRing{ FRAMES_IN_FLIGHT };
			std::array<MasterFrameData, FRAMES_IN_FLIGHT> m_FrameDataStorage;


			static std::unique_ptr<MasterRenderer> s_Instance;
		};
	}
}
