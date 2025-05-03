// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "CoriGraphicsAPI.hpp"
#include "../Application.hpp"
#include "OpenGL/GL_GraphicsAPI.hpp"

namespace Cori {
	CoriGraphicsAPI* CoriGraphicsAPI::Create() {
		CoriGraphicsAPI* result = nullptr;
		switch (Application::Get().GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			break;
		case GraphicsAPIs::OpenGL:
			result = new OpenGLGraphicsAPI();
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