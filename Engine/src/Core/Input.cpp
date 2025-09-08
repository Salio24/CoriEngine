#include "Input.hpp"

namespace Cori {
	namespace Core {
		bool Input::IsKeyPressed(const CoriKeycode keycode) {
			const auto state = SDL_GetKeyboardState(nullptr);
			return state[keycode] == 1;
		}

		bool Input::IsMouseKeyPressed(const CoriMouseCode button) {
			const auto state = SDL_GetMouseState(nullptr, nullptr);
			return (state & SDL_BUTTON_MASK(button)) != 0;
		}

		int32_t Input::GetMouseX() {
			float x;
			SDL_GetMouseState(&x, nullptr);
			return static_cast<int32_t>(x);
		}

		int32_t Input::GetMouseY() {
			float y;
			SDL_GetMouseState(nullptr, &y);
			return static_cast<int32_t>(y);
		}
	}
}