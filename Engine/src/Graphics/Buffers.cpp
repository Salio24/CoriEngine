#include "Buffers.hpp"
#include "OpenGL/GL_Buffers.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::shared_ptr<VertexBuffer> VertexBuffer::Create() {
			std::shared_ptr<VertexBuffer> vbo = Core::Factory<VertexBuffer, GraphicsAPIs>::CreateShared(Core::Window::GetCurrentAPI());
			CORI_CORE_ASSERT(vbo, "Failed to create VertexBuffer for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
			return vbo;
		}

		std::shared_ptr<IndexBuffer> IndexBuffer::Create(uint32_t* indices, const uint32_t count) {
			std::shared_ptr<IndexBuffer> ibo = Core::Factory<IndexBuffer, GraphicsAPIs, uint32_t*, uint32_t>::CreateShared(Core::Window::GetCurrentAPI(), indices, count);
			CORI_CORE_ASSERT(ibo, "Failed to create IndexBuffer for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
			return ibo;
		}
	}
}