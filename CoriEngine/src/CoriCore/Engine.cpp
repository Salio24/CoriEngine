// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Engine.hpp"

namespace Cori {
	void Engine::Start() {
		Cori::Logger::Init();

		bool SDL_verify = SDL_Init(SDL_INIT_VIDEO);
		CORI_CORE_ASSERT_FATAL(SDL_verify, "SDL3 failed to initialized! SDL_Error: " + std::string(SDL_GetError()));

		CORI_CORE_INFO("Cori Engine Core initialized");
	}

	void Engine::Stop() {
		SDL_Quit();

		CORI_CORE_INFO("Cori Engine stopped");
		CORI_CORE_INFO("Bye");
	}
}