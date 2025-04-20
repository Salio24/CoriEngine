#pragma once
#include <CoriEngine_export.hpp>
#include <imgui.h>

// This is a wrapper for ImGui function. This is needed to use them in a game/application. ImGui doesnt like being used in a dll, 
// and from that comes a lot of problems, with context access and DLL boundaries and bla-bla-bla (see imgui.cpp line 1308 for detail).
// And I have absolutely no desire to deal with this, just not worth it. Wrapper works perfectly fine, as it calls imgui functions inside of this dll.
// Not perfect, but absolutely good enough. 

class CORI_ENGINE_API IG {
public:
	inline static bool Begin(const char* name, bool* p_open = NULL, ImGuiWindowFlags flags = 0);
	inline static void End();
	inline static void Text(const char* fmt, ...);
};