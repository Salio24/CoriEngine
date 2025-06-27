// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "PipelineProgram.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_PipelineProgram.hpp"

namespace Cori {
	PipelineProgram* PipelineProgram::Create() {
		CORI_CORE_ERROR("Dont use pipeline program, not using separable programs rn");
		return nullptr;
		PipelineProgram* result = nullptr;
		switch (Application::GetWindow().GetAPI()) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			break;
		case GraphicsAPIs::OpenGL:
			//result = new OpenGLPipelineProgram();
			break;
		case GraphicsAPIs::Vulkan:
			CORI_CORE_ASSERT_FATAL(false, "Vulkan is not supported yet");
			break;
		default:
			CORI_CORE_ASSERT_FATAL(false, "Unknown graphics API");
			break;
		}
		return result;
	}

}