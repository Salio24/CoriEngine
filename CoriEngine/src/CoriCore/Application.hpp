#pragma once 
#include <CoriEngine_export.hpp>
#include "EventSystem/Event.hpp"
#include "Window.hpp"

namespace Cori {
	class CORI_ENGINE_API Application {
	public:
		Application();
		virtual ~Application();

		void Run();
	private: 
		std::unique_ptr<Window> m_Window;

		bool m_Running{ true };
	};

	Application* CreateApplication();
}