#pragma once
#include "Core/CoriMouseCodes.hpp"
#include "Core/CoriKeycodes.hpp"

namespace Cori {
	namespace Core {
		class Input {
		public:

			/**
			 * @brief Checks if a specific keyboard key is down.
			 * @param keycode Said key keycode.
			 * @return True if down, false otherwise.
			 */
			static bool IsKeyDown(const CoriKeycode keycode);

			/**
			 * @brief Checks if a specific mouse key is down.
			 * @param keycode Said mouse key key keycode.
			 * @return True if down, false otherwise.
			 */
			static bool IsMouseKeyDown(const CoriMouseKeycode keycode);


			/**
			 * @brief Retrieves the current mouse X position on screen.
			 * @return Mouse position on X in screen coordinates.
			 */
			static int32_t GetMouseX();

			/**
			 * @brief Retrieves the current mouse Y position on screen.
			 * @return Mouse position on Y in screen coordinates.
			 */
			static int32_t GetMouseY();

			/**
			 * @brief Retrieves the current mouse position on screen.
			 * @return Mouse position in screen coordinates.
			 */
			static glm::ivec2 GetMousePosition();
		};
	}
}