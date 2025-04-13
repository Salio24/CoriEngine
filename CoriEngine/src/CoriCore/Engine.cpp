#include "Engine.hpp"

namespace Cori {
	void Engine::Start() {
		Cori::Logger::Init();

		bool SDL_verify = SDL_Init(SDL_INIT_VIDEO);
		CORI_CORE_ASSERT_FATAL(SDL_verify, "SDL could not initialize! SDL_Error: " + std::string(SDL_GetError()));

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

#ifdef DEBUG_BUILD
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif 

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

		CORI_CORE_INFO("Cori Engine initialized");
	}

	void Engine::Stop() {
		SDL_Quit();

		CORI_CORE_INFO("Cori Engine stopped");
		CORI_CORE_INFO("Bye");
	}
}