// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "ShaderProgram.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_ShaderProgram.hpp"

namespace Cori {
	std::shared_ptr<ShaderProgram> ShaderProgram::Create(const ShaderProgramDescriptor& descriptor) {


		std::shared_ptr<ShaderProgram> shader = Factory<ShaderProgram, GraphicsAPIs, const std::string_view, const std::string_view, const std::string_view, const std::string_view>::CreateShared(Application::GetCurrentAPI(), descriptor.GetDebugName(), descriptor.m_VertexPath, descriptor.m_FragmentPath, descriptor.m_GeometryPath);
		CORI_CORE_ASSERT_FATAL(shader, "Failed to create ShaderProgram for API: {0}. Check registrations and API validity.", static_cast<int>(Application::GetCurrentAPI())); // output api as a string
		return shader;
	}
}