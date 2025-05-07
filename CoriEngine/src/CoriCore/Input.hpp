#pragma once
#include "../src/CoriCore/EventSystem/CoriMouseCodes.hpp"
#include "../src/CoriCore/EventSystem/CoriKeycodes.hpp"

namespace Cori {

	class Input {
	public:
		inline static bool IsKeyPressed(CoriKeycode keycode) { return Get().IsKeyPressedImpl(keycode); }
		
		inline static bool IsMouseKeyPressed(CoriMouseCode button) { return Get().IsMouseKeyPressedImpl(button); }

		inline static int GetMouseX() { return Get().GetMouseXImpl(); }
		inline static int GetMouseY() { return Get().GetMouseYImpl(); }

	private:
		Input() = default;
		~Input() = default;

		Input(const Input&) = delete;
		Input& operator=(const Input&) = delete;
		Input(Input&&) = delete;
		Input& operator=(Input&&) = delete;

		static Input& Get();

		bool IsKeyPressedImpl(CoriKeycode keycode);

		bool IsMouseKeyPressedImpl(CoriMouseCode button);

		int GetMouseXImpl();
		int GetMouseYImpl();
	};
}