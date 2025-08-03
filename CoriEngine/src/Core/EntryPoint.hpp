#pragma once
#include "Engine.hpp"

extern Cori::Application* Cori::CreateApplication();

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {

#ifdef CORI_ASYNC_LOGGING
#ifndef CORI_NO_FILE_LOGGING
	Cori::Engine::Start(true, true);
#else
	Cori::Engine::Start(true, false);
#endif
#else 
#ifndef CORI_NO_FILE_LOGGING
	Cori::Engine::Start(false, true);
#else
	Cori::Engine::Start(false, false);
#endif
#endif

	Cori::Application* app = Cori::CreateApplication();

	if (app) {
		app->Run();
		delete app;
	}
	else {
		return -1;
	}

	Cori::Engine::Stop();
}