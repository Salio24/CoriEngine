#include "VertexArray.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_VertexArray.hpp"

namespace Cori {
	namespace Graphics {
		std::shared_ptr<VertexArray> VertexArray::Create() {
			switch (Core::Window::GetCurrentAPI()) {
			case GraphicsAPIs::OpenGL:
				{
					auto vao = std::make_shared<Internal::OpenGLVertexArray>();
					CORI_CORE_ASSERT(vao, "Failed to create Texture2D for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
					return vao;
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
