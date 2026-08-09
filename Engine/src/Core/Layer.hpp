#pragma once
#include <box2cpp/debug_imgui_renderer.h>
#include "Time.hpp"
#include "EventSystem/Event.hpp"
#include "WorldSystem/Scene.hpp"
#include "WorldSystem/SceneHandle.hpp"

namespace Cori {
	namespace Core {
		/**
		 * @brief An abstract class that is ment to be used as a template for defining layers.
		 */
		class Layer {
		public:
			explicit Layer(std::string name);

			virtual ~Layer();

			/**
			 * @brief Method that runs when the layer gets attached to the LayerStack.
			 */
			virtual void OnAttach();

			/**
			 * @brief Method that runs when the layer gets detached to the LayerStack.
			 */
			virtual void OnDetach();

			/**
			 * @brief Run every frame.
			 * @param gameTimer Global timer.
			 */
			virtual void OnUpdate([[maybe_unused]] GameTimer& gameTimer) {}

			/**
			 * @brief Run every tick.
			 * @param gameTimer Global timer.
			 */
			virtual void OnTickUpdate([[maybe_unused]] GameTimer& gameTimer) {}


			/**
			 * @brief Run every frame, this is the method where you do all the ImGui stuff.
			 * @param gameTimer Global timer.
			 */
			virtual void OnImGuiRender([[maybe_unused]] GameTimer& gameTimer) {}

			/**
			 * @brief This is the method to handle all the event at.
			 * @param event Event to handle or skip.
			 */
			virtual void OnEvent([[maybe_unused]] Event& event) {}


			/**
			 * @brief Change the Layer modal state.
			 * @param state Desired state.
			 * @detail When Layer is in a modal state all events that are passed to it will be considered handled and will not be passed further down the LayerStack.
			 * \n Also when a layer is modal any layer below it will not be updated, both tick update and frame update, it effectively stops any logic execution in the layers below it.
			 */
			void SetModal(const bool state) { m_Modal = state; }

			/**
			 * @brief Checks if the Layer is modal.
			 */
			[[nodiscard]] bool IsModal() const { return m_Modal; }

			/**
			 * @brief Returns the name of the Layer.
			 * @return String view, so read only.
			 */
			[[nodiscard]] std::string_view GetName() const { return m_Name; }

			inline static Physics::DebugImguiRenderer m_DebugImGuiRenderer;

		private:
			friend class Application;

			bool m_Modal{ false };
			std::string m_Name;
		};
	}
}