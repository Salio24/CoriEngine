#pragma once
#include "Core/CoriMouseCodes.hpp"
#include "Core/CoriKeycodes.hpp"

namespace Cori {

	class Input {
	public:
		static bool IsKeyPressed(CoriKeycode keycode);

		static bool IsMouseKeyPressed(CoriMouseCode button);

		static int GetMouseX();
		static int GetMouseY();
	};
}