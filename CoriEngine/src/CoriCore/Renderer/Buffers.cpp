#include "Buffers.hpp"
#include "OpenGL/GL_Buffers.hpp"
#include "../Application.hpp"

namespace Cori {
	VertexBuffer* VertexBuffer::Create(float* vertices, uint32_t size, DRAW_TYPE drawtype) {
		switch (Application::Get().GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			return nullptr;
		case GraphicsAPIs::OpenGL:
			return new OpenGLVertexBuffer(vertices, size, drawtype);
		case GraphicsAPIs::Vulkan:
			CORI_CORE_ASSERT_FATAL(false, "Vulkan is not supported yet");
			return nullptr;
		}
		CORI_CORE_ASSERT_FATAL(false, "Unknown graphics API");
		return nullptr;
	}

	IndexBuffer* IndexBuffer::Create(uint32_t* indices, uint32_t count) {
		switch (Application::Get().GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			return nullptr;
		case GraphicsAPIs::OpenGL:
			return new OpenGLIndexBuffer(indices, count);
		case GraphicsAPIs::Vulkan:
			CORI_CORE_ASSERT_FATAL(false, "Vulkan is not supported yet");
			return nullptr;
		}
		CORI_CORE_ASSERT_FATAL(false, "Unknown graphics API");
		return nullptr;

	}
}