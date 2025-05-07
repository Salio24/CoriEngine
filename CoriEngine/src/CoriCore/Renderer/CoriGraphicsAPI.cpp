// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "CoriGraphicsAPI.hpp"
#include "../Application.hpp"
#include "OpenGL/GL_GraphicsAPI.hpp"

namespace Cori {
	std::unique_ptr<CoriGraphicsAPI> CoriGraphicsAPI::Create() {
		switch (Application::GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			return nullptr;
			break;
		case GraphicsAPIs::OpenGL:
			return std::make_unique<OpenGLGraphicsAPI>();
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