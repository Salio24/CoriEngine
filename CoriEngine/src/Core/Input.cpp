#include "Input.hpp"

namespace Cori {

	bool Input::IsKeyPressed(CoriKeycode keycode) {
		auto state = SDL_GetKeyboardState(nullptr);
		return (state[keycode] == 1);
	}

	bool Input::IsMouseKeyPressed(CoriMouseCode button) {
		auto state = SDL_GetMouseState(nullptr, nullptr);
		return (state & SDL_BUTTON_MASK(button)) != 0;
	}

	int Input::GetMouseX() {
		float x;
		SDL_GetMouseState(&x, nullptr);
		return static_cast<int>(x);
	}

	int Input::GetMouseY() {
		float y;
		SDL_GetMouseState(nullptr, &y);
		return static_cast<int>(y);
	}

}