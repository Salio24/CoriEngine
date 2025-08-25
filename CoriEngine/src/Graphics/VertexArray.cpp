#include "VertexArray.hpp"
#include "OpenGL/GL_VertexArray.hpp"
#include "Core/Application.hpp"

namespace Cori {
	std::shared_ptr<VertexArray> VertexArray::Create() {
		std::shared_ptr<VertexArray> vao = Factory<VertexArray, GraphicsAPIs>::CreateShared(Window::GetCurrentAPI());
		CORI_CORE_ASSERT(vao, "Failed to create VertexArray for API: {}. Check registrations and API validity.", APIEnumToName(Window::GetCurrentAPI()));
		return vao;

	}
}