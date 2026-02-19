#include "Renderer.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<Renderer> Renderer::s_Instance{ nullptr };

		void Renderer::Init() {
			CORI_CORE_ASSERT(!s_Instance, "Renderer is already initialized.")
			s_Instance = std::unique_ptr<Renderer>(new Renderer());
		}

		void Renderer::Shutdown() {
			s_Instance.reset();
		}

		Renderer& Renderer::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling Renderer::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}
	}
}