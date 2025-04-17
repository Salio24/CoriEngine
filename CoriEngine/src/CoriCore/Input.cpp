#include "Input.hpp"

namespace Cori {

	Input* Input::s_Instance = new Input();

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
		return (int)x;
	}

	int Input::GetMouseYImpl() {
		float y;
		SDL_GetMouseState(nullptr, &y);
		return (int)y;
	}

}