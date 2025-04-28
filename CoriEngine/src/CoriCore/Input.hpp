#pragma once
#include "../src/CoriCore/EventSystem/CoriMouseCodes.hpp"
#include "../src/CoriCore/EventSystem/CoriKeycodes.hpp"

namespace Cori {
	class Input {
	public:
		inline static bool IsKeyPressed(CoriKeycode keycode) { return s_Instance->IsKeyPressedImpl(keycode); }
		
		inline static bool IsMouseKeyPressed(CoriMouseCode button) { return s_Instance->IsMouseKeyPressedImpl(button); }

		inline static int GetMouseX() { return s_Instance->GetMouseXImpl(); }
		inline static int GetMouseY() { return s_Instance->GetMouseYImpl(); }
	protected:
		bool IsKeyPressedImpl(CoriKeycode keycode);

		bool IsMouseKeyPressedImpl(CoriMouseCode button);

		int GetMouseXImpl();
		int GetMouseYImpl();

	private:
		static Input* s_Instance;
	};
}