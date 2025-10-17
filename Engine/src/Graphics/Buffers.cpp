#include "Buffers.hpp"
#include "OpenGL/GL_Buffers.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::shared_ptr<VertexBuffer> VertexBuffer::Create() {
			switch (Core::Window::GetCurrentAPI()) {
			case GraphicsAPIs::OpenGL:
				{
					auto vbo = std::make_shared<Internal::OpenGLVertexBuffer>();
					CORI_CORE_ASSERT(vbo, "Failed to create Texture2D for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
					return vbo;
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

		std::shared_ptr<IndexBuffer> IndexBuffer::Create(uint32_t* indices, const uint32_t count) {
			switch (Core::Window::GetCurrentAPI()) {
			case GraphicsAPIs::OpenGL:
				{
					auto ibo = std::make_shared<Internal::OpenGLIndexBuffer>(indices, count);
					CORI_CORE_ASSERT(ibo, "Failed to create Texture2D for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
					return ibo;
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
