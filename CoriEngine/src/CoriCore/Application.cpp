// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Application.hpp"
#include "Engine.hpp"
#include "WindowImpl.hpp"
#include "Input.hpp"
#include <imgui.h>
#include "Renderer/Buffers.hpp"
#include "Renderer/GraphicsCall.hpp"
#include "AssetManager/AssetDefinitions.hpp"
#include "ECS/SceneManager.hpp"
#include "ECS/TriggerManager.hpp"
#include "EventSystem/GameEvents.hpp"
#include "ECS/Components.hpp"

namespace Cori {
	Application* Application::s_Instance = nullptr;

	Application::Application() {
		CORI_CORE_ASSERT_FATAL(!s_Instance, "Trying to construct application for the second time. Application already exists!");
		s_Instance = this;

		m_Window = Window::Create();

		m_Window->SetEventCallback(CORI_BIND_EVENT_FN(Application::OnEvent, CORI_PLACEHOLDERS(1)));

		TriggerManager::Get().SetEventCallback(CORI_BIND_EVENT_FN(Application::OnEvent, CORI_PLACEHOLDERS(1)));

		m_ImGuiLayer = new ImGuiLayer();

		PushOverlay(m_ImGuiLayer);

		GraphicsCall::InitRenderers();

		m_GameTimer.SetTickrate(60);
		m_GameTimer.SetTickrateUpdateFunc(CORI_BIND_EVENT_FN(Application::TickrateUpdate, CORI_PLACEHOLDERS(1)));
	}

	Application::~Application() {
		GraphicsCall::ShutdownRenderers();
	}

	void Application::OnEvent(Event& e) {
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(CORI_BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<GameTriggerStayEvent>([](Cori::GameTriggerStayEvent& e) -> bool {
			return e.GetTriggerEntity().GetComponents<TriggerComponent>().TriggerScript(e.GetTriggerEntity(), e.GetOtherEntity(), e.m_EventCallback);
		});

#ifdef DEBUG_BUILD
		dispatcher.Dispatch<KeyReleasedEvent>([this](const KeyReleasedEvent& e) -> bool {
			if (e.GetKeyCode() == Cori::CORI_KEY_F8) {
				CORI_PROFILE_REQUEST_NEXT_FRAME();
			}
			return false;
		});
#endif

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
			--it;
			(*it)->OnEvent(e);
			if (e.m_Handeled || (*it)->IsModal()) {
				break;
			}
		}
	}

	void Application::PushLayer(Layer* layer) {
		Get().m_LayerStack.PushLayer(layer);
	}

	void Application::PushOverlay(Layer* layer) {
		Get().m_LayerStack.PushOverlay(layer);
	}

	void Application::Run() {
		while(m_Running) {
			CORI_PROFILER_FRAME_START();
			{
				CORI_PROFILE_SCOPE("Cori Engine Global Update");
				m_GameTimer.Update();

				GraphicsCall::SetClearColor({ 1.0f, 1.0f, 0.0f, 1.0f });
				GraphicsCall::ClearFramebuffer();

				for (Layer* layer : m_LayerStack) {
					layer->OnUpdate(m_GameTimer);
					if (layer->IsModal()) {
						break;
					}
				}

				for (Layer* layer : m_LayerStack) {
					layer->SceneUpdate(m_GameTimer);
					if (layer->IsModal()) {
						break;
					}
				}

				m_ImGuiLayer->StartFrame();

				for (Layer* layer : m_LayerStack) {
					layer->OnImGuiRender(m_GameTimer);
				}

				m_ImGuiLayer->EndFrame();

				m_Window->OnUpdate();
			}
			CORI_PROFILER_FRAME_END();
		}
	}

	void Application::TickrateUpdate(const float timeStep) {
		for (Layer* layer : m_LayerStack) {
			layer->OnTickUpdate(timeStep);
		}
	}

	bool Application::OnWindowClose() {
		m_Running = false;
		return true;
	}
}