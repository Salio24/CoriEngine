#pragma once
#ifdef CORI_ENABLE_PROFILING
#include <tracy/Tracy.hpp>
#undef TRACY_ON_DEMAND
#define TRACY_NO_EXIT 1
void* operator new(std::size_t count)
{
	auto ptr= malloc(count);
	TracyAllocS(ptr, count, 35);
	return ptr;
}
void operator delete(void* ptr) noexcept
{
	TracyFreeS(ptr, 35);
	free(ptr);
}
#endif

#include "Engine.hpp"

extern Cori::Core::Application* Cori::Core::CreateApplication();

int32_t main([[maybe_unused]] int32_t argc, [[maybe_unused]] char** argv) {
	#ifdef CORI_ENABLE_PROFILING
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(7);
		while (!TracyIsConnected && std::chrono::steady_clock::now() < deadline) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	#endif

	bool asyncLogging = false;
	#ifdef CORI_ASYNC_LOGGING
		asyncLogging = true;
	#endif

	bool fileLogging = true;
	#ifdef CORI_NO_FILE_LOGGING
		fileLogging = false;
	#endif
	Cori::Core::Application* app;
	{
		CORI_PROFILE_SCOPE("Init");
		Cori::Core::Internal::Engine::Start(asyncLogging, fileLogging);
		app = Cori::Core::CreateApplication();
	}

	if (app) {
		app->Run();
		delete app;
	}
	else {
		return -1;
	}

	Cori::Core::Internal::Engine::Stop();
}
