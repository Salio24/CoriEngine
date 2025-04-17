#include "WindowImpl.hpp"
#include <glad/gl.h>
#include <backends/imgui_impl_sdl3.h>

namespace Cori {

	Window* Window::Create(const WindowProperties& props) {
		return new WindowImpl(props);
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

		m_Window = SDL_CreateWindow(m_Data.Title.c_str(), m_Data.Width, m_Data.Height, SDL_WINDOW_OPENGL);
		CORI_CORE_ASSERT_FATAL(!!m_Window, "Window could not be created! SDL_Error: " + std::string(SDL_GetError()));

		SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

		context = SDL_GL_CreateContext(m_Window);
		CORI_CORE_ASSERT_FATAL(!!context, "OpenGL context could not be created! SDL_Error: " + std::string(SDL_GetError()));

		int glad_version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
		CORI_CORE_ASSERT_FATAL(glad_version != 0, "Failed to initialize GLAD");

		CORI_CORE_TRACE("Vendor: " + std::string(reinterpret_cast<const char*>(glGetString(GL_VENDOR))));
		CORI_CORE_TRACE("Renderer: " + std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER))));
		CORI_CORE_TRACE("GL Version: " + std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
		CORI_CORE_TRACE("Shading Language Version: " + std::string(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));

		CORI_CORE_INFO('"' + m_Data.Title + '"' + " Window Created");
	}

	void WindowImpl::Shutdown() {
		SDL_GL_DestroyContext(context);
		SDL_DestroyWindow(m_Window);

		CORI_CORE_INFO('"' + m_Data.Title + '"' + " Window Destroyed");
	}

	void WindowImpl::OnUpdate() {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
		ImGui_ImplSDL3_ProcessEvent(&e);
			switch (e.type) {
			case SDL_EVENT_WINDOW_RESIZED:
				{
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
					KeyPressedEvent keyPressedEvent((CoriKeycode)e.key.scancode, e.key.repeat);
					m_Data.EventCallback(keyPressedEvent);
					break;
				}
			case SDL_EVENT_KEY_UP:
				{
					KeyReleasedEvent keyReleasedEvent((CoriKeycode)e.key.scancode);
					m_Data.EventCallback(keyReleasedEvent);
					break;
				}
			case SDL_EVENT_MOUSE_MOTION:
				{
					MouseMovedEvent mouseMovedEvent((int)e.motion.x, (int)e.motion.y);
					m_Data.EventCallback(mouseMovedEvent);
					break;
				}
			case SDL_EVENT_MOUSE_WHEEL:
				{
					MouseScrolledEvent mouseScrolledEvent((short)e.wheel.x, (short)e.wheel.y);
					m_Data.EventCallback(mouseScrolledEvent);
					break;
				}
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				{
					MouseButtonPressedEvent mouseButtonPressedEvent((CoriMouseCode)e.button.button);
					m_Data.EventCallback(mouseButtonPressedEvent);
					break;
				}
			case SDL_EVENT_MOUSE_BUTTON_UP:
				{
					MouseButtonReleasedEvent mouseButtonReleasedEvent((CoriMouseCode)e.button.button);
					m_Data.EventCallback(mouseButtonReleasedEvent);
					break;
				}
			}
		}

		
		SDL_GL_SwapWindow(m_Window);
	}

	void WindowImpl::SetVSync(bool enabled) {
		if (enabled == false) {
			SDL_GL_SetSwapInterval(0);
		}
		else {
			SDL_GL_SetSwapInterval(1);
		}

		CORI_CORE_INFO("Set VSync to: " + enabled);

		m_Data.VSync = enabled;
	}

	bool WindowImpl::IsVSync() const {
		return m_Data.VSync;
	}
}