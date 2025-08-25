#pragma once
#include "WorldSystem/Entity.hpp"

namespace Cori {
	namespace Components {
		namespace Entity {
			class StateMachine;
		}
	}

	class State {
	public:
		virtual ~State() = default;

		virtual void OnEnter([[maybe_unused]] Entity& owner, [[maybe_unused]] Components::Entity::StateMachine* fsm) {}

		virtual void OnTickUpdate([[maybe_unused]] Entity& owner, [[maybe_unused]] Components::Entity::StateMachine* fsm, [[maybe_unused]] float timestep) {}

		virtual void OnExit([[maybe_unused]] Entity& owner, [[maybe_unused]] Components::Entity::StateMachine* fsm) {}

		virtual const char* GetDebugName() const { return "Unnamed State"; }

		// maybe add an onevent func 
	};
}