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

			SceneRendererHandle CreateSceneRenderer(SceneRenderer::CreateInfo&& info);

			void DestroySceneRenderer(SceneRendererHandle handle);

			SceneRenderer* Resolve(SceneRendererHandle handle) {
				CORI_CORE_ASSERT(handle < s_MaxSceneRendererCount, "Invalid scene renderer handle passed to MasterRenderer::Resolve.");
				return m_SceneRenderers[handle].load(std::memory_order_acquire);
			}

			void BeginFrame();

			void AddParticipant(const SceneRendererHandle handle);

			void EndFrame(const FrameLatencyStamps& latencyStamps, const RendererSettings& settings);

			static void ChangeCompositeMode(const Mode newMode) {
				Get().m_CurrentMode.store(newMode, std::memory_order_release);
			}

			static void ChangeMainRenderer(const SceneRendererHandle newRenderer) {
				Get().m_MainRenderer.store(newRenderer, std::memory_order_release);
			}

			static void PushPRTForInitialTransition(vk::Image PRTimage) {
				Get().m_PRTInitialTransitionQueue.emplace_back(PRTimage);
			}

			~MasterRenderer();
		protected:
			friend VulkanEngine;
			friend SceneRenderer;
			void EnterThreadedMode() {
				RenderThreadCommandQueue::ClearExecuterThreadId();
				m_RenderThread = std::thread(&MasterRenderer::RtTask, this);
			}

			void ExitThreadedMode();

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

			MasterRenderer();

			void ProcessPendingSceneRendererCreations();

			void ProcessPendingSceneRendererDestructions();

			bool TryRunFrame();

			void InitializeFreshPRTs(vk::CommandBuffer cmb);

			void Composite(vk::CommandBuffer cmb, std::array<ParticipatingRenderer, s_MaxSceneRendererCount>& participants, const uint32_t participantCount, const uint32_t emptySceneCount, const Mode mode, const SceneRendererHandle requestedHandle);

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
