#pragma once

#include "Window.hpp"

#include "EventSystem/AppEvent.hpp"
#include "EventSystem/KeyEvent.hpp"
#include "EventSystem/MouseEvent.hpp"



namespace Cori {

	class WindowImpl : public Window {
	public:
		WindowImpl(const WindowProperties& props);
		virtual ~WindowImpl();

		void OnUpdate() override;

		inline unsigned int GetWidth() const override { return m_Data.Width; }
		inline unsigned int GetHeight() const override { return m_Data.Height; }
		inline void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }

		SDL_Window* GetSDLWindow() { return m_Window; }
		SDL_GLContext GetSDLGLContext() { return context; }

		inline virtual void* GetNativeContex() const override { return context; }
		inline virtual void* GetNativeWindow() const override { return m_Window; }

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
