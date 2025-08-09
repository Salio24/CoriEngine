#include "Application.hpp"
#include "Engine.hpp"
#include "WindowImpl.hpp"
#include "Input.hpp"
#include <imgui.h>
#include "Renderer/Buffers.hpp"
#include "Renderer/Renderer2D.hpp"
#include "Renderer/GraphicsCall.hpp"
#include "AssetManager/AssetDefinitions.hpp"
#include "WorldSystem/SceneManager.hpp"
#include "WorldSystem/Components.hpp"
#include "EventSystem/GameEvents.hpp"
#include "Audio/Mixer.hpp"

namespace Cori {
	Application* Application::s_Instance = nullptr;

	Application::Application() {
		CORI_CORE_ASSERT_FATAL(!s_Instance, "Trying to construct application for the second time. Application already exists!");
		s_Instance = this;

		m_Window = Window::Create();

		m_Window->SetEventCallback(CORI_BIND_EVENT_FN(Application::OnEvent, CORI_PLACEHOLDERS(1)));
		m_Window->SetVSync(false);

		m_ImGuiLayer = new ImGuiLayer();

		m_LayerStack.PushOverlay(m_ImGuiLayer);

		GraphicsCall::InitRenderers();
		Audio::Mixer::Init();

		m_GameTimer.SetTickrate(60);
		m_GameTimer.SetTickrateUpdateFunc(CORI_BIND_EVENT_FN(Application::TickrateUpdate, CORI_PLACEHOLDERS(1)));
	}

	Application::~Application() {
		GraphicsCall::ShutdownRenderers();
		Audio::Mixer::Shutdown();
	}

	void Application::OnEvent(Event& e) {
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(CORI_BIND_EVENT_FN(Application::OnWindowClose));


#ifdef DEBUG_BUILD
		dispatcher.Dispatch<KeyReleasedEvent>([](const KeyReleasedEvent& e) -> bool {
			if (e.GetKeyCode() == Cori::CORI_KEY_F8) {
				CORI_PROFILE_REQUEST_NEXT_FRAME();
			}
			return false;
		});
#endif
		dispatcher.Dispatch<KeyReleasedEvent>([this](const KeyReleasedEvent& e) -> bool {
			if (e.GetKeyCode() == Cori::CORI_KEY_F9) {
				m_RenderImGui = !m_RenderImGui;
			}
			return false;
		});

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
			--it;
			(*it)->OnEvent(e);
			if (e.m_Handeled || (*it)->IsModal()) {
				break;
			}
		}
	}

	void Application::PushLayer(Layer* layer) {
		Get().m_LayerStack.PushLayerToQueue(layer);
	}

	void Application::PushOverlay(Layer* layer) {
		Get().m_LayerStack.PushOverlayToQueue(layer);
	}

	void Application::Run() {
		while(m_Running) {
			CORI_PROFILER_FRAME_START();
			{
				CORI_PROFILE_SCOPE("Cori Engine Global Update");
				m_GameTimer.Update();

				GraphicsCall::SetClearColor({ 0.5f, 0.5f, 0.0f, 1.0f });
				GraphicsCall::ClearFramebuffer();

				for (Layer* layer : m_LayerStack) {
					layer->OnUpdate(m_GameTimer.m_DeltaTime, m_GameTimer.m_TickAlpha);
					layer->SceneUpdate(m_GameTimer.m_DeltaTime);
					if (layer->IsModal()) {
						break;
					}
				}

				m_ImGuiLayer->StartFrame();

				if (m_RenderImGui) {
					for (Layer* layer : m_LayerStack) {
						layer->OnImGuiRender(m_GameTimer.m_DeltaTime);
						
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
			test123++;
		for (Layer* layer : m_LayerStack) {
			layer->SceneTickrateUpdate(timeStep);
			layer->OnTickUpdate(timeStep);
		}
	}

	bool Application::OnWindowClose() {
		m_Running = false;
		return true;
	}
}