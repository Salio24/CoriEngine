#pragma once
#include "EventSystem/Event.hpp"
#include "Renderer/GraphicsAPIs.hpp"
#include "Profiling/Trackable.hpp"
#include "Core/SelfFactory.hpp"

namespace Cori {
	struct ScreenMode {
		int m_Width;
		int m_Height;
		float m_RefreshRate;
		std::string m_ModeName;

	protected:
		friend class Window;
		ScreenMode() = default;

		ScreenMode(int width, int height, float refreshRate, uint32_t modeIndex)
			: m_Width(width), m_Height(height), m_RefreshRate(refreshRate), m_ModeIndex(modeIndex) {
			m_ModeName = std::to_string(m_Width) + "x" + std::to_string(m_Height) + " " + std::to_string(m_RefreshRate) + " Hz";
		}

		uint32_t m_ModeIndex{ 1000 }; //impossible initial number
	};

	enum class WindowMode {
		WINDOWED = 0,
		BORDERLESS_WINDOWED = 1,
		EXCLUSIVE_FULLSCREEN = 2
	};

	class Window : public Profiling::Trackable<Window>, public UniqueSelfFactory<Window> {
	public:
		static bool PreCreateHook([[maybe_unused]] std::string title, [[maybe_unused]] bool vsync = false) { return true; }
		Window(const std::string& title, bool vsync = false);
		~Window();

		void OnUpdate();

		int GetWidth() const;
		int GetHeight() const;

		void SetEventCallback(const EventCallbackFn& callback);

		static GraphicsAPIs GetAPI() { return s_API; }

		void SetVSync(bool enabled);
		bool IsVSync() const;

		void* GetNativeContext() const;
		void* GetNativeWindow() const;

		std::vector<ScreenMode> GetScreenModes() const;
		bool SetScreenMode(const ScreenMode& mode);

		WindowMode GetWindowMode() const;
		bool SetWindowMode(WindowMode mode);

	private:
		inline static GraphicsAPIs s_API = GraphicsAPIs::OpenGL;
		struct Data;
		Data* m_Data{nullptr};
	};
}
