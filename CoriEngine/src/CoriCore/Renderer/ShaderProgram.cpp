// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "ShaderProgram.hpp"
#include "../Application.hpp"
#include "OpenGL/GL_ShaderProgram.hpp"

namespace Cori {
	std::shared_ptr<ShaderProgram> ShaderProgram::Create(const ShaderProgramDescriptor& descriptor) {
		switch (Application::GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_FATAL("No graphics API selected");
			return nullptr;
			break;
		case GraphicsAPIs::OpenGL:
			return std::make_shared<OpenGLShaderProgram>(descriptor.GetDebugName() ,descriptor.m_VertexPath, descriptor.m_FragmentPath, descriptor.m_GeometryPath);
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