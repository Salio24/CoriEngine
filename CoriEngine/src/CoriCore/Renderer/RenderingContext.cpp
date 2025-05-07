// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "RenderingContext.hpp"
#include "OpenGL/GL_GraphicsContext.hpp"
#include "../Application.hpp"

namespace Cori {
	std::unique_ptr<RenderingContext> RenderingContext::Create(GraphicsAPIs api) {
		switch (api) {
		case GraphicsAPIs::None:
			CORI_CORE_FATAL("No graphics API selected");
			return nullptr;
			break;
		case GraphicsAPIs::OpenGL:
			return std::make_unique<OpenGLContext>();
			break;
		case GraphicsAPIs::Vulkan:
			CORI_CORE_FATAL("Vulkan is not supported yet");
			return nullptr;
			break;
		default:
			CORI_CORE_FATAL("Unknown graphics API");
			return nullptr;
			break;
		}
	}
}