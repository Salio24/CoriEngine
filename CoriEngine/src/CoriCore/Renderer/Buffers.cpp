// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Buffers.hpp"
#include "OpenGL/GL_Buffers.hpp"
#include "../Application.hpp"

namespace Cori {
	std::shared_ptr<VertexBuffer> VertexBuffer::Create() {
		switch (Application::GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			return nullptr;
			break;
		case GraphicsAPIs::OpenGL:
			return std::make_shared<OpenGLVertexBuffer>();
			break;
		case GraphicsAPIs::Vulkan:
			CORI_CORE_ASSERT_FATAL(false, "Vulkan is not supported yet");
			return nullptr;
			break;
		default:
			CORI_CORE_ASSERT_FATAL(false, "Unknown graphics API");
			return nullptr;
			break;
		}
	}

	std::shared_ptr<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t count) {
		switch (Application::GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			return nullptr;
			break;
		case GraphicsAPIs::OpenGL:
			return std::make_shared<OpenGLIndexBuffer>(indices, count);
			break;
		case GraphicsAPIs::Vulkan:
			CORI_CORE_ASSERT_FATAL(false, "Vulkan is not supported yet");
			return nullptr;
			break;
		default:
			CORI_CORE_ASSERT_FATAL(false, "Unknown graphics API");
			return nullptr;
			break;
		}
	}
}