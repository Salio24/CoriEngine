#pragma once
#include "SceneSystem/Entity.hpp"

namespace Cori {
	class StateMachine;

	class State {
	public:
		virtual ~State() = default;

		virtual void OnEnter(Entity owner, StateMachine* fsm) {}

		virtual void OnUpdate(Entity owner, StateMachine* fsm, float timestep) {}

		virtual void OnExit(Entity owner, StateMachine* fsm) {}

		virtual const char* GetDebugName() const {}

		// maybe add an onevent func 
	};
}