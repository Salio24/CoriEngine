#pragma once 
#include <CoriEngine_export.hpp>

namespace Cori {
	class CORI_ENGINE_API Application {
	public:
		Application();
		virtual ~Application();

		void Run();
	};

	Application* CreateApplication();
}