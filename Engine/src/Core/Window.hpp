#pragma once
#include "EventSystem/Event.hpp"
#include "Graphics/GraphicsAPIs.hpp"
#include "Profiling/Trackable.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
			class OpenGLContext;
		}
	}
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
				: m_Width(width), m_Height(height), m_RefreshRate(refreshRate), m_ModeIndex(modeIndex) {
				m_ModeName = std::to_string(m_Width) + "x" + std::to_string(m_Height) + " " + std::to_string(m_RefreshRate) + " Hz";
			}

			uint32_t m_ModeIndex{ 1000 }; //impossible initial number
		};

		enum class WindowMode {
			WINDOWED = 0,
			BORDERLESS_WINDOWED = 1,
			EXCLUSIVE_FULLSCREEN = 2
		};

		/**
		 * @brief This class manages everything that is connected with physical Window management, i might add multiwindow support later, but for now, only one Window per application.
		 */
		class Window : public Profiling::Trackable<Window> {
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
			 * @brief Returns the graphical API used by this window.
			 * @return API enumerator.
			 */
			[[nodiscard]] static Graphics::GraphicsAPIs GetCurrentAPI() { return s_API; } // NOLINT

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
			 * @brief Retrieves a list of all available ScreenMode.
			 * @return A vector containing all available ScreenMode.
			 */
			[[nodiscard]] std::vector<ScreenMode> GetScreenModes() const;

			/**
			 * @brief Changes the current ScreenMode.
			 * @param mode ScreenMode to change to.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, CoriError<>> SetScreenMode(const ScreenMode& mode);

			/**
			 * @brief Gets the current WindowMode;
			 * @return Enumerator of the current WindowMode.
			 */
			[[nodiscard]] WindowMode GetWindowMode() const;

			/**
			 * @brief Changes the current WindowMode.
			 * @param mode WindowMode enumerator to change to.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, CoriError<>> SetWindowMode(WindowMode mode);

		private:
			friend Internal::ImGuiLayer;
			friend Graphics::Internal::OpenGLContext;
			[[nodiscard]] void* GetNativeContext() const;
			[[nodiscard]] void* GetNativeWindow() const;

			friend class Application;
			[[nodiscard]] static std::unique_ptr<Window> Create(std::string name, const bool vsync = false);
			void OnUpdate();
			void SetEventCallback(const EventCallbackFn& callback);

			explicit Window(std::string title, const bool vsync = false);

			inline static auto s_API = Graphics::GraphicsAPIs::OpenGL;
			struct Data;
			Data* m_Data{nullptr};
		};
	}
}
