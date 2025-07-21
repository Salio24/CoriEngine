#pragma once
#include "SceneSystem/Entity.hpp"

namespace Cori {
	class StateMachine;

	class State {
	public:
		virtual ~State() = default;

		virtual void OnEnter([[maybe_unused]] Entity& owner, [[maybe_unused]] StateMachine* fsm) {}

		// rename to ontick
		virtual void OnUpdate([[maybe_unused]] Entity& owner, [[maybe_unused]] StateMachine* fsm, [[maybe_unused]] float timestep) {}

		virtual void OnExit([[maybe_unused]] Entity& owner, [[maybe_unused]] StateMachine* fsm) {}

		virtual const char* GetDebugName() const { return "Unnamed State"; }

		// maybe add an onevent func 
	};
}