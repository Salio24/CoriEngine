#pragma once 
#include "EventSystem/Event.hpp"
#include "EventSystem/AppEvent.hpp"
#include "Window.hpp"
#include "Layer.hpp"
#include "LayerStack.hpp"
#include "Input.hpp"
#include "ImGui/ImGuiLayer.hpp"
#include "Renderer/VertexArray.hpp"
#include "Renderer/ShaderProgram.hpp"
#include "AssetManager/AssetManager.hpp"
#include "WorldSystem/SceneManager.hpp"
#include "Time.hpp"

namespace Cori {
	class Application {
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		static void PushLayer(Layer* layer);
		static void PushOverlay(Layer* layer);
		
		static Window& GetWindow() { return *Get().m_Window; }

		static GameTimer& GetGameTimer() { return Get().m_GameTimer; }

		static GraphicsAPIs GetCurrentAPI() { return GetWindow().GetAPI(); }

	protected:
		friend class SceneManager;

		static SceneManager* GetSceneManager() { return &s_Instance->m_SceneManager; }

	private:

		// idk if i even need this Get func
		static Application& Get() { return *s_Instance; }

		void TickrateUpdate(const float timeStep);

		bool OnWindowClose();

		bool m_RenderImGui{ true };

		std::unique_ptr<Window> m_Window;

		ImGuiLayer* m_ImGuiLayer;

		LayerStack m_LayerStack;
		SceneManager m_SceneManager;

		GameTimer m_GameTimer;

		bool m_Running{ true };

		static Application* s_Instance;

	};

	Application* CreateApplication();
}