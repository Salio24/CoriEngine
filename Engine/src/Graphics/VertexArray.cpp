#include "VertexArray.hpp"
#include "OpenGL/GL_VertexArray.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
			std::shared_ptr<VertexArray> VertexArray::Create() {
				std::shared_ptr<VertexArray> vao = Core::Factory<VertexArray, GraphicsAPIs>::CreateShared(Core::Window::GetCurrentAPI());
				CORI_CORE_ASSERT(vao, "Failed to create VertexArray for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
				return vao;
			}
		}
	}
}