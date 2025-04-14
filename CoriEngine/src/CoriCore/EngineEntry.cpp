#include "EngineEntry.hpp"
#include "Engine.hpp"

EngineEntry::EngineEntry() {
	Cori::Engine::Start();
}

EngineEntry::~EngineEntry() {
	Cori::Engine::Stop();
}
