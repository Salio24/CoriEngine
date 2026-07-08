#pragma once
#include "SceneRenderer.hpp"
#include "FrameData.hpp"
#include "Graphics/RenderThreadCommandQueue.hpp"
#include "Graphics/RenderThreadWakeup.hpp"

namespace Cori {
	namespace Graphics {
		using SceneRendererHandle = uint32_t;

		class MasterRenderer {
			struct PendingCreation {
				SceneRendererHandle handle;
				SceneRenderer::CreateInfo creationInfo;
				uint32_t generation;
			};
		public:
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

			~MasterRenderer() {
				//temporary
				ProcessPendingSceneRendererCreations();
				ProcessPendingSceneRendererDestructions();

				for (auto& renderer : m_SceneRenderers) {
					delete renderer.load(std::memory_order_relaxed);
				}
			}
		//protected:
			void Loop() {
				uint64_t wakeBefore = RenderThreadWakeup::Snapshot();

				ProcessPendingSceneRendererCreations();
				ProcessPendingSceneRendererDestructions();

				if (TryRunFrame()) {
					RenderThreadCommandQueue::DrainOnRenderThread();
					return;
				}

				if (RenderThreadCommandQueue::DrainOnRenderThread() > 0) {
					return;
				}

				if (RenderThreadWakeup::Snapshot() != wakeBefore) {
					return;
				}

				//if (HasNonDormantScene()) {
				//}
				
				RenderThreadWakeup::WaitChanged(wakeBefore);
			}

		private:
			static constexpr uint32_t s_MaxSceneRendererCount{ 16 };

			MasterRenderer() {
				for (uint32_t i = 0; i < s_MaxSceneRendererCount; i++) {
					m_FreeList.emplace_back(i);
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

				std::array<SceneRenderer*, s_MaxSceneRendererCount> nonDormant{ nullptr };
				uint32_t nonDormantCounter = 0;

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
						nonDormant[nonDormantCounter - 1] = ptr;
					}
				}

				if (nonDormantCounter != 0) {
					// check imgui snapshot gen, if the same, return, if diff, fall throught
				}

				if (RenderThreadCommandQueue::DrainedCount() < maxWatermark) {
					return false;
				}

				for (uint32_t i = 0; i < nonDormantCounter; i++) {
					SceneRenderer* ptr = nonDormant[i];
					ptr->ProcessFrameData();
				}

				static std::vector<SceneRenderer::FrameContext> frameContexts;
				frameContexts.clear();

				VulkanEngine::Get().CPUFrameStart();
				for (uint32_t i = 0; i < nonDormantCounter; i++) {
					SceneRenderer* ptr = nonDormant[i];
					frameContexts.push_back(ptr->Stage1());
				}

				auto& frameInfo = VulkanEngine::Get().GPUFrameBegin();

				if (!frameInfo.m_SkippedFrame) {
					for (uint32_t i = 0; i < nonDormantCounter; i++) {
						SceneRenderer* ptr = nonDormant[i];
						ptr->Stage2(frameInfo, frameContexts[i]);
					}

					VulkanEngine::Get().GPUFrameMiddlePointSync();

					for (uint32_t i = 0; i < nonDormantCounter; i++) {
						SceneRenderer* ptr = nonDormant[i];
						ptr->Stage3(frameInfo, frameContexts[i]);
					}

					Composite(frameInfo.m_CommandBuffer, nonDormant, nonDormantCounter);
				}


				VulkanEngine::Get().GPUFrameEnd();
				return true;
			}


			void Composite(vk::CommandBuffer cmb, std::array<SceneRenderer*, s_MaxSceneRendererCount>& nonDormantRenderers, const uint32_t nonDormantRendererCount) {
				{
					vk::ImageMemoryBarrier2 scBar{
						.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
						.srcAccessMask = vk::AccessFlagBits2::eNone,
						.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
						.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
						.oldLayout = vk::ImageLayout::eUndefined,
						.newLayout = vk::ImageLayout::eTransferDstOptimal,
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

				if (nonDormantRendererCount == 1) {
					auto* renderer = nonDormantRenderers[0];

					vk::Extent2D prtExtent = { renderer->GetPRT().GetImage().m_Extent3D.width, renderer->GetPRT().GetImage().m_Extent3D.height };
					vk::Extent2D scExtent = VulkanEngine::GetSwapChainExtent();

					std::array srcOffsets = { vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ static_cast<int32_t>(prtExtent.width), static_cast<int32_t>(prtExtent.height), 1 } };
					std::array dstOffsets = { vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ static_cast<int32_t>(scExtent.width), static_cast<int32_t>(scExtent.height), 1 } };

					vk::ImageBlit blit{
						.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
						.srcOffsets = srcOffsets,
						.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
						.dstOffsets = dstOffsets
					};

					cmb.blitImage(renderer->GetPRT().GetImage().m_Image, vk::ImageLayout::eTransferSrcOptimal, VulkanEngine::GetSwapChainImage(), vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eNearest);
				}

				{
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
			}

			std::array<std::atomic<SceneRenderer*>, s_MaxSceneRendererCount> m_SceneRenderers{ nullptr };
			std::array<std::atomic<uint32_t>, s_MaxSceneRendererCount> m_Generation{};

			std::vector<PendingCreation> m_PendingCreations;
			std::vector<SceneRenderer*> m_PendingDestructions;
			std::deque<SceneRendererHandle> m_FreeList;
			std::mutex m_QueueMutex;

			static std::unique_ptr<MasterRenderer> s_Instance;
		};
	}
}
