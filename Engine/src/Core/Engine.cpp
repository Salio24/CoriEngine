#include "Engine.hpp"
#include "Threading/CpuTopology.hpp"
#include <SDL3_image/SDL_image.h>

namespace Cori {
	namespace Core {
		namespace Internal {
			void Engine::Start(const bool asyncLogging, const bool fileLogging) {
				Logger::EnableVirtualTerminalProcessing();

				Logger::Init(asyncLogging, fileLogging);

				Threading::CpuTopology::Init();

				CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self }, "Starting Cori Engine.");

				const char* sessionType = std::getenv("XDG_SESSION_TYPE");
				if (sessionType) {
					if (std::string(sessionType) == "wayland") {
						if (std::getenv("ENABLE_VULKAN_RENDERDOC_CAPTURE" ) || std::getenv("ENABLE_NVIDIA_NSIGHT_CAPTURE")) {
							CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self }, "RenderDoc or nsight environment variable detected. Current session is wayland, switching to XWayland for them to be happy.");
							SDL_SetHint("SDL_VIDEO_DRIVER", "x11");
						}
					}
				}

				const bool success = SDL_Init(SDL_INIT_VIDEO);
				CORI_CORE_ASSERT(success, "SDL3 failed to initialized! SDL_Error: {}", SDL_GetError());
			}

			void Engine::Stop() {
				SDL_Quit();

				Threading::CpuTopology::Shutdown();

				CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self }, "Cori Engine stopped.");
				CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self }, "Bye");
				spdlog::shutdown();
			}
		}
	}
}
