// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "GL_GraphicsContext.hpp"
#include <glad/gl.h>
#include "Core/Application.hpp"

namespace Cori {
	bool OpenGLContext::PreCreateHook() {
		return true;
	}

	OpenGLContext::OpenGLContext() {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#ifdef DEBUG_BUILD
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif	
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

		CORI_CORE_DEBUG("GL_GraphicsContext: Rendering context was created successfully");
		CORI_CORE_TRACE("GL_GraphicsContext: Using hardware renderer: \n{4}Vendor: {0}\n{4}Renderer: {1}\n{4}OpenGL Version {2}\n{4}Shading Language Version: {3}",
			std::string(reinterpret_cast<const char*>(glGetString(GL_VENDOR))),
			std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER))),
			std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION))),
			std::string(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))), CORI_SECOND_LINE_SPACING);
	}

	void OpenGLContext::SwapBuffers() {
		SDL_GL_SwapWindow(static_cast<SDL_Window*>(Application::GetWindow().GetNativeWindow()));
	}
}