#include "Application.hpp"
#include "Engine.hpp"
#include "WindowImpl.hpp"
#include "Input.hpp"
#include <imgui.h>

namespace Cori {
#define CORI_BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;

	Application::Application() {
		CORI_CORE_ASSERT_FATAL(!s_Instance, "Application already exists!");
		s_Instance = this;

		m_Window = std::unique_ptr<Window>(Window::Create());


		m_Window->SetEventCallback(CORI_BIND_EVENT_FN(Application::OnEvent));

		m_ImGuiLayer = new ImGuiLayer();  

		PushOverlay(m_ImGuiLayer);
	}

	Application::~Application() {
		//m_Window.reset();
	}

	void Application::OnEvent(Event& e) {
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(CORI_BIND_EVENT_FN(Application::OnWindowClose));
	
		//CORI_CORE_DEBUG(e);

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
			glViewport(0, 0, m_Window->GetWidth(), m_Window->GetHeight());
			glClearColor((14.0f / 256.0f), (7.0f / 256.0f), (27.0f / 256.0f), 1.0f);
			glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
			for (Layer* layer : m_LayerStack) {
				layer->OnUpdate();
			}

			m_ImGuiLayer->StartFrame();

			for (Layer* layer : m_LayerStack) {
			
				layer->OnImGuiRender();
			}

			m_ImGuiLayer->EndFrame();

			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e) {
		m_Running = false;
		return true;
	}

	void Application::test() {
		ImGui::Begin("Example Layer");
		ImGui::Text("Hello from ExampleLayer");
		ImGui::End();
	}
}