#pragma once
#include "EventSystem/Event.hpp"

namespace Cori {
	namespace Core {
		namespace Internal {
			class ImGuiLayer;
		}
		/**
		 * @brief A resolution the display can be driven at, always expressed in pixels.
		 * @details SDL reports a display mode as a size plus a pixel density, but exclusive fullscreen ignores the density and uses the size alone, so the size is the pixel count and m_PixelDensity is only there to tell otherwise identical modes apart. Windowed modes reach that same pixel count by dividing by the density of the display the window is on, which is what GetLogicalSizeForScreenMode does.
		 */
		struct ScreenMode {
			int32_t m_Width{};
			int32_t m_Height{};
			float m_RefreshRate{};
			float m_PixelDensity{ 1.0f };
			std::string m_ModeName;

		protected:
			friend class Window;
			ScreenMode() = default;

			ScreenMode(const int32_t pixelWidth, const int32_t pixelHeight, const float refreshRate, const float pixelDensity, const uint32_t modeIndex)
				: m_Width(pixelWidth), m_Height(pixelHeight), m_RefreshRate(refreshRate), m_PixelDensity(pixelDensity), m_SDLModeIndex(modeIndex) {
				m_ModeName = std::format("{}x{} {} Hz {} PD", m_Width, m_Height, m_RefreshRate, m_PixelDensity);
			}

			uint32_t m_SDLModeIndex{ 1000 }; //impossible initial number
		};

		enum class WindowMode {
			WINDOWED = 0,
			BORDERLESS_WINDOWED = 1,
			EXCLUSIVE_FULLSCREEN = 2,
			RESIZABLE = 3
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
			[[nodiscard]] int32_t GetPixelWidth() const;

			/**
			 * @brief Give the current window height.
			 * @return Window height in pixels.
			 */
			[[nodiscard]] int32_t GetPixelHeight() const;

			/**
			 * @brief Give the current window width.
			 * @return Window width in logical points.
			 */
			[[nodiscard]] int32_t GetLogicalWidth() const;

			/**
			 * @brief Give the current window height.
			 * @return Window height in logical points.
			 */
			[[nodiscard]] int32_t GetLogicalHeight() const;

			/**
			 * @brief Gives the ratio of pixels to logical points for this window.
			 * @return Pixel density.
			 */
			[[nodiscard]] float GetPixelDensity() const;

			/**
			 * @brief Gives the scale the user expects content to be displayed at, the pixel density and the display content scale folded together.
			 * @return Display scale.
			 */
			[[nodiscard]] float GetDisplayScale() const;

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
			 * @brief Gives the timestamp of the oldest input event consumed by the last event pump.
			 * @details In the SDL_GetTicksNS domain. On backends that expose high resolution input
			 * timestamps (Wayland via zwp_input_timestamps_v1) this is the timestamp the input stack
			 * recorded, not the moment the engine dequeued the event, so it includes the latency
			 * incurred before the event ever reached the process. Oldest rather than newest, because
			 * it is the worst case wait of anything this frame reflects.
			 * @return Timestamp in nanoseconds, or 0 when the pump consumed no input.
			 */
			[[nodiscard]] uint64_t GetOldestInputTimestamp() const;

			/**
			 * @brief Retrieves a list of all available ScreenMode.
			 * @return A const reference to the vector containing all available ScreenMode.
			 */
			[[nodiscard]] const std::vector<ScreenMode>& GetScreenModes() const;

			/**
			 * @brief Re-syncs the current ScreenMode with the size the window actually has.
			 * @details The window manager can resize the window behind our back, which leaves the selected ScreenMode describing a size the window no longer has. This picks the ScreenMode matching the real size (highest refresh rate one when several match) and persists it. Does nothing in WindowMode::RESIZABLE, where the size is the user's to change.
			 */
			void VerifyDisplayMode();

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

			/**
			 * @brief Converts the pixel size of a ScreenMode into the logical size, clamped to what the display can actually display.
			 * @param mode ScreenMode
			 * @return Width and height in logical points.
			 */
			[[nodiscard]] std::pair<int32_t, int32_t> GetLogicalSizeForScreenMode(const ScreenMode& mode) const;

		private:
			friend Internal::ImGuiLayer;
			[[nodiscard]] void* GetNativeWindow() const;

			struct WindowSaveData {
				int32_t m_Width{};
				int32_t m_Height{};
				float m_RefreshRate{};
				float m_PixelDensity{};
				WindowMode m_WindowMode{};
				uint32_t m_SDLModeIndex{};
				uint32_t m_ModeIndex{};
				bool m_Resizable{};
			};

			void SaveConfiguration() const;

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
