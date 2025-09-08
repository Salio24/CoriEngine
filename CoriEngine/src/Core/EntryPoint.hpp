#pragma once
#include "Engine.hpp"

extern Cori::Core::Application* Cori::Core::CreateApplication();

// ReSharper disable once CppNonInlineFunctionDefinitionInHeaderFile
int32_t main([[maybe_unused]] int32_t argc, [[maybe_unused]] char** argv) {

#ifdef CORI_ASYNC_LOGGING
#ifndef CORI_NO_FILE_LOGGING
	Cori::Core::Engine::Start(true, true);
#else
	Cori::Core::Engine::Start(true, false);
#endif
#else 
#ifndef CORI_NO_FILE_LOGGING
	Cori::Core::Engine::Start(false, true);
#else
	Cori::Core::Engine::Start(false, false);
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

	Cori::Core::Engine::Stop();
}
