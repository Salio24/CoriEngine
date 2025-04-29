// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "EngineEntry.hpp"
#include "Engine.hpp"

EngineEntry::EngineEntry() {
	Cori::Engine::Start();
}

EngineEntry::~EngineEntry() {
	Cori::Engine::Stop();
}
