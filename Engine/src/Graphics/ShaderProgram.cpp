#include "ShaderProgram.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_ShaderProgram.hpp"

namespace Cori {
	namespace Graphics {
		std::shared_ptr<ShaderProgram> ShaderProgram::Create(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath) {
			std::shared_ptr<ShaderProgram> shader = Core::Factory<ShaderProgram, GraphicsAPIs, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&>::CreateShared(Core::Window::GetCurrentAPI(), vertexPath, fragmentPath, geometryPath);
			CORI_CORE_ASSERT(shader, "Failed to create ShaderProgram for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
			return shader;
		}

		std::shared_ptr<ShaderProgram> ShaderProgram::Create(const Descriptor& descriptor) {
			return Create(descriptor.m_VertexPath, descriptor.m_FragmentPath, descriptor.m_GeometryPath);
		}
	}
}
