// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Buffers.hpp"
#include "OpenGL/GL_Buffers.hpp"
#include "../Application.hpp"

namespace Cori {
	VertexBuffer* VertexBuffer::Create() {
		VertexBuffer* result = nullptr;
		switch (Application::GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			break;
		case GraphicsAPIs::OpenGL:
			result = new OpenGLVertexBuffer();
			break;
		case GraphicsAPIs::Vulkan:
			CORI_CORE_ASSERT_FATAL(false, "Vulkan is not supported yet");
			break;
		default:
			CORI_CORE_ASSERT_FATAL(false, "Unknown graphics API");
			break;
		}
		return result;
	}

	IndexBuffer* IndexBuffer::Create(uint32_t* indices, uint32_t count) {
		IndexBuffer* result = nullptr;
		switch (Application::GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			break;
		case GraphicsAPIs::OpenGL:
			result = new OpenGLIndexBuffer(indices, count);
			break;
		case GraphicsAPIs::Vulkan:
			CORI_CORE_ASSERT_FATAL(false, "Vulkan is not supported yet");
			break;
		default:
			CORI_CORE_ASSERT_FATAL(false, "Unknown graphics API");
			break;
		}
		return result;
	}
}