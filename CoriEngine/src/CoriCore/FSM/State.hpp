#pragma once
#include "../ECS/Entity.hpp"

namespace Cori {
	namespace FSM {

		class Machine;

		class State {
		public:
			virtual ~State() = default;

			virtual void OnEnter(Entity owner, Machine* fsm) {}

			virtual void OnUpdate(Entity owner, Machine* fsm, float timestep) {}

			virtual void OnExit(Entity owner, Machine* fsm) {}

			virtual const char* GetDebugName() const {}

			// maybe add an onevent func 
		};

	}
}