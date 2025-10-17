#include "CoriGraphicsAPI.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_GraphicsAPI.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<CoriGraphicsAPI> CoriGraphicsAPI::Create() {
			switch (Core::Window::GetCurrentAPI()) {
			case GraphicsAPIs::OpenGL:
				{
					auto api = std::make_unique<Internal::OpenGLGraphicsAPI>();
					CORI_CORE_ASSERT(api, "Failed to create Texture2D for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
					return api;
					break;
				}
			case GraphicsAPIs::Vulkan:
				{
					CORI_CORE_ASSERT(false, "Unsupported Graphics API.");
				}
			case GraphicsAPIs::None:
				{
					break;
				}
			}

			return nullptr;
		}
	}
}
