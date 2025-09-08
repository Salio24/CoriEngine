#include "CoriGraphicsAPI.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_GraphicsAPI.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<CoriGraphicsAPI> CoriGraphicsAPI::Create() {
			std::unique_ptr<CoriGraphicsAPI> api = Core::Factory<CoriGraphicsAPI, GraphicsAPIs>::CreateUnique(Core::Window::GetCurrentAPI());
			CORI_CORE_ASSERT(api, "Failed to create CoriGraphicsAPI for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
			return api;
		}
	}
}