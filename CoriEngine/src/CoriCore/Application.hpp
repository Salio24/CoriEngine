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
#include "Time.hpp"

namespace Cori {
	class Application {
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);
		
		inline static Window& GetWindow() { return *(*s_Instance).m_Window; }

		inline static GameTimer& GetGameTimer() { return (*s_Instance).m_GameTimer; }

	private: 

		// idk if i even need this Get func
		inline static Application& Get() { return *s_Instance; }

		void TickrateUpdate();

		bool OnWindowClose();

		std::unique_ptr<Window> m_Window;

		ImGuiLayer* m_ImGuiLayer;

		LayerStack m_LayerStack;

		GameTimer m_GameTimer;

		bool m_Running{ true };

		static Application* s_Instance;

	};

	Application* CreateApplication();
}