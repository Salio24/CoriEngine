#pragma once
#include "EngineEntry.hpp"

extern Cori::Application* Cori::CreateApplication();

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
	EngineEntry engineEntry;

	Cori::Application* app = Cori::CreateApplication();

	if (app) {
		app->Run();
		delete app;
	}
	else {
		return -1;
	}
}