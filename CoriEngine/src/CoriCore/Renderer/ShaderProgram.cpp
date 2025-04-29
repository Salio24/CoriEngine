// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "ShaderProgram.hpp"
#include "../Application.hpp"
#include "OpenGL/GL_ShaderProgram.hpp"

namespace Cori {
	ShaderProgram* ShaderProgram::Create(const char* vertexPath, const char* fragmentPath, const char* geometryPath) {
		ShaderProgram* result = nullptr;
		switch (Application::Get().GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			break;
		case GraphicsAPIs::OpenGL:
			result = new OpenGLShaderProgram(vertexPath, fragmentPath, geometryPath);
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