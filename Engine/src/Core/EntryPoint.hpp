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

// ReSharper disable once CppNonInlineFunctionDefinitionInHeaderFile
int32_t main([[maybe_unused]] int32_t argc, [[maybe_unused]] char** argv) {

#ifdef CORI_ASYNC_LOGGING
#ifndef CORI_NO_FILE_LOGGING
	Cori::Core::Internal::Engine::Start(true, true);
#else
	Cori::Core::Internal::Engine::Start(true, false);
#endif
#else 
#ifndef CORI_NO_FILE_LOGGING
	Cori::Core::Internal::Engine::Start(false, true);
#else
	Cori::Core::Internal::Engine::Start(false, false);
#endif
#endif

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
