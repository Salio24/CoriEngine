#include "Buffers.hpp"
#include "OpenGL/GL_Buffers.hpp"
#include "Core/Application.hpp"

namespace Cori {
	std::shared_ptr<VertexBuffer> VertexBuffer::Create() {
		std::shared_ptr<VertexBuffer> vbo = Factory<VertexBuffer, GraphicsAPIs>::CreateShared(Window::GetAPI());
		CORI_CORE_ASSERT_FATAL(vbo, "Failed to create VertexBuffer for API: {0}. Check registrations and API validity.", static_cast<int>(Window::GetAPI())); // output api as a string
		return vbo;
	}

	std::shared_ptr<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t count) {
		std::shared_ptr<IndexBuffer> ibo = Factory<IndexBuffer, GraphicsAPIs, uint32_t*, uint32_t>::CreateShared(Window::GetAPI(), indices, count);
		CORI_CORE_ASSERT_FATAL(ibo, "Failed to create IndexBuffer for API: {0}. Check registrations and API validity.", static_cast<int>(Window::GetAPI())); // output api as a string
		return ibo;
	}
}