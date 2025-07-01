#pragma once
#include "Engine.hpp"

extern Cori::Application* Cori::CreateApplication();

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {

#ifdef CORI_ASYNC_LOGGING
	Cori::Engine::Start(true);
#else 
	Cori::Engine::Start(false);
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