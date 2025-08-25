#include "RenderingContext.hpp"
#include "OpenGL/GL_GraphicsContext.hpp"
#include "Core/Application.hpp"

namespace Cori {
	std::unique_ptr<RenderingContext> RenderingContext::Create(GraphicsAPIs api) {
		std::unique_ptr<RenderingContext> context = Factory<RenderingContext, GraphicsAPIs>::CreateUnique(api);
		CORI_CORE_ASSERT(context, "Failed to create RenderingContext for API: {}. Check registrations and API validity.", APIEnumToName(Window::GetCurrentAPI()));
		return context;
	}
}