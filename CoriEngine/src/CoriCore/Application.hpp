#pragma once 
#include <CoriEngine_export.hpp>
#include "EventSystem/Event.hpp"
#include "EventSystem/AppEvent.hpp"
#include "Window.hpp"
#include "Layer.hpp"
#include "LayerStack.hpp"

namespace Cori {
	class CORI_ENGINE_API Application {
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);
		
	private: 
		bool OnWindowClose(WindowCloseEvent& e);

		std::unique_ptr<Window> m_Window;

		LayerStack m_LayerStack;

		bool m_Running{ true };
	};

	Application* CreateApplication();
}