// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "VertexArray.hpp"
#include "OpenGL/GL_VertexArray.hpp"
#include "../Application.hpp"

namespace Cori {
	std::shared_ptr<VertexArray> VertexArray::Create() {
		std::shared_ptr<VertexArray> vao = Factory<VertexArray, GraphicsAPIs>::CreateShared(Application::GetCurrentAPI());
		CORI_CORE_ASSERT_FATAL(vao, "Failed to create VertexArray for API: {0}. Check registrations and API validity.", static_cast<int>(Application::GetCurrentAPI())); // output api as a string
		return vao;

	}
}