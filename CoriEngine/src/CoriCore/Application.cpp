#include "Application.hpp"
#include "EventSystem/Event.hpp"
#include "EventSystem/AppEvent.hpp"
#include "Logger.hpp"	
#include "Engine.hpp"

namespace Cori
{
	Application::Application() {
		Cori::Engine::Start();


		m_Window = std::unique_ptr<Window>(Window::Create());
	}
	Application::~Application() {
		Cori::Engine::Stop();
	}
	void Application::Run() {

		while(m_Running) {
			m_Window->OnUpdate();
		}
	}
}