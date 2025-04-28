#include "ShaderProgram.hpp"
#include "../Application.hpp"
#include "OpenGL/GL_ShaderProgram.hpp"

namespace Cori {
	ShaderProgram* ShaderProgram::Create(const char* vertexPath, const char* fragmentPath, const char* geometryPath) {
		switch (Application::Get().GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			return nullptr;
		case GraphicsAPIs::OpenGL:
			return new OpenGLShaderProgram(vertexPath, fragmentPath, geometryPath);
		case GraphicsAPIs::Vulkan:
			CORI_CORE_ASSERT_FATAL(false, "Vulkan is not supported yet");
			return nullptr;
		}
		CORI_CORE_ASSERT_FATAL(false, "Unknown graphics API");
		return nullptr;
	}

}