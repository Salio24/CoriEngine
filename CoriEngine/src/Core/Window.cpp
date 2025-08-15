#include "Window.hpp"
#include "Renderer/RenderingContext.hpp"
#include <backends/imgui_impl_sdl3.h>
#include <SDL3_image/SDL_image.h>

#include "EventSystem/AppEvent.hpp"
#include "EventSystem/KeyEvent.hpp"
#include "EventSystem/MouseEvent.hpp"

namespace Cori {
	struct Window::Data {
		ScreenMode m_CurrentScreenMode;
		WindowMode m_CurrentWindowMode = WindowMode::EXCLUSIVE_FULLSCREEN;
		std::string m_WindowTitle;

		int m_DisplayModeCount;
		SDL_DisplayMode** m_SDLModes;
		SDL_DisplayID m_PrimaryDisplayID{};

		SDL_Window* m_Window{nullptr};
		std::unique_ptr<RenderingContext> m_Context;
		bool m_VSync{false};

		EventCallbackFn m_EventCallback;
	};

	Window::Window(const std::string& title, bool vsync) {
		m_Data = new Data();
		m_Data->m_WindowTitle = title;
		m_Data->m_VSync = vsync;

		m_Data->m_Context = RenderingContext::Create(s_API);

		const SDL_DisplayID primaryDisplayID = SDL_GetPrimaryDisplay();
		if (primaryDisplayID == 0) {
			// error
		}
		else {
			m_Data->m_PrimaryDisplayID = primaryDisplayID;
		}

		ScreenMode mode;

		m_Data->m_SDLModes = SDL_GetFullscreenDisplayModes(m_Data->m_PrimaryDisplayID, &m_Data->m_DisplayModeCount);
		if (!m_Data->m_SDLModes) {
			// error
			return;
		}

		mode.m_Width = m_Data->m_SDLModes[0]->w;
		mode.m_Height = m_Data->m_SDLModes[0]->h;
		mode.m_RefreshRate = m_Data->m_SDLModes[0]->refresh_rate;
		mode.m_ModeIndex = 0;

		m_Data->m_CurrentScreenMode = mode;

		switch (s_API) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			break;
		case GraphicsAPIs::OpenGL:
			m_Data->m_Window = SDL_CreateWindow(m_Data->m_WindowTitle.c_str(), m_Data->m_CurrentScreenMode.m_Width, m_Data->m_CurrentScreenMode.m_Height, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
			CORI_CORE_ASSERT_FATAL(m_Data->m_Window, "Window could not be created! SDL_Error: {}", std::string(SDL_GetError()));
			break;
		case GraphicsAPIs::Vulkan:
			CORI_CORE_ASSERT_FATAL(false, "Vulkan is not supported yet");
			break;
		}

		std::string logoPath = "assets/engine/textures/ui/logo256.png";

		SDL_Surface* logo = IMG_Load(logoPath.c_str());

		if (!CORI_CORE_ASSERT_WARN(logo, "Failed to load App Logo: {0}", SDL_GetError())) {
			SDL_SetWindowIcon(m_Data->m_Window, logo);
		}

		SDL_DestroySurface(logo);

		SDL_SetWindowPosition(m_Data->m_Window, SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID), SDL_WINDOWPOS_CENTERED_DISPLAY(m_Data->m_PrimaryDisplayID));

		m_Data->m_Context->Init(m_Data->m_Window);

