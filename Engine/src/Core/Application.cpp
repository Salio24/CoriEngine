#include "Application.hpp"
#include "Graphics/API.hpp"
#include "WorldSystem/SceneManager.hpp"
#include "WorldSystem/Components.hpp"
#include "AssetManager/AssetManager.hpp"
#include "EventSystem/Event.hpp"
#include "EventSystem/AppEvent.hpp"
#include "EventSystem/KeyEvent.hpp"
#include "FileSystem/PathManager.hpp"
#include "Graphics/Renderer2D.hpp"
#include "../Graphics/Vulkan/Renderer/SceneRenderer.hpp"
#include "Core/AssetManager/AssetManager2.hpp"

//FIXME: remove include later
#include "Graphics/Vulkan/Renderer/MasterRenderer.hpp"

namespace Cori {
	namespace Core {
		Application* Application::s_Instance{ nullptr };

		Application::Application(const char* windowName) : m_WorkerPool(std::thread::hardware_concurrency() == 1 ? 1 : std::thread::hardware_concurrency() - 1) {
			//m_ManualStep = true;
			CORI_CORE_ASSERT(!s_Instance, "Trying to construct application for the second time. Application already exists!");
			s_Instance = this;

			FileSystem::PathManager::Get();

			m_Window = Window::Create(windowName, false);
			m_VulkanEngine = Graphics::VulkanEngine::Create(m_Window->GetNativeWindow(), true);

			m_Window->SetEventCallback(CORI_BIND_EVENT_FN(Application::OnEvent, CORI_PLACEHOLDERS(1)));
			m_Window->SetVSync(false);

			m_ImGuiLayer = new Internal::ImGuiLayer();
			m_LayerStack.PushOverlay(m_ImGuiLayer);

			AssetManager::Init();
			AssetManager2::Init();
			World::SceneManager::Init();
			Audio::Mixer::Init();

			m_GameTimer.SetTickrate(120);
			m_GameTimer.SetTickrateUpdateFunc(CORI_BIND_EVENT_FN(Application::TickrateUpdate, CORI_PLACEHOLDERS(1)));
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self }, "Cori Engine started.");
		}

		Application::~Application() {
			World::SceneManager::Shutdown();
			m_LayerStack.ClearStack();
			m_VulkanEngine.reset();
			Audio::Mixer::Shutdown();
			AssetManager2::Shutdown();
			AssetManager::Shutdown();
		}

		void Application::EmitEvent(Event& event) {
			s_Instance->OnEvent(event);
		}

		uint16_t Application::GetWorkerCount() {
			return s_Instance->m_WorkerPool.GetWorkerCount();
		}

		void Application::OnEvent(Event& event) {
			EventDispatcher dispatcher(event);
			dispatcher.Dispatch<WindowCloseEvent>(CORI_BIND_EVENT_FN(Application::OnWindowClose));

			dispatcher.Dispatch<KeyReleasedEvent>([this](const KeyReleasedEvent& e) -> bool {
				if (e.GetKeyCode() == CORI_KEY_F9) {
					m_RenderImGui = !m_RenderImGui;
				}
				return false;
			});
			dispatcher.Dispatch<WindowResizeEvent>([](const WindowResizeEvent& e) -> bool {
				Graphics::VulkanEngine::ReportWindowResize();
				//Graphics::Internal::API::SetViewport(0, 0, e.GetWidth(), e.GetHeight());
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
					m_GameTimer.Update();

					for (Layer* layer : m_LayerStack) {
						layer->OnUpdate(m_GameTimer);
						layer->SceneUpdate(m_GameTimer);
					}

					{
						CORI_PROFILE_SCOPE("ImGui Render");
						m_ImGuiLayer->StartFrame();

						if (m_RenderImGui) {
							for (Layer* layer : m_LayerStack) {
								layer->OnImGuiRender(m_GameTimer);
								layer->SceneImGuiRender(m_GameTimer);
								if (layer->IsModal()) {
									break;
								}
							}
						}

						m_ImGuiLayer->EndFrame();
					}

					{
						CORI_PROFILE_SCOPE("FrameData preparation");
						bool success = false;
						while (success == false) {
							success = true;

							for (Layer* layer : m_LayerStack) {
								success &= layer->ActiveScene.PrepareFrameData();
							}
						}
					}

					{
						CORI_PROFILE_SCOPE("Application submit to renderer");
						bool success = false;
						while (success == false) {
							success = true;

							for (Layer* layer : m_LayerStack) {
								success &= layer->ActiveScene.SubmitForRender();
							}
						}
					}

					Graphics::MasterRenderer::Get().Loop();

					//AssetManager2::OnUpdate(m_GameTimer);

					//Graphics::SceneRenderer::Get().Render();

					m_Window->OnUpdate();

					m_LayerStack.ProcessQueue();
				}
			}
		}

		void Application::TickrateUpdate(GameTimer& gameTimer) {
			CORI_PROFILE_SCOPE("Tick Update");

			for (Layer* layer : m_LayerStack) {
				layer->SceneTickrateUpdate(gameTimer);
				layer->OnTickUpdate(gameTimer);
				if (layer->IsModal()) {
					break;
				}
			}
		}

		bool Application::OnWindowClose() {
			m_Running = false;
			return true;
		}
	}
}