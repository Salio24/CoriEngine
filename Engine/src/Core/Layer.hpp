#pragma once
#include <box2cpp/debug_imgui_renderer.h>
#include "Time.hpp"
#include "EventSystem/Event.hpp"
#include "WorldSystem/Scene.hpp"
#include "WorldSystem/SceneHandle.hpp"

namespace Cori {
	namespace Core {
		class Layer {
		public:
			using EventCallbackFn = std::function<void(Event&)>;

			explicit Layer(std::string name);

			virtual ~Layer();

			virtual void OnAttach();
			virtual void OnDetach();
			virtual void OnUpdate([[maybe_unused]] const GameTimer& gameTimer) {}
			virtual void OnTickUpdate([[maybe_unused]] const float timeStep) {}
			virtual void OnImGuiRender([[maybe_unused]] const double deltaTime) { }
			virtual void OnEvent([[maybe_unused]] Event& event) {}

			void SetModal(const bool state) { m_Modal = state; }
			[[nodiscard]] bool IsModal() const { return m_Modal; }

			[[nodiscard]] const std::string& GetName() const { return m_Name; }

			[[nodiscard]] std::expected<void, CoriError<>> BindScene(const std::string& name);
			[[nodiscard]] std::expected<void, CoriError<>> UnbindScene();

			void SceneUpdate(const double deltaTime) {
				ActiveScene.OnUpdate(deltaTime);
			}

			void SceneTickrateUpdate(const float timeStep) {
				ActiveScene.OnTickUpdate(timeStep);
			}

			World::SceneHandle ActiveScene;

			inline static Physics::DebugImguiRenderer m_DebugImGuiRenderer;

		protected:
			bool m_Modal{ false };
			std::string m_Name;
		};
	}
}