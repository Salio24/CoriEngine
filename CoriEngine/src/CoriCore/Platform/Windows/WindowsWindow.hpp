#pragma once

#include "../../Window.hpp"
#include <glad/gl.h>

#include "../../EventSystem/AppEvent.hpp"
#include "../../EventSystem/KeyEvent.hpp"
#include "../../EventSystem/MouseEvent.hpp"



namespace Cori {

	class WindowsWindow : public Window {
	public:
		WindowsWindow(const WindowProperties& props);
		virtual ~WindowsWindow();

		void OnUpdate() override;

		inline unsigned int GetWidth() const override { return m_Data.Width; }
		inline unsigned int GetHeight() const override { return m_Data.Height; }
		inline void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }

		void SetVSync(bool enabled) override;
		bool IsVSync() const override;
	private:
		virtual void Init(const WindowProperties& props);
		virtual void Shutdown();
		
		SDL_Window* m_Window{ nullptr };

		SDL_GLContext context{ nullptr };

		struct WindowData {
			std::string Title;
			unsigned int Width, Height;
			bool VSync;
			
			EventCallbackFn EventCallback;
		};

		WindowData m_Data;

	};
}
