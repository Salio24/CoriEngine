// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Input.hpp"

namespace Cori {

	Input& Input::Get() {
		static Input instance;
		return instance;
	}

	bool Input::IsKeyPressedImpl(CoriKeycode keycode) {
		auto state = SDL_GetKeyboardState(nullptr);
		return (state[keycode] == 1);
	}

	bool Input::IsMouseKeyPressedImpl(CoriMouseCode button) {
		auto state = SDL_GetMouseState(nullptr, nullptr);
		return (state & SDL_BUTTON_MASK(button)) != 0;
	}

	int Input::GetMouseXImpl() {
		float x;
		SDL_GetMouseState(&x, nullptr);
		return static_cast<int>(x);
	}

	int Input::GetMouseYImpl() {
		float y;
		SDL_GetMouseState(nullptr, &y);
		return static_cast<int>(y);
	}

}