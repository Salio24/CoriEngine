#include "Engine.hpp"

namespace Cori {
	void Engine::Start(bool asyncLogging, bool fileLogging) {

		Logger::EnableVirtualTerminalProcessing();

		Logger::Init(asyncLogging, fileLogging);

		bool SDL_verify = SDL_Init(SDL_INIT_VIDEO);
		CORI_CORE_ASSERT_FATAL(SDL_verify, "SDL3 failed to initialized! SDL_Error: {}", std::string(SDL_GetError()));

		CORI_CORE_INFO("Cori Engine Core initialized");
	}

	void Engine::Stop() {
		SDL_Quit();

		CORI_CORE_INFO("Cori Engine stopped");
		CORI_CORE_INFO("Bye");
		spdlog::shutdown();
	}
}