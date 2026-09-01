#include "Window.hpp"
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

			SDL_Window* m_Window{ nullptr };

			glm::vec2 m_MouseDelta{ 0.0f };

			uint64_t m_OldestInputTimestamp{ 0 };

			EventCallbackFn m_EventCallback;

			SDL_DisplayMode** m_SDLModes{ nullptr };
			SDL_DisplayID m_PrimaryDisplayID{};

			int32_t m_DisplayModeCount;
			bool m_VSync{ false };
			bool m_WindowResizable{ false };

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

			const SDL_DisplayMode* desktopMode = SDL_GetDesktopDisplayMode(m_Data->m_PrimaryDisplayID);
			const float desktopPixelDensity = (desktopMode && desktopMode->pixel_density > 0.0f) ? desktopMode->pixel_density : 1.0f;

			for (int32_t i = 0; i < m_Data->m_DisplayModeCount; i++) {
				const SDL_DisplayMode* sdlMode = m_Data->m_SDLModes[i];
				const float pixelDensity = sdlMode->pixel_density > 0.0f ? sdlMode->pixel_density : 1.0f;

				ScreenMode mode(sdlMode->w, sdlMode->h, sdlMode->refresh_rate, pixelDensity, i);

				const auto duplicate = std::ranges::find_if(m_Data->m_ScreenModes, [&mode](const ScreenMode& existing) {
					return existing.m_Width == mode.m_Width && existing.m_Height == mode.m_Height && existing.m_RefreshRate == mode.m_RefreshRate;
				});

				if (duplicate == m_Data->m_ScreenModes.end()) {
					m_Data->m_ScreenModes.emplace_back(mode);
				}
				else if (std::abs(mode.m_PixelDensity - desktopPixelDensity) < std::abs(duplicate->m_PixelDensity - desktopPixelDensity)) {
					*duplicate = mode;
				}
			}

			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Display {} reports {} display modes, which collapse into {} distinct resolutions.", m_Data->m_PrimaryDisplayID, m_Data->m_DisplayModeCount, m_Data->m_ScreenModes.size());

			ScreenMode mode = m_Data->m_ScreenModes[0];

			const std::filesystem::path savePath = FileSystem::PathManager::GetAliasedPath("USER_DATA") / "settings/window.bin";
			std::filesystem::create_directories(savePath.parent_path());

			bool configLoaded = false;

			if (std::filesystem::exists(savePath)) {
				auto data = FileSystem::BinaryFileManager::LoadAggregateStruct<WindowSaveData>(savePath);
				if (!data) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to read the saved window configuration from '{}', falling back to the default screen mode. Error: {}", savePath.string(), data.error().what());
				}
				else if (data->m_SDLModeIndex >= static_cast<uint32_t>(m_Data->m_DisplayModeCount) || data->m_ModeIndex >= m_Data->m_ScreenModes.size()) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "The saved window configuration points at screen mode {} (SDL mode {}), but this display only reports {} resolutions across {} modes. Falling back to the default screen mode.", data->m_ModeIndex, data->m_SDLModeIndex, m_Data->m_ScreenModes.size(), m_Data->m_DisplayModeCount);
				}
				else if (const ScreenMode& savedMode = m_Data->m_ScreenModes[data->m_ModeIndex];
					savedMode.m_Width != data->m_Width || savedMode.m_Height != data->m_Height || savedMode.m_RefreshRate != data->m_RefreshRate || savedMode.m_PixelDensity != data->m_PixelDensity || savedMode.m_SDLModeIndex != data->m_SDLModeIndex) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "The saved window configuration ({}x{} pixels, {} Hz, {}x pixel density) no longer describes the mode it was saved under, the display setup has most likely changed. Falling back to the default screen mode.", data->m_Width, data->m_Height, data->m_RefreshRate, data->m_PixelDensity);
				}
				else {
					mode.m_Width = data->m_Width;
					mode.m_Height = data->m_Height;
					mode.m_RefreshRate = data->m_RefreshRate;
					mode.m_PixelDensity = data->m_PixelDensity;
					mode.m_SDLModeIndex = data->m_SDLModeIndex;
					m_Data->m_CurrentWindowMode = data->m_WindowMode;
					m_Data->m_CurrentScreenMode = data->m_ModeIndex;
					m_Data->m_WindowResizable = data->m_Resizable;
					configLoaded = true;
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
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, std::lround(static_cast<float>(m_Data->m_ScreenModes[0].m_Width) / desktopPixelDensity));
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, std::lround(static_cast<float>(m_Data->m_ScreenModes[0].m_Height) / desktopPixelDensity));
			//SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 800);
			//SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 600);
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, displayBounds.x);
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, displayBounds.y);
			SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, false);
			SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);
			//SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
			SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);


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

			if (const auto result = SetScreenMode(m_Data->m_CurrentScreenMode); !result) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to apply the startup screen mode to window '{}'. Error: {}", m_Data->m_WindowTitle, result.error().what());
			}

			SaveConfiguration();

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Window '{}' Created", m_Data->m_WindowTitle);
		}

		Window::~Window() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Window '{}' Destroyed", m_Data->m_WindowTitle);
			delete m_Data;
		}

		// ReSharper disable once CppMemberFunctionMayBeConst
		void Window::OnUpdate() {
			CORI_PROFILE_FUNCTION();
			VerifyDisplayMode();
			SDL_Event e;
			SDL_Event wheelEvent{};
			bool hasWheel = false;

			static bool test_{ false };

			m_Data->m_MouseDelta = { 0.0f, 0.0f };
			m_Data->m_OldestInputTimestamp = 0;

			const float pixelDensity = GetPixelDensity();

			while (SDL_PollEvent(&e)) {
				if (e.type == SDL_EVENT_MOUSE_MOTION) {
					e.motion.x *= pixelDensity;
					e.motion.y *= pixelDensity;
				}

				if (e.type == SDL_EVENT_MOUSE_WHEEL) {
					if (!hasWheel) { wheelEvent = e; hasWheel = true; }
					else { wheelEvent.wheel.x += e.wheel.x; wheelEvent.wheel.y += e.wheel.y; }
				}
				else {
					ImGui_ImplSDL3_ProcessEvent(&e);
				}

				switch (e.type) {
				case SDL_EVENT_KEY_DOWN:
				case SDL_EVENT_KEY_UP:
				case SDL_EVENT_MOUSE_MOTION:
				case SDL_EVENT_MOUSE_WHEEL:
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
				case SDL_EVENT_MOUSE_BUTTON_UP:
					{
						if (e.common.timestamp != 0 && (m_Data->m_OldestInputTimestamp == 0 || e.common.timestamp < m_Data->m_OldestInputTimestamp)) {
							m_Data->m_OldestInputTimestamp = e.common.timestamp;
						}
						break;
					}
				default:
					break;
				}

				// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
				switch (e.type) {
				case SDL_EVENT_WINDOW_RESIZED:
					{
						if (e.window.windowID != SDL_GetWindowID(m_Data->m_Window)) {
							break;
						}

						WindowLogicalResizeEvent resizeEvent(GetLogicalWidth(), GetLogicalHeight());
						m_Data->m_EventCallback(resizeEvent);

						break;
					}
				case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
					{
						if (e.window.windowID != SDL_GetWindowID(m_Data->m_Window)) {
							break;
						}

						WindowPixelResizeEvent resizeEvent(GetPixelWidth(), GetPixelHeight());
						m_Data->m_EventCallback(resizeEvent);

						break;
					}
				case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
					{
						if (e.window.windowID != SDL_GetWindowID(m_Data->m_Window)) {
							break;
						}

						WindowDisplayScaleChangedEvent resizeEvent(GetDisplayScale());
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
						if (keyPressedEvent.GetKeyCode() == CORI_KEY_O) {
							//test_ = !test_;
							//CORI_WARN("change2");
						}
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
						m_Data->m_MouseDelta += glm::vec2{ e.motion.xrel, e.motion.yrel };
						if (test_) {
							CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Mouse motion: pos ({}, {}), rel ({}, {}), mouse '{}' (ID {}), windowID {} (ours {}), relative mode {}, cursor visible {}", e.motion.x, e.motion.y, e.motion.xrel, e.motion.yrel, SDL_GetMouseNameForID(e.motion.which) ? SDL_GetMouseNameForID(e.motion.which) : "unknown", e.motion.which, e.motion.windowID, SDL_GetWindowID(m_Data->m_Window), SDL_GetWindowRelativeMouseMode(m_Data->m_Window), SDL_CursorVisible());
						}

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

			if (hasWheel) {
				ImGui_ImplSDL3_ProcessEvent(&wheelEvent);
			}

			if (test_) {
				if (m_Data->m_MouseDelta != glm::vec2{ 0.0f, 0.0f }) {
					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Mouse delta for this pump: ({}, {})", m_Data->m_MouseDelta.x, m_Data->m_MouseDelta.y);
				}
			}
		}

		int32_t Window::GetPixelWidth() const {
			int x, y;
			SDL_GetWindowSizeInPixels(m_Data->m_Window, &x, &y);
			return x;
		}

		int32_t Window::GetPixelHeight() const {
			int x, y;
			SDL_GetWindowSizeInPixels(m_Data->m_Window, &x, &y);
			return y;
		}

		int32_t Window::GetLogicalWidth() const {
			int x, y;
			SDL_GetWindowSize(m_Data->m_Window, &x, &y);
			return x;
		}

		int32_t Window::GetLogicalHeight() const {
			int x, y;
			SDL_GetWindowSize(m_Data->m_Window, &x, &y);
			return y;
		}

		float Window::GetPixelDensity() const {
			const float pixelDensity = SDL_GetWindowPixelDensity(m_Data->m_Window);
			if (pixelDensity <= 0.0f) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to query the pixel density of window '{}', assuming 1.0. SDL_Error: {}", m_Data->m_WindowTitle, SDL_GetError());
				return 1.0f;
			}
			return pixelDensity;
		}

		float Window::GetDisplayScale() const {
			const float displayScale = SDL_GetWindowDisplayScale(m_Data->m_Window);
			if (displayScale <= 0.0f) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to query the display scale of window '{}', assuming 1.0. SDL_Error: {}", m_Data->m_WindowTitle, SDL_GetError());
				return 1.0f;
			}
			return displayScale;
		}

		std::pair<int32_t, int32_t> Window::GetLogicalSizeForScreenMode(const ScreenMode& mode) const {
			const float pixelDensity = GetPixelDensity();

			auto logicalWidth = static_cast<int32_t>(std::lround(static_cast<float>(mode.m_Width) / pixelDensity));
			auto logicalHeight = static_cast<int32_t>(std::lround(static_cast<float>(mode.m_Height) / pixelDensity));

			SDL_Rect usableBounds;
			if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(m_Data->m_Window), &usableBounds)) {
				logicalWidth = std::min(logicalWidth, usableBounds.w);
				logicalHeight = std::min(logicalHeight, usableBounds.h);
			}
			else {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to query the usable bounds of the display window '{}' is on, screen mode '{}' will be applied without clamping it to what the desktop can hold. SDL_Error: {}", m_Data->m_WindowTitle, mode.m_ModeName, SDL_GetError());
			}

			return { std::max(logicalWidth, 1), std::max(logicalHeight, 1) };
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

		// ReSharper disable once CppMemberFunctionMayBeConst
		bool Window::SetRelativeMouseMode(const bool status) {
			if (status && !(SDL_GetWindowFlags(m_Data->m_Window) & SDL_WINDOW_INPUT_FOCUS)) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Refused to enable relative mouse mode, window '{}' does not have input focus. Relative motion will not be enabled.", m_Data->m_WindowTitle);
				return false;
			}

			if (!SDL_SetWindowRelativeMouseMode(m_Data->m_Window, status)) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to {} relative mouse mode. SDL_Error: {}", status ? "enable" : "disable", SDL_GetError());
				return false;
			}

			m_Data->m_MouseDelta = { 0.0f, 0.0f };

			//CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Relative mouse mode requested: {}, reads back as: {}, cursor visible: {}, window focused: {}", status, SDL_GetWindowRelativeMouseMode(m_Data->m_Window), SDL_CursorVisible(), (SDL_GetWindowFlags(m_Data->m_Window) & SDL_WINDOW_INPUT_FOCUS) != 0);

			return true;
		}

		bool Window::IsRelativeMouseMode() const {
			return SDL_GetWindowRelativeMouseMode(m_Data->m_Window);
		}

		glm::vec2 Window::GetMouseDelta() const {
			return m_Data->m_MouseDelta;
		}

		uint64_t Window::GetOldestInputTimestamp() const {
			return m_Data->m_OldestInputTimestamp;
		}

		void* Window::GetNativeWindow() const {
			return m_Data->m_Window;
		}

		const std::vector<ScreenMode>& Window::GetScreenModes() const {
			return m_Data->m_ScreenModes;
		}

		void Window::VerifyDisplayMode() {
			if (m_Data->m_WindowResizable) {
				return;
			}

			int w, h;
			if (!SDL_GetWindowSizeInPixels(m_Data->m_Window, &w, &h)) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to query the pixel size of window '{}', its screen mode can not be verified this frame. SDL_Error: {}", m_Data->m_WindowTitle, SDL_GetError());
				return;
			}

			static constexpr int32_t s_RoundingTolerance{ 1 };

			const ScreenMode& previousMode = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode];
			if (std::abs(previousMode.m_Width - w) <= s_RoundingTolerance && std::abs(previousMode.m_Height - h) <= s_RoundingTolerance) {
				return;
			}

			static std::vector<std::pair<const ScreenMode*, int32_t>> candidates{ 5 };
			candidates.clear();
			for (int32_t i = 0; i < static_cast<int32_t>(m_Data->m_ScreenModes.size()); i++) {
				if (m_Data->m_ScreenModes[i].m_Width == w && m_Data->m_ScreenModes[i].m_Height == h) {
					candidates.emplace_back(&m_Data->m_ScreenModes[i], i);
				}
			}

			if (candidates.empty()) {
				static int32_t lastReportedWidth{ 0 };
				static int32_t lastReportedHeight{ 0 };
				if (lastReportedWidth != w || lastReportedHeight != h) {
					lastReportedWidth = w;
					lastReportedHeight = h;
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Window '{}' is {}x{} pixels, which matches neither the current screen mode '{}' nor any of the {} resolutions this display reports. Keeping screen mode {} and leaving the saved configuration untouched.", m_Data->m_WindowTitle, w, h, previousMode.m_ModeName, m_Data->m_ScreenModes.size(), m_Data->m_CurrentScreenMode);
				}
				return;
			}

			float maxRefreshRate = 0.0f;
			int32_t bestCandidate = candidates.front().second;

			for (const auto& candidate : candidates) {
				if (candidate.first->m_RefreshRate > maxRefreshRate) {
					maxRefreshRate = candidate.first->m_RefreshRate;
					bestCandidate = candidate.second;
				}
			}

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Window '{}' is {}x{} pixels but its screen mode was '{}', adopting screen mode {} '{}' ({} of {} resolutions matched that size, the one with the highest refresh rate was taken).", m_Data->m_WindowTitle, w, h, previousMode.m_ModeName, bestCandidate, m_Data->m_ScreenModes[bestCandidate].m_ModeName, candidates.size(), m_Data->m_ScreenModes.size());

			m_Data->m_CurrentScreenMode = bestCandidate;

			SaveConfiguration();
		}

		void Window::SaveConfiguration() const {
			const std::filesystem::path savePath = FileSystem::PathManager::GetAliasedPath("USER_DATA") / "settings/window.bin";
			std::filesystem::create_directories(savePath.parent_path());
			const ScreenMode& currentMode = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode];
			const WindowSaveData data{ .m_Width = currentMode.m_Width, .m_Height = currentMode.m_Height, .m_RefreshRate = currentMode.m_RefreshRate, .m_PixelDensity = currentMode.m_PixelDensity, .m_WindowMode = m_Data->m_CurrentWindowMode, .m_SDLModeIndex = currentMode.m_SDLModeIndex, .m_ModeIndex = m_Data->m_CurrentScreenMode, .m_Resizable = m_Data->m_WindowResizable };
			FileSystem::BinaryFileManager::SaveAggregateStruct(data, savePath);
		}

		std::expected<void, CoriError<>> Window::SetScreenMode(const uint32_t modeIndex) {
			m_Data->m_CurrentScreenMode = modeIndex;
			//auto result = SetWindowMode(m_Data->m_CurrentWindowMode);
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
			if (mode != WindowMode::RESIZABLE && m_Data->m_WindowResizable) {
				if (SDL_SetWindowResizable(m_Data->m_Window, false)) {
					m_Data->m_WindowResizable = false;
				}
				else {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to clear the resizable flag on window '{}' while leaving 'Resizable' mode, the user may still be able to resize it, so its screen mode will keep being left unverified. SDL_Error: {}", m_Data->m_WindowTitle, SDL_GetError());
				}
			}

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

					const ScreenMode& screenMode = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode];
					const auto [logicalWidth, logicalHeight] = GetLogicalSizeForScreenMode(screenMode);

					success = SDL_SetWindowSize(m_Data->m_Window, logicalWidth, logicalHeight);
					if (!success) {
						return std::unexpected(CoriError(std::format("Failed to set window mode to 'Windowed'. SDL_Error: {}", SDL_GetError())));
					}

					const bool windowMoveSuccess = SDL_SetWindowPosition(m_Data->m_Window, SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID), SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID));
					if (!windowMoveSuccess) {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to set window position to the center of the main screen. (This is expected on Wayland) SDL_Error: {}", SDL_GetError());
					}

					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Window set to 'Windowed' mode. Screen mode: (Width: {}, Height: {}) pixels, requested from the window manager as {}x{} points.", screenMode.m_Width, screenMode.m_Height, logicalWidth, logicalHeight);

					m_Data->m_CurrentWindowMode = mode;

					SaveConfiguration();

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

					SaveConfiguration();

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

					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Window set to 'Exclusive Fullscreen' mode. Screen mode: (Width: {}, Height: {}, Refresh Rate: {}), which SDL describes as {}x{} points at {}x pixel density.", m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Width, m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode].m_Height, sdlMode->refresh_rate, sdlMode->w, sdlMode->h, sdlMode->pixel_density);

					m_Data->m_CurrentWindowMode = mode;

					SaveConfiguration();

					break;
				}
			case WindowMode::RESIZABLE:
				{
					success = SDL_SetWindowFullscreen(m_Data->m_Window, false);
					if (!success) {
						return std::unexpected(CoriError(std::format("Failed to set window mode to 'Resizable'. SDL_Error: {}", SDL_GetError())));
					}

					const bool borderAdded = SDL_SetWindowBordered(m_Data->m_Window, true);
					if (!borderAdded) {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to add border to the window. SDL_Error: {}", SDL_GetError());
					}

					success = SDL_SetWindowResizable(m_Data->m_Window, true);
					if (!success) {
						return std::unexpected(CoriError(std::format("Failed to set window mode to 'Resizable'. SDL_Error: {}", SDL_GetError())));
					}
					m_Data->m_WindowResizable = true;

					const ScreenMode& screenMode = m_Data->m_ScreenModes[m_Data->m_CurrentScreenMode];
					const auto [logicalWidth, logicalHeight] = GetLogicalSizeForScreenMode(screenMode);

					success = SDL_SetWindowSize(m_Data->m_Window, logicalWidth, logicalHeight);
					if (!success) {
						return std::unexpected(CoriError(std::format("Failed to set window mode to 'Resizable'. SDL_Error: {}", SDL_GetError())));
					}

					const bool windowMoveSuccess = SDL_SetWindowPosition(m_Data->m_Window, SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID), SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID));
					if (!windowMoveSuccess) {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Failed to set window position to the center of the main screen. (This is expected on Wayland) SDL_Error: {}", SDL_GetError());
					}

					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Window }, "Window set to 'Resizable' mode. Screen mode: (Width: {}, Height: {}) pixels, requested from the window manager as {}x{} points.", screenMode.m_Width, screenMode.m_Height, logicalWidth, logicalHeight);

					m_Data->m_CurrentWindowMode = mode;

					SaveConfiguration();

					break;
				}
			}

			SDL_SyncWindow(m_Data->m_Window);

			return {};
		}
	}
}
