#include "Texture.hpp"
#include "../Application.hpp"
#include "OpenGL/GL_Texture.hpp"

namespace Cori {
	std::shared_ptr<Texture2D> Texture2D::Create(const std::string& path) {
		switch (Application::GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			return nullptr;
			break;
		case GraphicsAPIs::OpenGL:
			return std::make_shared<OpenGLTexture2D>(path);
			break;
		case GraphicsAPIs::Vulkan:
			CORI_CORE_ASSERT_FATAL(false, "Vulkan is not supported yet");
			return nullptr;
			break;
		default:
			CORI_CORE_ASSERT_FATAL(false, "Unknown graphics API");
			return nullptr;
			break;
		}
	}

}