#pragma once
#include "SceneSystem/Entity.hpp"

namespace Cori {
	namespace Physics {
		class TriggerBehaviour {
		public:
			virtual ~TriggerBehaviour() = default;

			virtual void OnEnter([[maybe_unused]] Entity& visitor) {}

			virtual void OnTickUpdate([[maybe_unused]] Entity& visitor, [[maybe_unused]] float timestep) {}

			virtual void OnExit([[maybe_unused]] Entity& visitor) {}

			virtual const char* GetDebugName() const { return "Unnamed Trigger"; }

			// maybe add an onevent func 
		};
	}
}