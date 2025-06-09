#pragma once
#include "EventSystem/Event.hpp"
#include "Profiling/TimeProfiler.hpp"
#include "ECS/Scene.hpp"
#include "ECS/TriggerManager.hpp"

namespace Cori {

	class Layer {
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		Layer(const std::string& name = "Layer");

		virtual ~Layer();

		virtual void OnAttach();
		virtual void OnDetach();
		virtual void OnUpdate([[maybe_unused]] const double deltaTime) {}
		virtual void OnTickUpdate() {}
		virtual void OnImGuiRender([[maybe_unused]] const double deltaTime) { }
		virtual void OnEvent([[maybe_unused]] Event& event) {}

		void SetModal(bool state) { m_Modal = state; }
		inline bool IsModal() const { return m_Modal; }

		inline const std::string& GetName() const { return m_DebugName; }

		void BindScene(const std::string& name);
		void UnbindScene();

		void SceneUpdate(const double deltaTime) {
			if (ActiveScene != nullptr) {
				TriggerManager::Get().OnUpdate(deltaTime, ActiveScene);
				ActiveScene->OnUpdate(deltaTime);
			}
		}

		std::shared_ptr<Scene> ActiveScene{ nullptr };

	protected:

		bool m_Modal{ false };

		std::string m_DebugName;
	};
}