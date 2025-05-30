// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "WindowImpl.hpp"
#include <backends/imgui_impl_sdl3.h>
#include <SDL3_image/SDL_image.h>

namespace Cori {

	std::unique_ptr<Window> Window::Create(const WindowProperties& props) {
		return std::make_unique<WindowImpl>(props);
	}

	WindowImpl::WindowImpl(const WindowProperties& props) {
		Init(props);
	}

	WindowImpl::~WindowImpl() {
		Shutdown();
	}

	void WindowImpl::Init(const WindowProperties& props) {
		m_Data.Title = props.Title;
		m_Data.Width = props.Width;
		m_Data.Height = props.Height;
		m_Data.API = props.API;

		m_Context = RenderingContext::Create(m_Data.API);

		switch (m_Data.API) {
		case GraphicsAPIs::None:
			CORI_CORE_ASSERT_FATAL(false, "No graphics API selected");
			break;
		case GraphicsAPIs::OpenGL:
			m_Window = SDL_CreateWindow(m_Data.Title.c_str(), m_Data.Width, m_Data.Height, SDL_WINDOW_OPENGL);
			CORI_CORE_ASSERT_FATAL(!!m_Window, "Window could not be created! SDL_Error: " + std::string(SDL_GetError()));
			break;
		case GraphicsAPIs::Vulkan:
			CORI_CORE_ASSERT_FATAL(false ,"Vulkan is not supported yet");
			break;
		}

		std::string logoPath = "assets/engine/textures/ui/logo256.png";

		SDL_Surface* logo = IMG_Load(logoPath.c_str());

		if (!CORI_CORE_ASSERT_WARN(logo, "Failed to load App Logo: {0}", SDL_GetError())) {
			SDL_SetWindowIcon(m_Window, logo);
		}

		SDL_DestroySurface(logo);

		SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

		m_Context->Init(m_Window);

		CORI_CORE_INFO('"' + m_Data.Title + '"' + " Window Created");
	}

	void WindowImpl::Shutdown() {
		SDL_DestroyWindow(m_Window);

		CORI_CORE_INFO('"' + m_Data.Title + '"' + " Window Destroyed");
	}

	void WindowImpl::OnUpdate() {
		CORI_PROFILE_FUNCTION();
		SDL_Event e;
		while (SDL_PollEvent(&e)) {

			// FIX: event in imgui affect main layer
			if (e.window.windowID != SDL_GetWindowID(m_Window)) {
				//CORI_CORE_DEBUG("event");
			}

			if (!ImGui_ImplSDL3_ProcessEvent(&e)) {
				//CORI_CORE_ERROR("Error processing sdl event in imgui");
			}


			switch (e.type) {
			case SDL_EVENT_WINDOW_RESIZED:
				{
					if (e.window.windowID != SDL_GetWindowID(m_Window)) {
						break;
					}
					// FIX: handling resize event here is just wrong 
					m_Data.Width = e.window.data1;
					m_Data.Height = e.window.data2;

					WindowResizeEvent resizeEvent(m_Data.Width, m_Data.Height);
					m_Data.EventCallback(resizeEvent);
					break;
				}
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				{
					WindowCloseEvent closeEvent;
					m_Data.EventCallback(closeEvent);
					break;
				}
			case SDL_EVENT_KEY_DOWN:
				{
					KeyPressedEvent keyPressedEvent(static_cast<CoriKeycode>(e.key.scancode), e.key.repeat);
					m_Data.EventCallback(keyPressedEvent);
					break;
				}
			case SDL_EVENT_KEY_UP:
				{
					KeyReleasedEvent keyReleasedEvent(static_cast<CoriKeycode>(e.key.scancode));
					m_Data.EventCallback(keyReleasedEvent);
					break;
				}
			case SDL_EVENT_MOUSE_MOTION:
				{
					MouseMovedEvent mouseMovedEvent(static_cast<int>(e.motion.x), static_cast<int>(e.motion.y));
					m_Data.EventCallback(mouseMovedEvent);
					break;
				}
			case SDL_EVENT_MOUSE_WHEEL:
				{
					MouseScrolledEvent mouseScrolledEvent(static_cast<short>(e.wheel.x), static_cast<short>(e.wheel.y));
					m_Data.EventCallback(mouseScrolledEvent);
					break;
				}
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				{
					MouseButtonPressedEvent mouseButtonPressedEvent(static_cast<CoriMouseCode>(e.button.button));
					m_Data.EventCallback(mouseButtonPressedEvent);
					break;
				}
			case SDL_EVENT_MOUSE_BUTTON_UP:
				{
					MouseButtonReleasedEvent mouseButtonReleasedEvent(static_cast<CoriMouseCode>(e.button.button));
					m_Data.EventCallback(mouseButtonReleasedEvent);
					break;
				}
			}
		}

		m_Context->SwapBuffers();
	}

	void WindowImpl::SetVSync(bool enabled) {
		if (enabled == false) {
			SDL_GL_SetSwapInterval(0);
		}
		else {
			SDL_GL_SetSwapInterval(1);
		}

		CORI_CORE_INFO("VSync is now set to: {0}", enabled);

		m_Data.VSync = enabled;
	}

	bool WindowImpl::IsVSync() const {
		return m_Data.VSync;
	}
}