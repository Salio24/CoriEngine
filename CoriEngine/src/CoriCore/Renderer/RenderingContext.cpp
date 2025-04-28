#include "RenderingContext.hpp"
#include "OpenGL/GL_GraphicsContext.hpp"
#include "../Application.hpp"

namespace Cori {
	RenderingContext* RenderingContext::Create(GraphicsAPIs api) {
		switch (api) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			return nullptr;
		case GraphicsAPIs::OpenGL:
			return new OpenGLContext();
		case GraphicsAPIs::Vulkan:
			CORI_CORE_ASSERT_FATAL(false, "Vulkan is not supported yet");
			return nullptr;
		}
		CORI_CORE_ASSERT_FATAL(false, "Unknown graphics API");
		return nullptr;
	}
}