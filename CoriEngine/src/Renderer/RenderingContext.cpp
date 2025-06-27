// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "RenderingContext.hpp"
#include "OpenGL/GL_GraphicsContext.hpp"
#include "Core/Application.hpp"

namespace Cori {
	std::unique_ptr<RenderingContext> RenderingContext::Create(GraphicsAPIs api) {


		std::unique_ptr<RenderingContext> context = Factory<RenderingContext, GraphicsAPIs>::CreateUnique(api);
		CORI_CORE_ASSERT_FATAL(context, "Failed to create RenderingContext for API: {0}. Check registrations and API validity.", static_cast<int>(Application::GetCurrentAPI())); // output api as a string
		return context;
	}
}