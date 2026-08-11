#pragma once
#include "SceneRenderer.hpp"
#include "Graphics/RenderThreadCommandQueue.hpp"
#include "Graphics/RenderThreadWakeup.hpp"
#include "Graphics/RendererSettings.hpp"

namespace Cori {
	namespace Graphics {
		using SceneRendererHandle = uint32_t;

		inline constexpr uint32_t s_MaxSceneRendererCount{ 16 };

		struct FrameParticipant {
			SceneRendererHandle handle{ 0 };
			uint32_t generation{ 0 };
		};

		struct MasterFrameData {
			FrameLatencyStamps latencyStamps{};
			RendererSettings settings{};
			std::array<FrameParticipant, s_MaxSceneRendererCount> participants{};
			uint32_t participantCount{ 0 };
			uint64_t rtcqWatermark{ 0 };

			void Clear() {
				latencyStamps = {};
				settings = {};
				participantCount = 0;
				rtcqWatermark = 0;
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

			void BeginFrame() {
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

			void AddParticipant(const SceneRendererHandle handle) {
				CORI_CORE_ASSERT(m_BuildingFrame, "MasterRenderer::AddParticipant was called outside of a BeginFrame/EndFrame pair.");
				CORI_CORE_ASSERT(handle < s_MaxSceneRendererCount, "Invalid scene renderer handle passed to MasterRenderer::AddParticipant.");

				m_BuildingFrame->participants[m_BuildingFrame->participantCount] = { handle, m_Generation[handle].load(std::memory_order_acquire) };
				m_BuildingFrame->participantCount++;

				CORI_CORE_ASSERT(m_BuildingFrame->participantCount <= s_MaxSceneRendererCount, "More participants than there are scene renderer slots.");
			}

			void EndFrame(const FrameLatencyStamps& latencyStamps, const RendererSettings& settings) {
				CORI_CORE_ASSERT(m_BuildingFrame, "MasterRenderer::EndFrame was called without a matching BeginFrame.");

				m_BuildingFrame->latencyStamps = latencyStamps;
				m_BuildingFrame->settings = settings;
				m_BuildingFrame->rtcqWatermark = RenderThreadCommandQueue::CurrentPushCount();

				[[maybe_unused]] const bool result = m_ReadyRing.TryEmplace(m_BuildingFrame);
				CORI_CORE_ASSERT(result, "MasterRenderer ready ring was full while publishing a frame.");

				m_BuildingFrame = nullptr;

				RenderThreadWakeup::Wake();
			}

			static void ChangeCompositeMode(const Mode newMode) {
				Get().m_CurrentMode.store(newMode, std::memory_order_release);
			}

			static void ChangeMainRenderer(const SceneRendererHandle newRenderer) {
				Get().m_MainRenderer.store(newRenderer, std::memory_order_release);
			}

			static void PushPRTForInitialTransition(vk::Image PRTimage) {
				Get().m_PRTInitialTransitionQueue.emplace_back(PRTimage);
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
				m_RenderThread = std::thread(&MasterRenderer::RtTask, this);
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
			struct ParticipatingRenderer {
				SceneRenderer* ptr{};
				SceneRendererHandle handle{};
				std::optional<SceneRenderer::FrameContext> context{};
				bool sceneEmpty{};
			};
			void RtTask();

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

			bool TryRunFrame();

			void InitializeFreshPRTs(vk::CommandBuffer cmb) {
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

			void Composite(vk::CommandBuffer cmb, std::array<ParticipatingRenderer, s_MaxSceneRendererCount>& participants, const uint32_t participantCount, const uint32_t emptySceneCount, const Mode mode, const SceneRendererHandle requestedHandle) {
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
			MasterFrameData* m_BuildingFrame{ nullptr };

			std::vector<vk::Image> m_PRTInitialTransitionQueue;


			static std::unique_ptr<MasterRenderer> s_Instance;
		};
	}
}
