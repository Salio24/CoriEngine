#pragma once
#include "SceneSystem/Entity.hpp"

namespace Cori {
	class StateMachine;

	class State {
	public:
		virtual ~State() = default;

		virtual void OnEnter(Entity& owner, StateMachine* fsm) {}

		// rename to ontick
		virtual void OnUpdate(Entity& owner, StateMachine* fsm, float timestep) {}

		virtual void OnExit(Entity& owner, StateMachine* fsm) {}

		virtual const char* GetDebugName() const { return "Unnamed State"; }

		// maybe add an onevent func 
	};
}