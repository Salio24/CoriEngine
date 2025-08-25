#include "GL_GraphicsAPI.hpp"
#include <glad/gl.h>


namespace Cori {

	static void GLAPIENTRY GLDebugMessageCallback(GLenum source, GLenum type, GLuint id,
		GLenum severity, [[maybe_unused]] GLsizei length,
		const GLchar* message, [[maybe_unused]] const void* param) {
		const char* source_, * type_, * severity_;

		switch (source)
		{
		case GL_DEBUG_SOURCE_API:             source_ = "API";             break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   source_ = "WINDOW_SYSTEM";   break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: source_ = "SHADER_COMPILER"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:     source_ = "THIRD_PARTY";     break;
		case GL_DEBUG_SOURCE_APPLICATION:     source_ = "APPLICATION";     break;
		case GL_DEBUG_SOURCE_OTHER:           source_ = "OTHER";           break;
		default:                              source_ = "<SOURCE>";        break;
		}

		switch (type)
		{
		case GL_DEBUG_TYPE_ERROR:               type_ = "ERROR";               break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_ = "DEPRECATED_BEHAVIOR"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  type_ = "UDEFINED_BEHAVIOR";   break;
		case GL_DEBUG_TYPE_PORTABILITY:         type_ = "PORTABILITY";         break;
		case GL_DEBUG_TYPE_PERFORMANCE:         type_ = "PERFORMANCE";         break;
		case GL_DEBUG_TYPE_OTHER:               type_ = "OTHER";               break;
		case GL_DEBUG_TYPE_MARKER:              type_ = "MARKER";              break;
		default:                                type_ = "<TYPE>";              break;
		}

		switch (severity)
		{
		case GL_DEBUG_SEVERITY_HIGH:         severity_ = "HIGH";         break;
		case GL_DEBUG_SEVERITY_MEDIUM:       severity_ = "MEDIUM";       break;
		case GL_DEBUG_SEVERITY_LOW:          severity_ = "LOW";          break;
		case GL_DEBUG_SEVERITY_NOTIFICATION: severity_ = "NOTIFICATION"; break;
		default:                             severity_ = "<SEVERITY>";   break;
		}


		std::ostringstream stream;
		stream << "| Id: " << id << " | Severity: " << severity_ << " | Type: " << type_ << " | Source: (" << source_ << ") | Message: " << message << " |" << std::endl;
		std::string output = stream.str();

		std::string dashes(output.size() - 3, '-');

		switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:
			CORI_CORE_ERROR("{}|{}|", output, dashes);
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			CORI_CORE_WARN("{}|{}|", output, dashes);
			break;
		case GL_DEBUG_SEVERITY_LOW:
			CORI_CORE_INFO("{}|{}|", output, dashes);
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			CORI_CORE_TRACE("{}|{}|", output, dashes);
			break;
		default:
			break;
		}
	}

	void OpenGLGraphicsAPI::Init() {

#ifdef DEBUG_BUILD
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(GLDebugMessageCallback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#endif
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
	}

	void OpenGLGraphicsAPI::SetViewport(const int32_t x, const int32_t y, const int32_t width, const int32_t height) {
		glViewport(x, y, width, height);
		CORI_CORE_TRACE_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL }, "Viewport was set size: x={}, y={}, width={}, height={}", x, y, width, height);
	}

	void OpenGLGraphicsAPI::SetClearColor(const glm::vec4& color) {
		glClearColor(color.r, color.g, color.b, color.a);
	}

	void OpenGLGraphicsAPI::ClearFramebuffer() {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLGraphicsAPI::DrawElementsTriangles(const uint32_t elementCount) {
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(elementCount), GL_UNSIGNED_INT, nullptr);
	}

	void OpenGLGraphicsAPI::DrawElementsInstancedTriangles(const uint32_t instanceCount) {
		glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, instanceCount);
	}

	void OpenGLGraphicsAPI::EnableDepthTest() {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		//CORI_CORE_TRACE_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL }, "Depth test was enabled");
	}

	void OpenGLGraphicsAPI::DisableDepthTest() {
		glDisable(GL_DEPTH_TEST);
		//CORI_CORE_TRACE_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL }, "Depth test was disabled");
	}

	void OpenGLGraphicsAPI::EnableBlending() {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		//CORI_CORE_TRACE_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL }, "Blending was enabled");
	}

	void OpenGLGraphicsAPI::DisableBlending() {
		glDisable(GL_BLEND);
		//CORI_CORE_TRACE_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL }, "Blending was disabled");
	}

	void OpenGLGraphicsAPI::SetDepthMask(const bool mode) {
		glDepthMask(mode);
	}

	OpenGLGraphicsAPI::OpenGLGraphicsAPI() {
	
	} 
	
	bool OpenGLGraphicsAPI::PreCreateHook() {
		return true;
	}
}