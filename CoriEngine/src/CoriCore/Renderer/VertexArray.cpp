// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "VertexArray.hpp"
#include "OpenGL/GL_VertexArray.hpp"
#include "../Application.hpp"

namespace Cori {
	std::shared_ptr<VertexArray> VertexArray::Create() {
		//switch (Application::GetWindow().GetAPI()) {
		//case GraphicsAPIs::None:
		//	CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
		//	return nullptr;
		//	break;
		//case GraphicsAPIs::OpenGL:
		//	return std::make_shared<OpenGLVertexArray>();
		//	break;
		//case GraphicsAPIs::Vulkan:
		//	CORI_CORE_ASSERT_FATAL(false, "Vulkan is not supported yet");
		//	return nullptr;
		//	break;
		//default:
		//	CORI_CORE_ASSERT_FATAL(false, "Unknown graphics API");
		//	return nullptr;
		//	break;
		//}

		std::shared_ptr<VertexArray> va = Factory<VertexArray, GraphicsAPIs>::CreateShared(Application::GetCurrentAPI());
		if (!va) {
			CORI_CORE_ASSERT_FATAL(false, "Failed to create VertexArray for API: {0}. Check registrations and API validity.", static_cast<int>(Application::GetCurrentAPI())); // output api as a string
		}
		return va;

	}
}