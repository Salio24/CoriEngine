#include "VertexArray.hpp"
#include "OpenGL/GL_VertexArray.hpp"
#include "Core/Application.hpp"

namespace Cori {
	std::shared_ptr<VertexArray> VertexArray::Create() {
		std::shared_ptr<VertexArray> vao = Factory<VertexArray, GraphicsAPIs>::CreateShared(Window::GetAPI());
		CORI_CORE_ASSERT_FATAL(vao, "Failed to create VertexArray for API: {0}. Check registrations and API validity.", static_cast<int>(Window::GetAPI())); // output api as a string
		return vao;

	}
}