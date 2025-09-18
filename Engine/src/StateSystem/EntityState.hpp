#pragma once
#include "WorldSystem/Entity.hpp"

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				class StateMachine;
			}
		}

		/**
		 * @brief An abstract class designed to be used with StateMachine component.
		 */
		class EntityState {
		public:
			virtual ~EntityState() = default;

			/**
			 * @brief Fired when then Entity enter the state.
			 * @param owner Owner of the StateMachine component.
			 * @param lastStateType The type_index of the last state, if there is no last state it is the type_index of the current state.
			 */
			virtual void OnEnter([[maybe_unused]] Entity& owner, [[maybe_unused]] const std::type_index& lastStateType) {}

			/**
			 * @brief Fired every tick when the Entity is in the state. Expect for the very first tick the Entity entered the state.
			 * @param owner Owner of the StateMachine component.
			 * @param timestep Do I need to explain this?
			 */
			virtual void OnTickUpdate([[maybe_unused]] Entity& owner, [[maybe_unused]] float timestep) {}

			/**
			 * @brief Fired when the Entity exist the state.
			 * @param owner Owner of the StateMachine component.
			 * @param nextStateType The type_index of the next state.
			 */
			virtual void OnExit([[maybe_unused]] Entity& owner, [[maybe_unused]] const std::type_index& nextStateType) {}

			[[nodiscard]] virtual const char* GetDebugName() const { return "Unnamed State"; }

			// maybe add an onevent func
		};
	}
}