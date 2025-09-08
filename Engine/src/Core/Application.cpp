#include "Application.hpp"
#include "Input.hpp"
#include "Graphics/API.hpp"
#include "WorldSystem/SceneManager.hpp"
#include "WorldSystem/Components.hpp"
#include "AssetManager/AssetManager.hpp"
#include "EventSystem/Event.hpp"
#include "EventSystem/AppEvent.hpp"
#include "EventSystem/KeyEvent.hpp"

namespace Cori {
	namespace Core {
		Application* Application::s_Instance{ nullptr };

		Application::Application(const char* windowName) {
			CORI_CORE_ASSERT(!s_Instance, "Trying to construct application for the second time. Application already exists!");
			s_Instance = this;

			m_Window = Window::Create(windowName, false);

			m_Window->SetEventCallback(CORI_BIND_EVENT_FN(Application::OnEvent, CORI_PLACEHOLDERS(1)));
			m_Window->SetVSync(false);

			m_ImGuiLayer = new ImGuiLayer();

			m_LayerStack.PushOverlay(m_ImGuiLayer);

			AssetManager::Init();
			World::SceneManager::Init();
			Graphics::API::Init();
			Audio::Mixer::Init();

			m_GameTimer.SetTickrate(60);
			m_GameTimer.SetTickrateUpdateFunc(CORI_BIND_EVENT_FN(Application::TickrateUpdate, CORI_PLACEHOLDERS(1)));
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self }, "Cori Engine started.");
		}

		Application::~Application() {
			m_LayerStack.ClearStack();
			AssetManager::Shutdown();
			World::SceneManager::Shutdown();
			Graphics::API::Shutdown();
			Audio::Mixer::Shutdown();
		}

		void Application::OnEvent(Event& event) {
			EventDispatcher dispatcher(event);
			dispatcher.Dispatch<WindowCloseEvent>(CORI_BIND_EVENT_FN(Application::OnWindowClose));


#ifdef DEBUG_BUILD
			dispatcher.Dispatch<KeyReleasedEvent>([](const KeyReleasedEvent& e) -> bool {
				if (e.GetKeyCode() == CORI_KEY_F8) {
					CORI_PROFILE_REQUEST_NEXT_FRAME();
				}
				return false;
			});
#endif
			dispatcher.Dispatch<KeyReleasedEvent>([this](const KeyReleasedEvent& e) -> bool {
				if (e.GetKeyCode() == CORI_KEY_F9) {
					m_RenderImGui = !m_RenderImGui;
				}
				return false;
			});
			dispatcher.Dispatch<WindowResizeEvent>([](const WindowResizeEvent& e) -> bool {
				Graphics::API::SetViewport(0, 0, e.GetWidth(), e.GetHeight());
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

		void Application::PopLayer(Layer* layer) {
			s_Instance->m_LayerStack.PopLayerToQueue(layer);
		}

		void Application::PopOverlay(Layer* overlay) {
			s_Instance->m_LayerStack.PopOverlayToQueue(overlay);
		}

		void Application::SetBackgroundColor(const glm::vec4& color) {
			s_Instance->m_BackgroundColor = color;
		}

		void Application::SetManualTickStep(const bool state) {
			s_Instance->m_ManualStep = state;
		}

		void Application::Run() {
			while(m_Running) {
				CORI_PROFILER_FRAME_START();
				{
					CORI_PROFILE_SCOPE("Cori Engine Global Update");
					m_GameTimer.Update();

					Graphics::API::SetClearColor(m_BackgroundColor);
					Graphics::API::ClearFramebuffer();

					for (Layer* layer : m_LayerStack) {
						layer->OnUpdate(m_GameTimer);
						layer->SceneUpdate(m_GameTimer.GetDeltaTime());
						if (layer->IsModal()) {
							break;
						}
					}

					m_ImGuiLayer->StartFrame();

					if (m_RenderImGui) {
						for (Layer* layer : m_LayerStack) {
							layer->OnImGuiRender(m_GameTimer.GetDeltaTime());
						}
					}

					m_ImGuiLayer->EndFrame();

					m_Window->OnUpdate();

					m_LayerStack.ProcessQueue();
				}
				CORI_PROFILER_FRAME_END();
			}
		}



		void Application::TickrateUpdate(const float timeStep) {
			//static uint64_t ti = 0;
			if (m_ManualStep) {
				static bool oneshot = true;
				if (Input::IsKeyPressed(CORI_KEY_K)) {
					if (oneshot) {
						oneshot = false;
						for (Layer* layer : m_LayerStack) {
							layer->SceneTickrateUpdate(timeStep);
							layer->OnTickUpdate(timeStep);
						}
						//ti++;
						//CORI_CORE_DEBUG("TICK {}", ti);
					}
				}
				else {
					oneshot = true;
				}

			} else {
				for (Layer* layer : m_LayerStack) {
					layer->SceneTickrateUpdate(timeStep);
					layer->OnTickUpdate(timeStep);

				}
				//ti++;
				//CORI_CORE_DEBUG("TICK {}", ti);
			}
		}

		bool Application::OnWindowClose() {
			m_Running = false;
			return true;
		}
	}
}