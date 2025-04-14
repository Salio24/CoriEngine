#pragma once
#include "EngineEntry.hpp"

extern Cori::Application* Cori::CreateApplication();

int main(int argc, char** argv) {
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