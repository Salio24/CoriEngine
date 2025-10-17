#include "RenderingContext.hpp"
#include "OpenGL/GL_GraphicsContext.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::unique_ptr<RenderingContext> RenderingContext::Create(const GraphicsAPIs api) {
			switch (api) {
			case GraphicsAPIs::OpenGL:
				{
					auto contex = std::make_unique<Internal::OpenGLContext>();
					CORI_CORE_ASSERT(contex, "Failed to create Texture2D for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
					return contex;
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
