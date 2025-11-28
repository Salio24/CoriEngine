#include "ShaderProgram.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		std::shared_ptr<ShaderProgram> ShaderProgram::Create(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath) {
			return nullptr;
		}

		std::shared_ptr<ShaderProgram> ShaderProgram::Create(const Descriptor& descriptor) {
			return Create(descriptor.m_VertexPath, descriptor.m_FragmentPath, descriptor.m_GeometryPath);
		}
	}
}
