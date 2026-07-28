#include "Input.hpp"
#include "Application.hpp"

namespace Cori {
	namespace Core {
		bool Input::IsKeyDown(const CoriKeycode keycode) {
			const auto state = SDL_GetKeyboardState(nullptr);
			return state[keycode] == 1;
		}

		bool Input::IsMouseKeyDown(const CoriMouseKeycode button) {
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

		glm::ivec2 Input::GetMousePosition() {
			float x, y;
			SDL_GetMouseState(&x, &y);
			return { x, y };
		}

		glm::vec2 Input::GetMouseDelta() {
			return Application::GetWindow().GetMouseDelta();
		}

		bool Input::SetRelativeMouseMode(const bool status) {
			return Application::GetWindow().SetRelativeMouseMode(status);
		}

		bool Input::IsRelativeMouseMode() {
			return Application::GetWindow().IsRelativeMouseMode();
		}
	}
}