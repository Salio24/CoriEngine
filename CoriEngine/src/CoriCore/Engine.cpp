#include "Engine.hpp"

namespace Cori {
	void Engine::Start() {
		Cori::Logger::Init();

		bool SDL_verify = SDL_Init(SDL_INIT_VIDEO);
		CORI_CORE_ASSERT_FATAL(SDL_verify, "SDL could not initialize! SDL_Error: " + std::string(SDL_GetError()));

		CORI_CORE_INFO("Cori Engine initialized");
	}

	void Engine::Stop() {
		SDL_Quit();

		CORI_CORE_INFO("Cori Engine stopped");
		CORI_CORE_INFO("Bye");
	}
}