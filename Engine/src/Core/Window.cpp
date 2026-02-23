#include "Window.hpp"
#include "Graphics/RenderingContext.hpp"
#include <backends/imgui_impl_sdl3.h>
#include <SDL3_image/SDL_image.h>
#include "FileSystem/PathManager.hpp"
#include "EventSystem/AppEvent.hpp"
#include "EventSystem/KeyEvent.hpp"
#include "EventSystem/MouseEvent.hpp"
#include "FileSystem/BinaryFileManager.hpp"

namespace Cori {
	namespace Core {
		struct Window::Data {
			std::vector<ScreenMode> m_ScreenModes;
			uint32_t m_CurrentScreenMode;
			WindowMode m_CurrentWindowMode{ WindowMode::EXCLUSIVE_FULLSCREEN };
			std::string m_WindowTitle;

			int32_t m_DisplayModeCount;
			SDL_DisplayMode** m_SDLModes{ nullptr };
			SDL_DisplayID m_PrimaryDisplayID{};

			SDL_Window* m_Window{ nullptr };
			bool m_VSync{ false };

			EventCallbackFn m_EventCallback;

			~Data() {
				SDL_DestroyWindow(m_Window);
				SDL_free(m_SDLModes);
			}
		};

		std::unique_ptr<Window> Window::Create(std::string name, const bool vsync) {
			return std::unique_ptr<Window>(new Window(std::move(name), vsync));
		}

