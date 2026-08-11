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
	}
}