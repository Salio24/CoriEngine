#pragma once
#include "EventSystem/Event.hpp"

namespace Cori {
	namespace Core {
		namespace Internal {
			class ImGuiLayer;
		}
		struct ScreenMode {
			int32_t m_Width{};
			int32_t m_Height{};
			float m_RefreshRate{};
			std::string m_ModeName;

		protected:
			friend class Window;
			ScreenMode() = default;

			ScreenMode(const int32_t width, const int32_t height, const float refreshRate, const uint32_t modeIndex)
				: m_Width(width), m_Height(height), m_RefreshRate(refreshRate), m_SDLModeIndex(modeIndex) {
				m_ModeName = std::to_string(m_Width) + "x" + std::to_string(m_Height) + " " + std::to_string(m_RefreshRate) + " Hz";
			}

			uint32_t m_SDLModeIndex{ 1000 }; //impossible initial number
		};

		enum class WindowMode {
			WINDOWED = 0,
			BORDERLESS_WINDOWED = 1,
			EXCLUSIVE_FULLSCREEN = 2
		};

		/**
		 * @brief This class manages everything that is connected with physical Window management, i might add multiwindow support later, but for now, only one Window per application.
		 */
		class Window {
		public:
			~Window();

			/**
			 * @brief Give the current window width.
			 * @return Window width in pixels.
			 */
			[[nodiscard]] int32_t GetWidth() const;

			/**
			 * @brief Give the current window height.
			 * @return Window height in pixels.
			 */
			[[nodiscard]] int32_t GetHeight() const;

			/**
			 * @brief Changes the VSync state.
			 * @param status True enable, false disable.
			 */
			void SetVSync(const bool status);

			/**
			 * @brief Checks is VSynch is currently enabled.
			 * @return True enabled, false disabled.
			 */
			[[nodiscard]] bool IsVSync() const;

			/**
			 * @brief Enables or disables relative mouse mode.
			 * @details While it is on the cursor is hidden and confined to the window, and the mouse reports movement deltas instead of a position, which is what a camera wants. Turning it back off returns the cursor to where it was when the mode was entered.
			 * @param status True enable, false disable.
			 * @return True on success, false otherwise.
			 */
			bool SetRelativeMouseMode(const bool status);

			/**
			 * @brief Checks whether relative mouse mode is currently enabled.
			 * @return True enabled, false disabled.
			 */
			[[nodiscard]] bool IsRelativeMouseMode() const;

			/**
			 * @brief Gives how far the mouse moved during the last event pump, in pixels.
			 * @details Reported in both mouse modes, but only relative mode keeps producing motion once the cursor would have run into the edge of the screen. Reading it twice in a frame gives the same answer, it is replaced by the next pump rather than consumed.
			 * @return Movement delta since the previous pump.
			 */
			[[nodiscard]] glm::vec2 GetMouseDelta() const;

			/**
			 * @brief Retrieves a list of all available ScreenMode.
			 * @return A const reference to the vector containing all available ScreenMode.
			 */
			[[nodiscard]] const std::vector<ScreenMode>& GetScreenModes() const;

			/**
			 * @brief Changes the current ScreenMode.
			 * @param modeIndex Index of the screen mode in the screen modes vector (obtainable via GetScreenModes).
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, CoriError<>> SetScreenMode(const uint32_t modeIndex);

			/**
			 * @brief Gets the current WindowMode;
			 * @return Enumerator of the current WindowMode.
			 */
			[[nodiscard]] WindowMode GetWindowMode() const;

			/**
			 * @brief Retrieves the current screen mode index.
			 * @return Index of the current screen mode in the screen modes vector (obtainable via GetScreenModes).
			 */
			[[nodiscard]] uint32_t GetCurrentScreenMode() const;

			/**
			 * @brief Changes the current WindowMode.
			 * @param mode WindowMode enumerator to change to.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, CoriError<>> SetWindowMode(WindowMode mode);

		private:
			friend Internal::ImGuiLayer;
			[[nodiscard]] void* GetNativeWindow() const;

			struct WindowSaveData {
				int32_t m_Width{};
				int32_t m_Height{};
				float m_RefreshRate{};
				WindowMode m_WindowMode{};
				uint32_t m_SDLModeIndex{};
				uint32_t m_ModeIndex{};
			};

			friend class Application;
			[[nodiscard]] static std::unique_ptr<Window> Create(std::string name, const bool vsync = false);
			void OnUpdate();
			void SetEventCallback(const EventCallbackFn& callback);

			explicit Window(std::string title, const bool vsync = false);

			struct Data;
			Data* m_Data{nullptr};
		};
	}
}
