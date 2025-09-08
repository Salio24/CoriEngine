#pragma once
#include "Core/CoriMouseCodes.hpp"
#include "Core/CoriKeycodes.hpp"

namespace Cori {
	namespace Core {
		class Input {
		public:
			static bool IsKeyPressed(const CoriKeycode keycode);

			static bool IsMouseKeyPressed(const CoriMouseCode button);

			static int32_t GetMouseX();
			static int32_t GetMouseY();
		};
	}
}