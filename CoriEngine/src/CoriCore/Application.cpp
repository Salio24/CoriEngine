#include "Application.hpp"
#include "Engine.hpp"

namespace Cori {

#define CORI_BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1)

	Application::Application() {
		m_Window = std::unique_ptr<Window>(Window::Create());

		m_Window->SetEventCallback(CORI_BIND_EVENT_FN(Application::OnEvent));
	}

	Application::~Application() {
		m_Window.reset();
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
			m_Window->OnUpdate();

			for (Layer* layer : m_LayerStack) {
				layer->OnUpdate();
			}
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e) {
		m_Running = false;
		return true;
	}
}