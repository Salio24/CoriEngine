#include "IGWrapper.hpp"

bool IG::Begin(const char* name, bool* p_open, ImGuiWindowFlags flags) {
	return ImGui::Begin(name, p_open, flags);
}

void IG::End() {
	ImGui::End();
}

void IG::Text(const char* fmt, ...) {
	ImGui::Text(fmt);
}