#include "WindowsWindow.hpp"

namespace Cori {

	Window* Window::Create(const WindowProperties& props) {
		return new WindowsWindow(props);
	}

	WindowsWindow::WindowsWindow(const WindowProperties& props) {
		Init(props);
	}

	WindowsWindow::~WindowsWindow() {
		Shutdown();
	}

	void WindowsWindow::Init(const WindowProperties& props) {
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

		CORI_CORE_INFO('"' + props.Title + '"' + " Window Created");
	}

	void WindowsWindow::Shutdown() {
		SDL_GL_DestroyContext(context);
		SDL_DestroyWindow(m_Window);
	}

	void WindowsWindow::OnUpdate() {
		glClearColor((14.0f / 256.0f), (7.0f / 256.0f), (27.0f / 256.0f), 1.0f);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		SDL_GL_SwapWindow(m_Window);
	}

	void WindowsWindow::SetVSync(bool enabled) {
		if (enabled == false) {
			SDL_GL_SetSwapInterval(0);
		}
		else {
			SDL_GL_SetSwapInterval(1);
		}

		m_Data.VSync = enabled;
	}

	bool WindowsWindow::IsVSync() const {
		return m_Data.VSync;
	}
}