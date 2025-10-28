#pragma once
#ifdef CORI_ENABLE_PROFILING
#include <tracy/Tracy.hpp>

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
			ZoneScopedN("Warmup Zone");
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

	Cori::Core::Internal::Engine::Start(asyncLogging, fileLogging);

	Cori::Core::Application* app = Cori::Core::CreateApplication();

	if (app) {
		app->Run();
		delete app;
	}
	else {
		return -1;
	}

	Cori::Core::Internal::Engine::Stop();
}
