#pragma once
#include "EventSystem/Event.hpp"
#include "Graphics/GraphicsAPIs.hpp"
#include "Profiling/Trackable.hpp"
#include "Core/SelfFactory.hpp"

namespace Cori {
	struct ScreenMode {
		int32_t m_Width;
		int32_t m_Height;
		float m_RefreshRate;
		std::string m_ModeName;

	protected:
		friend class Window;
		ScreenMode() = default;

		ScreenMode(const int32_t width, const int32_t height, const float refreshRate, const uint32_t modeIndex)
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
		static bool PreCreateHook([[maybe_unused]] const std::string& title, [[maybe_unused]] const bool vsync = false) { return true; }
		~Window();

		[[nodiscard]] int32_t GetWidth() const;
		[[nodiscard]] int32_t GetHeight() const;

		[[nodiscard]] static GraphicsAPIs GetCurrentAPI() { return s_API; } // NOLINT

		void SetVSync(const bool status);
		[[nodiscard]] bool IsVSync() const;

		[[nodiscard]] std::vector<ScreenMode> GetScreenModes() const;
		[[nodiscard]] std::expected<void, CoriError<>> SetScreenMode(const ScreenMode& mode);

		[[nodiscard]] WindowMode GetWindowMode() const;
		[[nodiscard]] std::expected<void, CoriError<>> SetWindowMode(WindowMode mode);

	protected:
		explicit Window(const std::string& title, const bool vsync = false);

		friend class Application;
		void OnUpdate();
		void SetEventCallback(const EventCallbackFn& callback);

		friend class ImGuiLayer;
		friend class OpenGLContext;
		void* GetNativeContext() const;
		void* GetNativeWindow() const;

	private:
		inline static auto s_API = GraphicsAPIs::OpenGL;
		struct Data;
		Data* m_Data{nullptr};
	};
}
