#include "ShaderProgram.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_ShaderProgram.hpp"

namespace Cori {
	namespace Graphics {
		std::shared_ptr<ShaderProgram> ShaderProgram::Create(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath) {
			switch (Core::Window::GetCurrentAPI()) {
			case GraphicsAPIs::OpenGL:
				{
					auto shader = std::make_shared<Internal::OpenGLShaderProgram>(vertexPath, fragmentPath, geometryPath);
					CORI_CORE_ASSERT(shader, "Failed to create Texture2D for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
					return shader;
					break;
				}
			case GraphicsAPIs::Vulkan:
				{
					CORI_CORE_ASSERT(false, "Unsupported Graphics API.");
				}
			case GraphicsAPIs::None:
				{
					break;
				}
			}

			return nullptr;
		}

		std::shared_ptr<ShaderProgram> ShaderProgram::Create(const Descriptor& descriptor) {
			return Create(descriptor.m_VertexPath, descriptor.m_FragmentPath, descriptor.m_GeometryPath);
		}
	}
}
