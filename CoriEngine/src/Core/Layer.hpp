#pragma once
#include <box2cpp/debug_imgui_renderer.h>
#include "EventSystem/Event.hpp"
#include "Profiling/TimeProfiler.hpp"
#include "WorldSystem/Scene.hpp"
#include "WorldSystem/SceneHandle.hpp"

namespace Cori {

	class Layer {
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		Layer(const std::string& name = "Layer");

		virtual ~Layer();

		virtual void OnAttach();
		virtual void OnDetach();
		virtual void OnUpdate([[maybe_unused]] const double deltaTime, [[maybe_unused]] const double tickAlpha) {}
		virtual void OnTickUpdate([[maybe_unused]] const float timeStep) {}
		virtual void OnImGuiRender([[maybe_unused]] const double deltaTime) { }
		virtual void OnEvent([[maybe_unused]] Event& event) {}

		void SetModal(bool state) { m_Modal = state; }
		inline bool IsModal() const { return m_Modal; }

		inline const std::string& GetName() const { return m_DebugName; }

		void BindScene(const std::string& name);
		void UnbindScene();

		void SceneUpdate(const double deltaTime) {
			ActiveScene.OnUpdate(deltaTime);
		}

		void SceneTickrateUpdate(const float timeStep) {
			ActiveScene.OnTickUpdate(timeStep);
		}

		SceneHandle ActiveScene;

		inline static Physics::DebugImguiRenderer debug_renderer;

	protected:

		bool m_Modal{ false };

		std::string m_DebugName;
	};
}