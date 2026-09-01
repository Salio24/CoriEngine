#pragma once
#include "Core/CoriMouseCodes.hpp"
#include "Core/CoriKeycodes.hpp"

namespace Cori {
	namespace Core {
		/**
		 * @brief A simple static class that allows to query for physical keyboard or mose inputs.
		 */
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
			 * @return Mouse position on X in screen pixel coordinate space.
			 */
			static float GetMouseX();

			/**
			 * @brief Retrieves the current mouse Y position on screen.
			 * @return Mouse position on Y in screen pixel coordinate space.
			 */
			static float GetMouseY();

			/**
			 * @brief Retrieves the current mouse position on screen.
			 * @return Mouse position in screen pixel coordinate space.
			 */
			static glm::vec2 GetMousePosition();

			/**
			 * @brief Retrieves how far the mouse moved during the last event pump.
			 * @details Prefer this over differencing GetMousePosition for anything that turns a camera: it keeps working once the cursor would have hit the edge of the screen, provided relative mouse mode is on. Reading it twice in a frame gives the same answer.
			 * @return Movement delta in pixels.
			 */
			static glm::vec2 GetMouseDelta();

			/**
			 * @brief Enables or disables relative mouse mode.
			 * @details While it is on the cursor is hidden and confined to the window, and the mouse only reports deltas. Turning it back off returns the cursor to where it was when the mode was entered.
			 * @param status
			 * @returns True on success, false otherwise.
			 */
			static bool SetRelativeMouseMode(const bool status);

			/**
			 * @brief Checks whether relative mouse mode is currently enabled.
			 * @return True enabled, false disabled.
			 */
			static bool IsRelativeMouseMode();
		};
	}
}