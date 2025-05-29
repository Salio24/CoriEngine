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

namespace Cori {
	Application* Application::s_Instance = nullptr;

	Application::Application() {
		CORI_CORE_ASSERT_FATAL(!s_Instance, "Trying to construct application for the second time. Application already exists!");
		s_Instance = this;

		m_Window = Window::Create();

		m_Window->SetEventCallback(CORI_BIND_EVENT_FN(Application::OnEvent, CORI_PLACEHOLDERS(1)));

		m_ImGuiLayer = new ImGuiLayer();

		PushOverlay(m_ImGuiLayer);

		GraphicsCall::InitRenderers();

		m_GameTimer.SetTickrate(128);
		m_GameTimer.SetTickrateUpdateFunc(CORI_BIND_EVENT_FN(Application::TickrateUpdate));
	}

	Application::~Application() {
		GraphicsCall::ShutdownRenderers();
	}

	void Application::OnEvent(Event& e) {
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(CORI_BIND_EVENT_FN(Application::OnWindowClose));

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
			(*--it)->OnEvent(e);
			if (e.m_Handeled) {
				break;
			}
		}
	}

	void Application::PushLayer(Layer* layer) {
		m_LayerStack.PushLayer(layer);
	}

	void Application::PushOverlay(Layer* layer) {
		m_LayerStack.PushOverlay(layer);
	}

	void Application::Run() {
		while(m_Running) {

			m_GameTimer.Update();

			for (Layer* layer : m_LayerStack) {
				layer->OnUpdate(m_GameTimer);
			}

			m_ImGuiLayer->StartFrame();

			for (Layer* layer : m_LayerStack) {
				layer->OnImGuiRender(m_GameTimer);
			}

			m_ImGuiLayer->EndFrame();

			m_Window->OnUpdate();

		}
	}

	void Application::TickrateUpdate() {
		for (Layer* layer : m_LayerStack) {
			layer->OnTickUpdate();
		}
	}


	bool Application::OnWindowClose() {
		m_Running = false;
		return true;
	}
}