#include "Engine.hpp"
#include <SDL3_image/SDL_image.h>

namespace Cori {
	namespace Core {
		namespace Internal {
			void Engine::Start(const bool asyncLogging, const bool fileLogging) {
				Logger::EnableVirtualTerminalProcessing();

				Logger::Init(asyncLogging, fileLogging);

				CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self }, "Starting Cori Engine.");

				const bool success = SDL_Init(SDL_INIT_VIDEO);
				CORI_CORE_ASSERT(success, "SDL3 failed to initialized! SDL_Error: {}", SDL_GetError());
			}

			void Engine::Stop() {
				SDL_Quit();

				CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self }, "Cori Engine stopped.");
				CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self }, "Bye");
				spdlog::shutdown();
			}
		}
	}
}