		CORI_CORE_INFO(" '{}'' Window Created", m_Data->m_WindowTitle);
	}

	Window::~Window() {
		SDL_DestroyWindow(m_Data->m_Window);
		SDL_free(m_Data->m_SDLModes);
		delete m_Data;
		CORI_CORE_INFO(" '{}'' Window Destroyed", m_Data->m_WindowTitle);
	}

	void Window::OnUpdate() {
		CORI_PROFILE_FUNCTION();
		SDL_Event e;

		while (SDL_PollEvent(&e)) {
			// FIX: event in imgui affect main layer
			if (e.window.windowID != SDL_GetWindowID(m_Data->m_Window)) {
				//CORI_CORE_DEBUG("event");
			}

			if (!ImGui_ImplSDL3_ProcessEvent(&e)) {
				//CORI_CORE_ERROR("Error processing sdl event in imgui");
			}


			switch (e.type) {
			case SDL_EVENT_WINDOW_RESIZED:
				{
					if (e.window.windowID != SDL_GetWindowID(m_Data->m_Window)) {
						break;
					}

					WindowResizeEvent resizeEvent(m_Data->m_CurrentScreenMode.m_Width, m_Data->m_CurrentScreenMode.m_Height);
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
					MouseMovedEvent mouseMovedEvent(static_cast<int>(e.motion.x), static_cast<int>(e.motion.y));
					m_Data->m_EventCallback(mouseMovedEvent);
					break;
				}
			case SDL_EVENT_MOUSE_WHEEL:
				{
					MouseScrolledEvent mouseScrolledEvent(static_cast<short>(e.wheel.x), static_cast<short>(e.wheel.y));
					m_Data->m_EventCallback(mouseScrolledEvent);
					break;
				}
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				{
					MouseButtonPressedEvent mouseButtonPressedEvent(static_cast<CoriMouseCode>(e.button.button));
					m_Data->m_EventCallback(mouseButtonPressedEvent);
					break;
				}
			case SDL_EVENT_MOUSE_BUTTON_UP:
				{
					MouseButtonReleasedEvent mouseButtonReleasedEvent(static_cast<CoriMouseCode>(e.button.button));
					m_Data->m_EventCallback(mouseButtonReleasedEvent);
					break;
				}
			}
		}

		m_Data->m_Context->SwapBuffers();
	}

	int Window::GetWidth() const {
		return m_Data->m_CurrentScreenMode.m_Width;
	}

	int Window::GetHeight() const {
		return m_Data->m_CurrentScreenMode.m_Height;
	}

	void Window::SetEventCallback(const EventCallbackFn& callback) {
		m_Data->m_EventCallback = callback;
	}

	void Window::SetVSync(bool enabled) {
		if (enabled == false) {
			SDL_GL_SetSwapInterval(0);
		}
		else {
			SDL_GL_SetSwapInterval(1);
		}

		CORI_CORE_INFO("VSync is now set to: {0}", enabled);

		m_Data->m_VSync = enabled;
	}

	bool Window::IsVSync() const {
		return m_Data->m_VSync;
	}


	void* Window::GetNativeContext() const {
		return m_Data->m_Context->GetNativeContext();
	}

	void* Window::GetNativeWindow() const {
		return m_Data->m_Window;
	}

	std::vector<ScreenMode> Window::GetScreenModes() const {
		std::vector<ScreenMode> screenModes;
		for (int i = 0; i < m_Data->m_DisplayModeCount; i++) {
			ScreenMode mode(m_Data->m_SDLModes[i]->w, m_Data->m_SDLModes[i]->h, m_Data->m_SDLModes[i]->refresh_rate, i);
			screenModes.emplace_back(mode);
		}
		return screenModes;
	}

	bool Window::SetScreenMode(const ScreenMode& mode) {
		m_Data->m_CurrentScreenMode = mode;
		bool success = SetWindowMode(m_Data->m_CurrentWindowMode);
		return success;
	}

	WindowMode Window::GetWindowMode() const {
		return m_Data->m_CurrentWindowMode;
	}

	bool Window::SetWindowMode(WindowMode mode) {
		bool success = true;
		switch (mode) {
		case WindowMode::WINDOWED:
			{
				success = SDL_SetWindowFullscreen(m_Data->m_Window, false);
				if (!success) {
					// error
					return false;
				}

				const SDL_DisplayMode* desktopMode = SDL_GetCurrentDisplayMode(m_Data->m_PrimaryDisplayID);

				if (m_Data->m_CurrentScreenMode.m_Width <= desktopMode->w && m_Data->m_CurrentScreenMode.m_Height <= desktopMode->h) {
					success = SDL_SetWindowSize(m_Data->m_Window, m_Data->m_CurrentScreenMode.m_Width, m_Data->m_CurrentScreenMode.m_Height);
					if (!success) {
						// error
						return false;
					}
				}
				else {
					success = SDL_SetWindowSize(m_Data->m_Window, desktopMode->w, desktopMode->h);
					if (!success) {
						// error
						return false;
					}
				}


				m_Data->m_CurrentWindowMode = mode;
				break;
			}
		case WindowMode::BORDERLESS_WINDOWED:
			{
				success = SDL_SetWindowFullscreen(m_Data->m_Window, true);
				if (!success) {
					// error
					return false;
				}
				success = SDL_SetWindowFullscreenMode(m_Data->m_Window, NULL);
				if (!success) {
					// error
					return false;
				}
				m_Data->m_CurrentWindowMode = mode;
				break;
			}
		case WindowMode::EXCLUSIVE_FULLSCREEN:
			{
				SDL_DisplayMode* sdlMode = m_Data->m_SDLModes[m_Data->m_CurrentScreenMode.m_ModeIndex];

				if (!sdlMode) {
					// error
					return false;
				}
				success = SDL_SetWindowFullscreen(m_Data->m_Window, true);
				if (!success) {
					// error
					return false;
				}
				success = SDL_SetWindowFullscreenMode(m_Data->m_Window, sdlMode);
				if (!success) {
					// error
					return false;
				}
				m_Data->m_CurrentWindowMode = mode;
				break;
			}
		}
		if (success) {
			SDL_SyncWindow(m_Data->m_Window);
		}

		return success;
	}
}
