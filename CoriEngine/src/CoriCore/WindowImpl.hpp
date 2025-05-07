#pragma once

#include "Window.hpp"

#include "EventSystem/AppEvent.hpp"
#include "EventSystem/KeyEvent.hpp"
#include "EventSystem/MouseEvent.hpp"
#include "Renderer/RenderingContext.hpp"


namespace Cori {

	class WindowImpl : public Window {
	public:
		WindowImpl(const WindowProperties& props);
		virtual ~WindowImpl();

		void OnUpdate() override;

		inline unsigned int GetWidth() const override { return m_Data.Width; }
		inline unsigned int GetHeight() const override { return m_Data.Height; }
		inline void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
		inline GraphicsAPIs GetAPI() const override { return m_Data.API; }

		inline virtual void* GetNativeContext() const override { return m_Context->GetNativeContext(); }
		inline virtual void* GetNativeWindow() const override { return m_Window; }

		void SetVSync(bool enabled) override;
		bool IsVSync() const override;
	private:
		void Init(const WindowProperties& props);
		void Shutdown();
		
		std::unique_ptr<RenderingContext> m_Context;

		SDL_Window* m_Window{ nullptr };

		struct WindowData {
			std::string Title;
			GraphicsAPIs API;
			unsigned int Width, Height;
			bool VSync;
			
			EventCallbackFn EventCallback;
		};

		WindowData m_Data;
	};
}
