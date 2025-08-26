#pragma once 
#include "EventSystem/Event.hpp"
#include "Window.hpp"
#include "Layer.hpp"
#include "LayerStack.hpp"
#include "ImGui/ImGuiLayer.hpp"
#include "WorldSystem/SceneManager.hpp"
#include "Time.hpp"

namespace Cori {
	class Application {
	public:
		explicit Application(const char* windowName);
		virtual ~Application();

		void Run();

		void OnEvent(Event& event);

		[[nodiscard]] static std::expected<void, CoriError<>> PushLayer(Layer* layer);
		[[nodiscard]] static std::expected<void, CoriError<>> PushOverlay(Layer* overlay);
		static void PopLayer(Layer* layer);
		static void PopOverlay(Layer* overlay);

		static void SetBackgroundColor(const glm::vec4& color);

		static Window& GetWindow() { return *s_Instance->m_Window; }

	private:
		void TickrateUpdate(const float timeStep);

		bool OnWindowClose();

		bool m_RenderImGui{ true };

		std::unique_ptr<Window> m_Window;

		ImGuiLayer* m_ImGuiLayer;

		LayerStack m_LayerStack;

		GameTimer m_GameTimer;

		bool m_Running{ true };

		static Application* s_Instance;

		glm::vec4 m_BackgroundColor{ 0.5f, 0.5f, 0.0f, 1.0f };
	};

	Application* CreateApplication();
}