#include "MasterRenderer.hpp"
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

				{
					CORI_PROFILE_SCOPE("Master Frame");
					bool result = TryRunFrame();
					//CORI_DEBUG("TRF: {}", result);
					if (result) {
						RenderThreadCommandQueue::DrainOnRenderThread();
						continue;
					}
				}

				if (RenderThreadCommandQueue::DrainOnRenderThread() > 0) {
					//CORI_DEBUG("drained");
					continue;
				}

				if (RenderThreadWakeup::Snapshot() != wakeBefore) {
					//CORI_DEBUG("snapshot change");
					continue;
				}

				//CORI_DEBUG("nothing");
				RenderThreadWakeup::WaitChanged(wakeBefore);
			}
		}

		bool MasterRenderer::TryRunFrame() {
			uint64_t maxWatermark = 0;
			FrameLatencyStamps latencyStamps{};

			std::array<NonDormantSceneRenderer, s_MaxSceneRendererCount> nonDormant;
			nonDormant.fill({ nullptr, UINT32_MAX, std::nullopt });
			uint32_t nonDormantCounter = 0;
			SceneRendererHandle handleCounter = 0;
			RendererSettings settings{};

			for (auto& renderer : m_SceneRenderers) {
				SceneRenderer* ptr = renderer.load(std::memory_order_relaxed);
				if (ptr) {
					FrameData** dataPtr = ptr->PeekFrameData();
					if (!dataPtr) {
						if (ptr->IsDormant()) {
							continue;
						}

						//CORI_DEBUG("no frame data");
						return false;
					}

					ptr->MarkNonDormant();
					maxWatermark = std::max(maxWatermark, (*dataPtr)->rtcqWatermark);

					nonDormantCounter++;
					nonDormant[nonDormantCounter - 1].ptr = ptr;
					nonDormant[nonDormantCounter - 1].handle = handleCounter;
				}
				handleCounter++;
			}

			bool ghostFrame = false;
			if (nonDormantCounter == 0) {
				if (m_ReadyRing.Front()) {
					ghostFrame = true;
				}
				else {
					//CORI_DEBUG("all dormant and no MFD");
					return false;
				}
			}

			if (RenderThreadCommandQueue::DrainedCount() < maxWatermark) {
				return false;
			}

			VulkanPresentTiming::ThrottlePresentQueue();

			MasterFrameData** frameData = m_ReadyRing.Front();
			if (!frameData) {
				//CORI_DEBUG("no MFD ");
				return false;
			}

			m_ReadyRing.Pop();

			ImGuiRenderer::ProcessTexQueueRequests();

			const FrameLatencyStamps& sceneStamps = (*frameData)->latencyStamps;
			settings = (*frameData)->settings;

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
				//CORI_DEBUG("???? ");
				return false;
			}

			for (uint32_t i = 0; i < nonDormantCounter; i++) {
				SceneRenderer* ptr = nonDormant[i].ptr;
				ptr->ProcessFrameData();
			}

			uint32_t emptySceneCount = 0;

			VulkanEngine::Get().CPUFrameStart();
			for (uint32_t i = 0; i < nonDormantCounter; i++) {
				SceneRenderer* ptr = nonDormant[i].ptr;
				nonDormant[i].context = ptr->Stage1(settings);
				if (!nonDormant[i].context) {
					emptySceneCount++;
				}
			}

			auto& frameInfo = VulkanEngine::Get().GPUFrameBegin();
			if (settings.Wireframe) {
				frameInfo.m_CommandBuffer.setLineWidth(settings.WireframeLineWidth);
				frameInfo.m_CommandBuffer.setPolygonModeEXT(vk::PolygonMode::eLine);

			}

			if (!ghostFrame && !frameInfo.m_SkippedFrame) {
				DeletionQueue::Flush();
			}

			if (!frameInfo.m_SkippedFrame) {
				InitializeFreshPRTs(frameInfo.m_CommandBuffer);

				for (uint32_t i = 0; i < nonDormantCounter; i++) {
					if (!nonDormant[i].context) {
						continue;
					}
					SceneRenderer* ptr = nonDormant[i].ptr;
					ptr->Stage2(frameInfo, nonDormant[i].context.value());
				}

				VulkanEngine::Get().GPUFrameMiddlePointSync();

				for (uint32_t i = 0; i < nonDormantCounter; i++) {
					if (!nonDormant[i].context) {
						continue;
					}
					SceneRenderer* ptr = nonDormant[i].ptr;
					ptr->Stage3(frameInfo, nonDormant[i].context.value());
				}

				if (settings.Wireframe) {
					frameInfo.m_CommandBuffer.setPolygonModeEXT(vk::PolygonMode::eFill);
				}
				Composite(frameInfo.m_CommandBuffer, nonDormant, nonDormantCounter, emptySceneCount, mode, requestedHandle);
			}

			ImGuiRenderer::RecycleSnapshot();

			VulkanEngine::Get().GPUFrameEnd(latencyStamps);
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