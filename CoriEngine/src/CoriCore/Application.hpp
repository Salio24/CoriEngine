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
		
		inline static Application& Get() { return *s_Instance; }

		inline Window& GetWindow() { return *m_Window; }

	private: 

		bool OnWindowClose(WindowCloseEvent& e);

		std::unique_ptr<Window> m_Window;

		LayerStack m_LayerStack;

		bool m_Running{ true };

		static Application* s_Instance;
	};

	Application* CreateApplication();
}