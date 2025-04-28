#include "GL_GraphicsContext.hpp"
#include <glad/gl.h>
#include "../../Application.hpp"

namespace Cori {
	OpenGLContext::OpenGLContext() {
#ifdef DEBUG_BUILD
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif 
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	}

	OpenGLContext::~OpenGLContext() {
		SDL_GL_DestroyContext(m_Context);
		CORI_CORE_INFO("OpenGL context destroyed");
	}

	void OpenGLContext::Init(SDL_Window* window) {
		m_Context = SDL_GL_CreateContext(window);
		CORI_CORE_ASSERT_FATAL(!!m_Context, "OpenGL context could not be created! SDL_Error: " + std::string(SDL_GetError()));

		int glad_version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
		CORI_CORE_ASSERT_FATAL(glad_version != 0, "Failed to initialize GLAD");

		CORI_CORE_TRACE("Vendor: " + std::string(reinterpret_cast<const char*>(glGetString(GL_VENDOR))));
		CORI_CORE_TRACE("Renderer: " + std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER))));
		CORI_CORE_TRACE("GL Version: " + std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
		CORI_CORE_TRACE("Shading Language Version: " + std::string(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));
	}

	void OpenGLContext::SwapBuffers() {
		SDL_GL_SwapWindow((SDL_Window*)Application::Get().GetWindow().GetNativeWindow());
	}
}