		Window::Window(std::string title, const bool vsync) {
			m_Data = new Data();
			m_Data->m_WindowTitle = std::move(title);
			m_Data->m_VSync = vsync;

			const SDL_DisplayID primaryDisplayID = SDL_GetPrimaryDisplay();
			if (primaryDisplayID == 0) {
				CORI_CORE_FATAL_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to get primary display ID. SDL_Error: {}", SDL_GetError());
			}
			else {
				m_Data->m_PrimaryDisplayID = primaryDisplayID;
			}

			m_Data->m_SDLModes = SDL_GetFullscreenDisplayModes(m_Data->m_PrimaryDisplayID, &m_Data->m_DisplayModeCount);

			CORI_CORE_VERIFY(m_Data->m_SDLModes, "Failed to get screen modes, Window '{}' can not be created. SDL_Error: {}",  title, SDL_GetError());

			for (int32_t i = 0; i < m_Data->m_DisplayModeCount; i++) {
				ScreenMode mode(m_Data->m_SDLModes[i]->w, m_Data->m_SDLModes[i]->h, m_Data->m_SDLModes[i]->refresh_rate, i);
				m_Data->m_ScreenModes.emplace_back(mode);
			}

			ScreenMode mode = m_Data->m_ScreenModes[0];

			const std::filesystem::path savePath = FileSystem::PathManager::GetAliasedPath("USER_DATA") / "settings/window.bin";
			std::filesystem::create_directories(savePath.parent_path());

			bool configLoaded = false;

			if (std::filesystem::exists(savePath)) {
				auto data = FileSystem::BinaryFileManager::LoadAggregateStruct<WindowSaveData>(savePath);
				if (data) {
					if (m_Data->m_DisplayModeCount >= data->m_SDLModeIndex) {
						if (m_Data->m_SDLModes[data->m_SDLModeIndex]->w == data->m_Width && m_Data->m_SDLModes[data->m_SDLModeIndex]->h == data->m_Height && m_Data->m_SDLModes[data->m_SDLModeIndex]->refresh_rate == data->m_RefreshRate &&
							m_Data->m_ScreenModes[data->m_ModeIndex].m_Width == data->m_Width && m_Data->m_ScreenModes[data->m_ModeIndex].m_Height == data->m_Height && m_Data->m_ScreenModes[data->m_ModeIndex].m_RefreshRate == data->m_RefreshRate) {
							mode.m_Width = data->m_Width;
							mode.m_Height = data->m_Height;
							mode.m_RefreshRate = data->m_RefreshRate;
							mode.m_SDLModeIndex = data->m_SDLModeIndex;
							m_Data->m_CurrentWindowMode = data->m_WindowMode;
							m_Data->m_CurrentScreenMode = data->m_ModeIndex;
							configLoaded = true;
						}
					}
				}
			}

			if (!configLoaded) {
				m_Data->m_CurrentScreenMode = 0;
			}

			SDL_Rect displayBounds;
			const bool success = SDL_GetDisplayBounds(m_Data->m_PrimaryDisplayID, &displayBounds);
			if (!success) {
				CORI_CORE_FATAL_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to get primary display bounds. SDL_Error: {}", SDL_GetError());
			}

			const SDL_PropertiesID props = SDL_CreateProperties();
			SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, m_Data->m_WindowTitle.c_str());
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, m_Data->m_ScreenModes[0].m_Width);
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, m_Data->m_ScreenModes[0].m_Height);
			//SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 800);
			//SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 600);
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, displayBounds.x);
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, displayBounds.y);
			SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, false);
			SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);

			m_Data->m_Window = SDL_CreateWindowWithProperties(props);

			SDL_DestroyProperties(props);

			CORI_CORE_ASSERT(m_Data->m_Window, "Failed to create Window '{}'. SDL_Error: {}", m_Data->m_WindowTitle, SDL_GetError());

			const auto logoPath = FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / std::filesystem::path("ui/logo256.png");
			SDL_Surface* logo = IMG_Load(logoPath.string().c_str());

			if (!logo) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to load App Logo: {}", SDL_GetError());
			} else {
				SDL_SetWindowIcon(m_Data->m_Window, logo);
			}

			SDL_DestroySurface(logo);

			SDL_SetWindowPosition(m_Data->m_Window, SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID), SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID));

			SetScreenMode(m_Data->m_CurrentScreenMode);

			const WindowSaveData data{ .m_Width = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Width, .m_Height = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Height, .m_RefreshRate = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_RefreshRate, .m_WindowMode = m_Data->m_CurrentWindowMode, .m_SDLModeIndex = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_SDLModeIndex, .m_ModeIndex = m_Data->m_CurrentScreenMode };
			FileSystem::BinaryFileManager::SaveAggregateStruct(data, savePath);

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Window '{}' Created", m_Data->m_WindowTitle);
		}

		Window::~Window() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Window '{}' Destroyed", m_Data->m_WindowTitle);
			delete m_Data;
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void Window::OnUpdate() {
			CORI_PROFILE_FUNCTION();
			SDL_Event e;

			while (SDL_PollEvent(&e)) {
				ImGui_ImplSDL3_ProcessEvent(&e);

				// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
				switch (e.type) {
				case SDL_EVENT_WINDOW_RESIZED:
					{
						if (e.window.windowID != SDL_GetWindowID(m_Data->m_Window)) {
							break;
						}

						WindowResizeEvent resizeEvent(GetWidth(), GetHeight());
						m_Data->m_EventCallback(resizeEvent);

						break;
					}
				case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
					{
						WindowCloseEvent closeEvent;
						m_Data->m_EventCallback(closeEvent);
						break;
					}
				case SDL_EVENT_KEY_DOWN:
					{
						KeyPressedEvent keyPressedEvent(static_cast<CoriKeycode>(e.key.scancode), e.key.repeat);
						m_Data->m_EventCallback(keyPressedEvent);
						break;
					}
				case SDL_EVENT_KEY_UP:
					{
						KeyReleasedEvent keyReleasedEvent(static_cast<CoriKeycode>(e.key.scancode));
						m_Data->m_EventCallback(keyReleasedEvent);
						break;
					}
				case SDL_EVENT_MOUSE_MOTION:
					{
						MouseMovedEvent mouseMovedEvent(static_cast<int32_t>(e.motion.x), static_cast<int32_t>(e.motion.y));
						m_Data->m_EventCallback(mouseMovedEvent);
						break;
					}
				case SDL_EVENT_MOUSE_WHEEL:
					{
						MouseScrolledEvent mouseScrolledEvent(static_cast<int16_t>(e.wheel.x), static_cast<int16_t>(e.wheel.y));
						m_Data->m_EventCallback(mouseScrolledEvent);
						break;
					}
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
					{
						MouseButtonPressedEvent mouseButtonPressedEvent(static_cast<CoriMouseKeycode>(e.button.button));
						m_Data->m_EventCallback(mouseButtonPressedEvent);
						break;
					}
				case SDL_EVENT_MOUSE_BUTTON_UP:
					{
						MouseButtonReleasedEvent mouseButtonReleasedEvent(static_cast<CoriMouseKeycode>(e.button.button));
						m_Data->m_EventCallback(mouseButtonReleasedEvent);
						break;
					}
				}
			}
		}

		int32_t Window::GetWidth() const {
			if (m_Data->m_CurrentWindowMode != WindowMode::BORDERLESS_WINDOWED) {
				return m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Width;
			}
			return m_Data->m_ScreenModes[0].m_Width;
		}

		int32_t Window::GetHeight() const {
			if (m_Data->m_CurrentWindowMode != WindowMode::BORDERLESS_WINDOWED) {
				return m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Height;
			}
			return m_Data->m_ScreenModes[0].m_Height;
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void Window::SetEventCallback(const EventCallbackFn& callback) {
			m_Data->m_EventCallback = callback;
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void Window::SetVSync(const bool status) {
			if (!status) {
				SDL_GL_SetSwapInterval(0);
			}
			else {
				SDL_GL_SetSwapInterval(1);
			}

			m_Data->m_VSync = status;
			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "VSync is now set to: {}", status);
		}

		bool Window::IsVSync() const {
			return m_Data->m_VSync;
		}

		void* Window::GetNativeWindow() const {
			return m_Data->m_Window;
		}

		const std::vector<ScreenMode>& Window::GetScreenModes() const {
			return m_Data->m_ScreenModes;
		}

		std::expected<void, CoriError<>> Window::SetScreenMode(const uint32_t modeIndex) {
			m_Data->m_CurrentScreenMode = modeIndex;
			return SetWindowMode(m_Data->m_CurrentWindowMode);
		}

		WindowMode Window::GetWindowMode() const {
			return m_Data->m_CurrentWindowMode;
		}

		uint32_t Window::GetCurrentScreenMode() const {
			return m_Data->m_CurrentScreenMode;
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		std::expected<void, CoriError<>> Window::SetWindowMode(const WindowMode mode) {
			bool success;
			switch (mode) {
			case WindowMode::WINDOWED:
				{
					success = SDL_SetWindowFullscreen(m_Data->m_Window, false);
					if (!success) {
						return std::unexpected(CoriError(std::format("Failed to set window mode to 'Windowed'. SDL_Error: {}", SDL_GetError())));
					}
					const bool borderAdded = SDL_SetWindowBordered(m_Data->m_Window, true);
					if (!borderAdded) {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to add border to the window. SDL_Error: {}", SDL_GetError());
					}

					const SDL_DisplayMode* desktopMode = SDL_GetCurrentDisplayMode(m_Data->m_PrimaryDisplayID);

					if (m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Width <= desktopMode->w && m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Height <= desktopMode->h) {
						success = SDL_SetWindowSize(m_Data->m_Window, m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Width, m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Height);
						if (!success) {
							return std::unexpected(CoriError(std::format("Failed to set window mode to 'Windowed'. SDL_Error: {}", SDL_GetError())));
						}
					}
					else {
						success = SDL_SetWindowSize(m_Data->m_Window, desktopMode->w, desktopMode->h);
						if (!success) {
							return std::unexpected(CoriError(std::format("Failed to set window mode to 'Windowed'. SDL_Error: {}", SDL_GetError())));
						}
					}

					const bool windowMoveSuccess = SDL_SetWindowPosition(m_Data->m_Window, SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID), SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID));
					if (!windowMoveSuccess) {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to set window position to the center of the main screen. (This is expected on Wayland) SDL_Error: {}", SDL_GetError());
					}

					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Window set to 'Windowed' mode. Screen mode: (Width: {}, Height: {})", m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Width, m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Height);

					m_Data->m_CurrentWindowMode = mode;

					const std::filesystem::path savePath = FileSystem::PathManager::GetAliasedPath("USER_DATA") / "settings/window.bin";
					std::filesystem::create_directories(savePath.parent_path());
					const WindowSaveData data{ .m_Width = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Width, .m_Height = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Height, .m_RefreshRate = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_RefreshRate, .m_WindowMode = m_Data->m_CurrentWindowMode, .m_SDLModeIndex = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_SDLModeIndex, .m_ModeIndex = m_Data->m_CurrentScreenMode };
					FileSystem::BinaryFileManager::SaveAggregateStruct(data, savePath);

					break;
				}
			case WindowMode::BORDERLESS_WINDOWED:
				{
					const bool windowMoveSuccess = SDL_SetWindowPosition(m_Data->m_Window, SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID), SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID));
					if (!windowMoveSuccess) {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to set window position to the center of the main screen. SDL_Error: {}", SDL_GetError());
					}

					success = SDL_SetWindowFullscreen(m_Data->m_Window, true);
					if (!success) {
						return std::unexpected(CoriError(std::format("Failed to set window mode to 'Borderless Windowed'. SDL_Error: {}", SDL_GetError())));
					}
					success = SDL_SetWindowFullscreenMode(m_Data->m_Window, nullptr);
					if (!success) {
						return std::unexpected(CoriError(std::format("Failed to set window mode to 'Borderless Windowed'. SDL_Error: {}", SDL_GetError())));
					}

					m_Data->m_CurrentWindowMode = mode;

					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Window set to 'Borderless Windowed' mode.");

					const std::filesystem::path savePath = FileSystem::PathManager::GetAliasedPath("USER_DATA") / "settings/window.bin";
					std::filesystem::create_directories(savePath.parent_path());
					const WindowSaveData data{ .m_Width = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Width, .m_Height = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Height, .m_RefreshRate = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_RefreshRate, .m_WindowMode = m_Data->m_CurrentWindowMode, .m_SDLModeIndex = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_SDLModeIndex, .m_ModeIndex = m_Data->m_CurrentScreenMode };
					FileSystem::BinaryFileManager::SaveAggregateStruct(data, savePath);

					break;
				}
			case WindowMode::EXCLUSIVE_FULLSCREEN:
				{
					const bool windowMoveSuccess = SDL_SetWindowPosition(m_Data->m_Window, SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID), SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID));
					if (!windowMoveSuccess) {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to set window position to the center of the main screen. (This is expected on Wayland) SDL_Error: {}", SDL_GetError());
					}

					const SDL_DisplayMode* sdlMode = m_Data->m_SDLModes[m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_SDLModeIndex];
					if (!sdlMode) {
						return std::unexpected(CoriError(std::format("Failed to set window mode to 'Exclusive Fullscreen'. SDL_Error: {}", SDL_GetError())));
					}
					success = SDL_SetWindowFullscreen(m_Data->m_Window, true);
					if (!success) {
						return std::unexpected(CoriError(std::format("Failed to set window mode to 'Exclusive Fullscreen'. SDL_Error: {}", SDL_GetError())));
					}
					success = SDL_SetWindowFullscreenMode(m_Data->m_Window, sdlMode);
					if (!success) {
						return std::unexpected(CoriError(std::format("Failed to set window mode to 'Exclusive Fullscreen'. SDL_Error: {}", SDL_GetError())));
					}

					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Window set to 'Exclusive Fullscreen' mode. Screen mode: (Width: {}, Height: {}, Refresh Rate: {})", sdlMode->w, sdlMode->h, sdlMode->refresh_rate);

					m_Data->m_CurrentWindowMode = mode;

					const std::filesystem::path savePath = FileSystem::PathManager::GetAliasedPath("USER_DATA") / "settings/window.bin";
					std::filesystem::create_directories(savePath.parent_path());
					const WindowSaveData data{ .m_Width = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Width, .m_Height = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Height, .m_RefreshRate = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_RefreshRate, .m_WindowMode = m_Data->m_CurrentWindowMode, .m_SDLModeIndex = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_SDLModeIndex, .m_ModeIndex = m_Data->m_CurrentScreenMode };
					FileSystem::BinaryFileManager::SaveAggregateStruct(data, savePath);

					break;
				}
			}

			SDL_SyncWindow(m_Data->m_Window);

			return {};
		}
	}
}
