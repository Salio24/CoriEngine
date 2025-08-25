#include "CoriGraphicsAPI.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_GraphicsAPI.hpp"

namespace Cori {
	std::unique_ptr<CoriGraphicsAPI> CoriGraphicsAPI::Create() {
		std::unique_ptr<CoriGraphicsAPI> api = Factory<CoriGraphicsAPI, GraphicsAPIs>::CreateUnique(Window::GetCurrentAPI());
		CORI_CORE_ASSERT(api, "Failed to create CoriGraphicsAPI for API: {}. Check registrations and API validity.", APIEnumToName(Window::GetCurrentAPI()));
		return api;
	}
}