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

		uint32_t GetWidth() const override { return m_Data.Width; }
		uint32_t GetHeight() const override { return m_Data.Height; }
		void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
		GraphicsAPIs GetAPI() const override { return m_Data.API; }

		void* GetNativeContext() const override { return m_Context->GetNativeContext(); }
		void* GetNativeWindow() const override { return m_Window; }

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
