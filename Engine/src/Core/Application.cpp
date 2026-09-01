#include "Application.hpp"
#include "WorldSystem/SceneManager.hpp"
#include "WorldSystem/Components.hpp"
#include "EventSystem/Event.hpp"
#include "EventSystem/AppEvent.hpp"
#include "EventSystem/KeyEvent.hpp"
#include "FileSystem/PathManager.hpp"
#include "../Graphics/Vulkan/Renderer/SceneRenderer.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "Core/Console/Console.hpp"
#include "Graphics/RendererSettings.hpp"

//FIXME: remove include later
#include "Graphics/Vulkan/Renderer/MasterRenderer.hpp"
#include "Threading/CpuTopology.hpp"

namespace Cori {
	namespace Core {
		Application* Application::s_Instance{ nullptr };

		Application::Application(const char* windowName) : m_WorkerPool(std::thread::hardware_concurrency() == 1 ? 1 : std::max(1u, std::thread::hardware_concurrency() - 4)) {
			CORI_CORE_ASSERT(!s_Instance, "Trying to construct application for the second time. Application already exists!");
			s_Instance = this;

			FileSystem::PathManager::Get();

			Console::Init();

			m_Window = Window::Create(windowName, false);
			Graphics::RenderThreadCommandQueue::SetExecuterThreadId(std::this_thread::get_id());

			if (Threading::CpuTopology::ShouldBind()) {
				Threading::CpuTopology::BindCurrentThreadToDomain(Threading::CpuTopology::PreferredDomain());
			}

			m_Window->SetEventCallback(std::bind(&Application::OnEvent, this , std::placeholders::_1));
			m_Window->SetVSync(false);

			ImGui::CreateContext();
			Graphics::VulkanEngine::Start(m_Window->GetNativeWindow(), { m_Window->GetPixelWidth(), m_Window->GetPixelHeight() });

			m_ImGuiLayer = new Internal::ImGuiLayer();
			m_LayerStack.PushOverlay(m_ImGuiLayer);

			AssetManager2::Init();
			World::SceneManager::Init();
			Audio::Mixer::Init();

			m_GameTimer.SetTickrate(120);
			m_GameTimer.SetTickrateUpdateFunc(std::bind(&Application::TickrateUpdate, this , std::placeholders::_1));

			Graphics::VulkanEngine::EnterThreadedMode();

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self }, "Cori Engine started.");
		}

		Application::~Application() {
			Console::Shutdown();
			Graphics::VulkanEngine::ExitThreadedMode();
			m_LayerStack.ClearStack();
			World::SceneManager::Shutdown();
			Audio::Mixer::Shutdown();
			m_WorkerPool.Stop();
			Graphics::VulkanEngine::Stop();
			ImGui::DestroyContext();
			AssetManager2::Shutdown();
		}

		void Application::EmitEvent(Event& event) {
			s_Instance->OnEvent(event);
		}

		uint16_t Application::GetWorkerCount() {
			return s_Instance->m_WorkerPool.GetWorkerCount();
		}

		void Application::OnEvent(Event& event) {
			EventDispatcher dispatcher(event);
			dispatcher.Dispatch<WindowCloseEvent>(std::bind(&Application::OnWindowClose, this  ));

			dispatcher.Dispatch<WindowPixelResizeEvent>([](const WindowPixelResizeEvent& e) -> bool {
				Graphics::VulkanEngine::ReportWindowResize({ e.GetWidth(), e.GetHeight() });
				return false;
			});

			for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
				--it;
				(*it)->OnEvent(event);
				if (event.m_Handled || (*it)->IsModal()) {
					break;
				}
			}
		}

		std::expected<void, CoriError<>> Application::PushLayer(Layer* layer) {
			return s_Instance->m_LayerStack.PushLayerToQueue(layer);
		}

		std::expected<void, CoriError<>> Application::PushOverlay(Layer* overlay) {
			return s_Instance->m_LayerStack.PushOverlayToQueue(overlay);
		}

		void Application::PopLayer() {
			s_Instance->m_LayerStack.PopLayerToQueue();
		}

		void Application::PopOverlay() {
			s_Instance->m_LayerStack.PopOverlayToQueue();
		}

		void Application::SetBackgroundColor(const glm::vec4& color) {
			s_Instance->m_BackgroundColor = color;
		}

		void Application::Run() {
			while(m_Running) {
				CORI_PROFILER_FRAME_START();
				{
					CORI_PROFILE_SCOPE("Cori Engine Global Update");
					m_CommandQueue.Execute();

					CORI_PROFILER_PLOT("Main thread CPU", Threading::CpuTopology::CurrentCpu());

					m_FramePacer.WaitForFrameStart();

					{
						CORI_PROFILE_SCOPE("MasterFrameData wait");
						Graphics::MasterRenderer::Get().BeginFrame();
					}

					m_FrameStartHostTime = Graphics::VulkanPresentTiming::HostNow();

					m_Window->OnUpdate();
					m_GameTimer.Update();
					AssetManager2::OnUpdate(m_GameTimer);

					{
						CORI_PROFILE_SCOPE("ImGui Render");
						m_ImGuiLayer->StartFrame();

						if (m_RenderImGui) {
							for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
								--it;
								(*it)->OnImGuiRender(m_GameTimer);
								if ((*it)->IsModal()) {
									break;
								}
							}
						}

						m_ImGuiLayer->EndFrame();
					}

					for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
						--it;
						(*it)->OnUpdate(m_GameTimer);
					}

					{
						CORI_PROFILE_SCOPE("FrameData preparation");
						for (auto& handle : World::SceneManager::GetStorage() | std::views::values) {
							handle.PrepareFrameData();
						}
					}

					{
						CORI_PROFILE_SCOPE("Application submit to renderer");
						for (auto& handle : World::SceneManager::GetStorage() | std::views::values) {
							handle.SubmitForRender();
						}
					}

					Graphics::MasterRenderer::Get().EndFrame(
						Graphics::FrameLatencyStamps{
							.inputTimestampSdl = m_Window->GetOldestInputTimestamp(),
							.frameStartHost = GetFrameStartHostTime()
						},
						Graphics::g_RendererSettings);

					m_LayerStack.ProcessQueue();
				}
			}
		}

		void Application::TickrateUpdate(GameTimer& gameTimer) {
			CORI_PROFILE_SCOPE("Tick Update");

			for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
				--it;
				(*it)->OnTickUpdate(gameTimer);
				if ((*it)->IsModal()) {
					break;
				}
			}
		}

		bool Core::Application::OnWindowClose() {
			m_Running = false;
			return true;
		}
	}
}
